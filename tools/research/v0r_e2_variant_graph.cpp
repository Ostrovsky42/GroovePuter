#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_canonical_diff.h"
#include "src/generation/rhythm/rhythm_realizer.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint32_t kProjectSeed = 0xE2A09901u;
constexpr uint16_t kPhraseOrdinal = 7u;
constexpr uint32_t kNodeCeiling = 1000000u;
constexpr uint64_t kTransitionCeiling = 8000000ull;

struct GraphSpec {
  const char* reportFamily;
  const char* enumName;
  ReferenceVocabulary::Archetype key;
};

constexpr GraphSpec kSpecs[] = {
    {"StraightFour", "StraightDrive",
     ReferenceVocabulary::Archetype::StraightDrive},
    {"OffbeatPulse", "OffbeatOpenHat",
     ReferenceVocabulary::Archetype::OffbeatOpenHat},
    {"Breakbeat", "SparseFastBreak",
     ReferenceVocabulary::Archetype::SparseFastBreak},
    {"HalfTime", "HalftimeSwitch",
     ReferenceVocabulary::Archetype::HalftimeSwitch},
    {"Sparse", "HypnoticSparse",
     ReferenceVocabulary::Archetype::HypnoticSparse},
    {"Rolling", "RollingAcid",
     ReferenceVocabulary::Archetype::RollingAcid},
};

constexpr RealizationLevel kLevels[] = {
    RealizationLevel::P1Canonical,
    RealizationLevel::P2Variation,
    RealizationLevel::P3Transformation,
};

const char* levelName(RealizationLevel level) {
  switch (level) {
    case RealizationLevel::P1Canonical: return "P1";
    case RealizationLevel::P2Variation: return "P2";
    case RealizationLevel::P3Transformation: return "P3";
    case RealizationLevel::Count: break;
  }
  return "INVALID";
}

const char* opName(RhythmMutationOp op) {
  switch (op) {
    case RhythmMutationOp::KEEP: return "KEEP";
    case RhythmMutationOp::ADD: return "ADD";
    case RhythmMutationOp::DROP: return "DROP";
    case RhythmMutationOp::DISPLACE: return "DISPLACE";
    case RhythmMutationOp::ACCENT: return "ACCENT";
    case RhythmMutationOp::GHOST: return "GHOST";
    case RhythmMutationOp::Count: break;
  }
  return "INVALID";
}

uint8_t popcount16(StepMask value) {
  uint8_t count = 0;
  while (value != 0) {
    value = static_cast<StepMask>(value & static_cast<StepMask>(value - 1u));
    ++count;
  }
  return count;
}

const LaneGrammar* laneForRole(const RhythmArchetype& archetype,
                               RhythmRole role) {
  for (uint8_t i = 0; i < archetype.laneCount; ++i) {
    if (archetype.lanes[i].role == role) return &archetype.lanes[i];
  }
  return nullptr;
}

StepMask allOnsets(const RoleRhythmPlan& role) {
  return static_cast<StepMask>(
      role.structural | role.secondary | role.ghosts);
}

// Research-only representation adapter for the two operations currently
// emitted by the selected production ReferenceVocabulary policy. The formulas
// exactly mirror rhythm_realizer_evolution.cpp::recomputeRoleGates().
void recomputeRoleGatesForResearch(const LaneGrammar& lane,
                                   RoleRhythmPlan& role) {
  const StepMask onsets = allOnsets(role);
  const StepMask explicitGateSites = static_cast<StepMask>(
      lane.shortGate | lane.heldGate | lane.tieGate);
  role.heldGate = static_cast<StepMask>(onsets & lane.heldGate);
  role.tieGate = static_cast<StepMask>(onsets & lane.tieGate);
  role.shortGate = static_cast<StepMask>(
      (onsets & lane.shortGate) |
      (role.ghosts & static_cast<StepMask>(~explicitGateSites)));
}

bool materializeCurrentProductionProposal(
    const RhythmArchetype& archetype,
    const RhythmPhrasePlan& current,
    RealizationLevel level,
    const RhythmMutationDelta& delta,
    RhythmPhrasePlan& candidate) {
  if (!rhythmMutationDeltaShapeValid(delta) ||
      delta.operation == RhythmMutationOp::KEEP ||
      !rhythmMutationRoleValid(delta.role)) {
    return false;
  }
  const LaneGrammar* lane = laneForRole(archetype, delta.role);
  if (!lane) return false;

  candidate = current;
  candidate.level = level;
  candidate.trajectoryId = kNoTrajectoryId;
  candidate.intent = TransformationIntent::Auto;
  candidate.bars[0].function = BarFunction::Statement;

  RoleRhythmPlan& role =
      candidate.bars[0].roles[static_cast<uint8_t>(delta.role)];
  const StepMask targetBit = rhythmMutationStepValid(delta.targetStep)
                                 ? stepBit(delta.targetStep)
                                 : 0;

  switch (delta.operation) {
    case RhythmMutationOp::ADD:
      if (!targetBit || (allOnsets(role) & targetBit)) return false;
      role.secondary = static_cast<StepMask>(role.secondary | targetBit);
      recomputeRoleGatesForResearch(*lane, role);
      return true;
    case RhythmMutationOp::GHOST:
      if (!targetBit || (allOnsets(role) & targetBit)) return false;
      role.ghosts = static_cast<StepMask>(role.ghosts | targetBit);
      recomputeRoleGatesForResearch(*lane, role);
      return true;
    case RhythmMutationOp::DROP:
    case RhythmMutationOp::DISPLACE:
    case RhythmMutationOp::ACCENT:
      // No selected production ReferenceVocabulary path currently emits these.
      // Fail closed rather than creating research-only musical semantics.
      return false;
    case RhythmMutationOp::KEEP:
    case RhythmMutationOp::Count:
      return false;
  }
  return false;
}

std::string hex16(StepMask value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(4)
      << static_cast<unsigned>(value);
  return out.str();
}

// Stable field-wise node identity. Never serialize struct bytes: padding is
// deliberately excluded. Every plan field that E2b, realizer validation, or
// future E2a proposal production can observe is represented.
std::string nodeKey(const RhythmPhrasePlan& plan) {
  std::ostringstream out;
  out << "bc=" << static_cast<unsigned>(plan.barCount)
      << "|tr=" << static_cast<unsigned>(plan.trajectoryId)
      << "|lv=" << static_cast<unsigned>(plan.level)
      << "|in=" << static_cast<unsigned>(plan.intent);
  for (uint8_t bar = 0; bar < plan.barCount; ++bar) {
    out << "|b" << static_cast<unsigned>(bar)
        << "f=" << static_cast<unsigned>(plan.bars[bar].function);
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      const RoleRhythmPlan& value = plan.bars[bar].roles[role];
      out << "|r" << static_cast<unsigned>(role)
          << ":s=" << hex16(value.structural)
          << ",q=" << hex16(value.secondary)
          << ",g=" << hex16(value.ghosts)
          << ",sh=" << hex16(value.shortGate)
          << ",he=" << hex16(value.heldGate)
          << ",ti=" << hex16(value.tieGate)
          << ",a=" << hex16(value.accents);
    }
  }
  return out.str();
}

RhythmPhrasePlan canonicalPlanFor(const RhythmArchetype& archetype,
                                  const RhythmCatalogView& catalog) {
  RhythmRealizationRequest request{};
  request.catalog = &catalog;
  request.archetypeId = archetype.id;
  request.phraseBars = 1;
  request.level = RealizationLevel::P1Canonical;
  request.generation.projectSeed = kProjectSeed;
  request.generation.phraseOrdinal = kPhraseOrdinal;
  const RhythmRealizationResult realized = realizeRhythmPhrase(request);
  if (realized.status != RealizationStatus::Ok &&
      realized.status != RealizationStatus::ValidButSparse) {
    std::cerr << "canonical realization failed for archetype "
              << archetype.id << "\n";
    std::exit(20);
  }
  return realized.plan;
}

RhythmPhrasePlan normalizedRoot(const RhythmPhrasePlan& canonical,
                                RealizationLevel level) {
  RhythmPhrasePlan root = canonical;
  root.level = level;
  root.trajectoryId = kNoTrajectoryId;
  root.intent = TransformationIntent::Auto;
  root.bars[0].function = BarFunction::Statement;
  return root;
}

struct CompactNode {
  RhythmBarPlan bar{};
  uint32_t depth = 0;
  uint16_t layer = 0;
  std::string key{};
};

RhythmPhrasePlan expandNode(const RhythmPhrasePlan& canonical,
                            RealizationLevel level,
                            const CompactNode& node) {
  RhythmPhrasePlan plan = normalizedRoot(canonical, level);
  plan.bars[0] = node.bar;
  return plan;
}

struct NodeStats {
  uint32_t raw[static_cast<uint8_t>(RhythmMutationOp::Count)]{};
  uint32_t materializationFailure = 0;
  uint32_t materializable = 0;
  uint32_t structuralRejected = 0;
  uint32_t budgetRejected = 0;
  uint32_t legalRecords = 0;
  uint32_t duplicateTarget = 0;
};

void density(const RhythmPhrasePlan& plan,
             uint16_t& totalOccupied,
             uint16_t& ghostCount,
             uint16_t& accentCount,
             uint8_t roleOccupied[kRhythmRoleCount]) {
  totalOccupied = 0;
  ghostCount = 0;
  accentCount = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    const RoleRhythmPlan& value = plan.bars[0].roles[role];
    roleOccupied[role] = popcount16(allOnsets(value));
    totalOccupied = static_cast<uint16_t>(
        totalOccupied + roleOccupied[role]);
    ghostCount = static_cast<uint16_t>(
        ghostCount + popcount16(value.ghosts));
    accentCount = static_cast<uint16_t>(
        accentCount + popcount16(value.accents));
  }
}

bool runGraph(const GraphSpec& spec,
              RealizationLevel level,
              uint32_t graphIndex,
              const RhythmCatalogView& catalog) {
  const RhythmArchetype* archetype =
      ReferenceVocabulary::archetypeFor(spec.key);
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionFor(spec.key);
  if (!archetype || !definition || archetype->id != definition->archetypeId) {
    std::cerr << "reference mapping lookup failed for " << spec.enumName << "\n";
    return false;
  }
  if (archetype->trajectoryCount != 1 ||
      !archetype->trajectories ||
      archetype->trajectories[0].id != 1) {
    std::cerr << "selected production archetype no longer uses the single "
                 "reference Statement trajectory: "
              << spec.enumName << "\n";
    return false;
  }

  const RhythmPhrasePlan canonical = canonicalPlanFor(*archetype, catalog);
  RhythmPhrasePlan rootPlan = normalizedRoot(canonical, level);
  const CanonicalRhythmCandidateValidation rootValidation =
      canonicalRhythmCandidateValid(
          *archetype, canonical, rootPlan, 0, level,
          BarFunction::Statement, TransformationIntent::Auto,
          nullptr, 0);
  if (rootValidation.diffStatus != CanonicalRhythmDiffStatus::Ok ||
      !rootValidation.legal ||
      rootValidation.stats.deltaCount != 0) {
    std::cerr << "canonical root is not E2b legal for "
              << spec.enumName << "/" << levelName(level) << "\n";
    return false;
  }

  const std::string graphId =
      std::string(spec.reportFamily) + "_" + levelName(level);
  std::cout << "GRAPH\t" << graphIndex
            << "\t" << graphId
            << "\t" << spec.reportFamily
            << "\t" << spec.enumName
            << "\t" << definition->name
            << "\t" << archetype->id
            << "\t" << levelName(level)
            << "\t" << kNodeCeiling
            << "\t" << kTransitionCeiling
            << "\n";

  std::vector<CompactNode> nodes;
  nodes.reserve(4096);
  std::map<std::string, uint32_t> ids;
  const std::string rootKey = nodeKey(rootPlan);
  nodes.push_back({rootPlan.bars[0], 0u, 0u, rootKey});
  ids.emplace(rootKey, 0u);

  uint64_t transitionRecords = 0;
  bool unsupportedProductionOperation = false;

  for (uint32_t cursor = 0; cursor < nodes.size(); ++cursor) {
    const RhythmPhrasePlan current =
        expandNode(canonical, level, nodes[cursor]);
    RhythmMutationDelta proposals[kMaxRhythmMutationDeltasPerBar]{};
    RhythmMutationProducerRequest request{};
    request.archetype = archetype;
    request.canonical = &canonical;
    request.current = &current;
    request.bar = 0;
    request.roles = kAllRhythmRoles;
    request.function = BarFunction::Statement;
    request.intent = TransformationIntent::Auto;
    request.level = level;
    request.generation.projectSeed = kProjectSeed;
    request.generation.phraseOrdinal = kPhraseOrdinal;

    const RhythmMutationProducerResult produced =
        produceRhythmMutationCandidates(
            request, proposals, kMaxRhythmMutationDeltasPerBar);
    if (produced.status != RhythmMutationProducerStatus::Ok ||
        produced.truncated) {
      std::cerr << "producer failed/truncated for " << graphId
                << " node=" << cursor << "\n";
      return false;
    }

    NodeStats stats{};
    std::set<uint32_t> uniqueTargets;
    for (uint16_t i = 0; i < produced.count; ++i) {
      const RhythmMutationDelta& delta = proposals[i];
      if (!rhythmMutationDeltaShapeValid(delta)) {
        std::cerr << "producer emitted invalid E2c delta shape\n";
        return false;
      }
      const uint8_t op = static_cast<uint8_t>(delta.operation);
      if (op >= static_cast<uint8_t>(RhythmMutationOp::Count)) return false;
      ++stats.raw[op];
      if (delta.operation == RhythmMutationOp::KEEP) {
        std::cerr << "KEEP proposal is not a graph alternative\n";
        return false;
      }

      RhythmPhrasePlan candidate{};
      if (!materializeCurrentProductionProposal(
              *archetype, current, level, delta, candidate)) {
        ++stats.materializationFailure;
        if (delta.operation == RhythmMutationOp::DROP ||
            delta.operation == RhythmMutationOp::DISPLACE ||
            delta.operation == RhythmMutationOp::ACCENT) {
          unsupportedProductionOperation = true;
        }
        continue;
      }
      ++stats.materializable;

      const CanonicalRhythmCandidateValidation validation =
          canonicalRhythmCandidateValid(
              *archetype, canonical, candidate, 0, level,
              BarFunction::Statement, TransformationIntent::Auto,
              nullptr, 0);
      if (validation.diffStatus != CanonicalRhythmDiffStatus::Ok ||
          !validation.canonicalPlanValid ||
          !validation.candidatePlanValid) {
        ++stats.structuralRejected;
        continue;
      }
      if (!validation.budgetValid || !validation.legal) {
        ++stats.budgetRejected;
        continue;
      }

      const std::string key = nodeKey(candidate);
      uint32_t targetId = 0;
      auto found = ids.find(key);
      if (found == ids.end()) {
        if (nodes.size() >= kNodeCeiling) {
          std::cerr << "ENUMERATION INCOMPLETE node ceiling reached graph="
                    << graphId << " ceiling=" << kNodeCeiling << "\n";
          return false;
        }
        targetId = static_cast<uint32_t>(nodes.size());
        ids.emplace(key, targetId);
        nodes.push_back({
            candidate.bars[0],
            static_cast<uint32_t>(nodes[cursor].depth + 1u),
            validation.stats.deltaCount,
            key,
        });
      } else {
        targetId = found->second;
      }

      ++stats.legalRecords;
      ++transitionRecords;
      if (transitionRecords > kTransitionCeiling) {
        std::cerr << "ENUMERATION INCOMPLETE transition ceiling reached graph="
                  << graphId << " ceiling=" << kTransitionCeiling << "\n";
        return false;
      }
      if (!uniqueTargets.insert(targetId).second) {
        ++stats.duplicateTarget;
      }

      std::cout << "EDGE\t" << graphIndex
                << "\t" << cursor
                << "\t" << targetId
                << "\t" << opName(delta.operation)
                << "\t" << static_cast<unsigned>(delta.sourceStep)
                << "\t" << static_cast<unsigned>(delta.targetStep)
                << "\n";
    }

    uint16_t totalOccupied = 0;
    uint16_t ghostCount = 0;
    uint16_t accentCount = 0;
    uint8_t roleOccupied[kRhythmRoleCount]{};
    density(current, totalOccupied, ghostCount, accentCount, roleOccupied);
    const bool identityPreserving =
        rhythmMutationPlanValid(*archetype, current);

    std::cout << "NODE\t" << graphIndex
              << "\t" << cursor
              << "\t" << nodes[cursor].depth
              << "\t" << nodes[cursor].layer
              << "\t" << (identityPreserving ? 1 : 0)
              << "\t" << totalOccupied
              << "\t" << ghostCount
              << "\t" << accentCount;
    for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
      std::cout << "\t" << static_cast<unsigned>(roleOccupied[role]);
    }
    for (uint8_t op = 0;
         op < static_cast<uint8_t>(RhythmMutationOp::Count);
         ++op) {
      std::cout << "\t" << stats.raw[op];
    }
    std::cout << "\t" << stats.materializationFailure
              << "\t" << stats.materializable
              << "\t" << stats.structuralRejected
              << "\t" << stats.budgetRejected
              << "\t" << stats.legalRecords
              << "\t" << stats.duplicateTarget
              << "\t" << nodes[cursor].key
              << "\n";
  }

  std::cout << "GRAPH_END\t" << graphIndex
            << "\t" << nodes.size()
            << "\t" << transitionRecords
            << "\t" << (unsupportedProductionOperation ? 1 : 0)
            << "\n";
  if (unsupportedProductionOperation) {
    std::cerr << "CONTRACT GAP: production producer emitted an operation "
                 "without an existing V0R materialization adapter for graph "
              << graphId << "\n";
    return false;
  }
  return true;
}

}  // namespace

int main() {
  const RhythmCatalogView& catalog = ReferenceVocabulary::catalog();
  if (!validateRhythmCatalog(catalog)) {
    std::cerr << "reference vocabulary catalog invalid\n";
    return 10;
  }

  std::cout << "V0R\t1\tseed=" << kProjectSeed
            << "\tphraseOrdinal=" << kPhraseOrdinal
            << "\tnodeCeiling=" << kNodeCeiling
            << "\ttransitionCeiling=" << kTransitionCeiling
            << "\n";

  uint32_t graphIndex = 0;
  for (const GraphSpec& spec : kSpecs) {
    for (RealizationLevel level : kLevels) {
      if (!runGraph(spec, level, graphIndex, catalog)) return 30;
      ++graphIndex;
    }
  }
  if (graphIndex != 18u) return 31;
  std::cout << "V0R_END\tgraphs=" << graphIndex << "\n";
  return 0;
}
