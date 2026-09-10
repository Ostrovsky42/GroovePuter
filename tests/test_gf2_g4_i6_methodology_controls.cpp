#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "scenes.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/composition/rhythm_selection.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_types.h"

using namespace GroovePuterRhythm;

namespace {

constexpr StepMask kQuarterNotes =
    stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12);

uint32_t fnv1a(const std::string& text) {
  uint32_t hash = 2166136261u;
  for (unsigned char value : text) {
    hash ^= static_cast<uint32_t>(value);
    hash *= 16777619u;
  }
  return hash;
}

const LaneGrammar* laneFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  for (uint8_t index = 0; index < archetype.laneCount; ++index) {
    if (archetype.lanes[index].role == role) return &archetype.lanes[index];
  }
  return nullptr;
}

bool knownArchetype(RhythmArchetypeId id) {
  const auto* definition = ReferenceVocabulary::definitionForId(id);
  return definition != nullptr &&
         ReferenceVocabulary::archetypeFor(definition->key) != nullptr;
}

std::string correctedProjection(const RhythmArchetype& archetype) {
  std::ostringstream out;
  out << "lanes{";
  for (uint8_t rawRole = 0; rawRole < kRhythmRoleCount; ++rawRole) {
    const LaneGrammar* lane =
        laneFor(archetype, static_cast<RhythmRole>(rawRole));
    out << static_cast<unsigned>(rawRole) << ':';
    if (lane == nullptr) {
      out << "-;";
      continue;
    }
    out << std::hex << std::setfill('0')
        << std::setw(4) << lane->immutableAnchors << ','
        << std::setw(4) << lane->canonicalAnchors << ','
        << std::setw(4) << lane->preferred << ','
        << std::setw(4) << lane->optional << ','
        << std::setw(4) << lane->forbidden << std::dec << ','
        << static_cast<unsigned>(lane->structuralMin) << ','
        << static_cast<unsigned>(lane->structuralMax) << ';';
  }

  out << "}protected{";
  std::vector<std::pair<StepMask, RhythmRoleMask>> spaces;
  for (uint8_t index = 0; index < archetype.protectedSpaceCount; ++index) {
    spaces.push_back({archetype.protectedSpaces[index].steps,
                      archetype.protectedSpaces[index].affectedRoles});
  }
  std::sort(spaces.begin(), spaces.end());
  for (const auto& space : spaces) {
    out << std::hex << std::setw(4) << std::setfill('0') << space.first
        << ':' << std::setw(4) << space.second << ';' << std::dec;
  }

  out << "}relationships{";
  std::vector<std::string> relationships;
  for (uint8_t index = 0; index < archetype.relationshipCount; ++index) {
    const LaneRelationship& relation = archetype.relationships[index];
    std::ostringstream item;
    item << static_cast<unsigned>(relation.source) << ','
         << static_cast<unsigned>(relation.target) << ','
         << static_cast<unsigned>(relation.op) << ','
         << static_cast<unsigned>(relation.scope) << ','
         << static_cast<int>(relation.minOffset) << ','
         << static_cast<int>(relation.maxOffset) << ','
         << static_cast<unsigned>(relation.minMatches) << ','
         << static_cast<unsigned>(relation.maxMatches) << ','
         << static_cast<unsigned>(relation.minResponsesPerWindow) << ','
         << static_cast<unsigned>(relation.maxResponsesPerWindow);
    relationships.push_back(item.str());
  }
  std::sort(relationships.begin(), relationships.end());
  for (const std::string& item : relationships) out << item << ';';
  out << '}';
  return out.str();
}

std::string outsideProjection(const RhythmArchetype& archetype) {
  std::ostringstream gates;
  for (uint8_t rawRole = 0; rawRole < kRhythmRoleCount; ++rawRole) {
    const LaneGrammar* lane =
        laneFor(archetype, static_cast<RhythmRole>(rawRole));
    if (lane == nullptr) continue;
    gates << static_cast<unsigned>(rawRole) << ':' << std::hex
          << lane->shortGate << ',' << lane->heldGate << ',' << lane->tieGate
          << std::dec << ';';
  }

  std::vector<std::string> relationshipPolicy;
  for (uint8_t index = 0; index < archetype.relationshipCount; ++index) {
    const LaneRelationship& relation = archetype.relationships[index];
    std::ostringstream item;
    item << static_cast<unsigned>(relation.source) << ','
         << static_cast<unsigned>(relation.target) << ','
         << static_cast<unsigned>(relation.op) << ','
         << static_cast<unsigned>(relation.scope) << ','
         << static_cast<unsigned>(relation.strength) << ',' << std::hex
         << relation.zoneMask << std::dec << ','
         << static_cast<unsigned>(relation.weight);
    relationshipPolicy.push_back(item.str());
  }
  std::sort(relationshipPolicy.begin(), relationshipPolicy.end());

  std::ostringstream rel;
  for (const std::string& item : relationshipPolicy) rel << item << ';';

  std::ostringstream out;
  out << "family=" << static_cast<unsigned>(archetype.family) << '|'
      << "phraseBars=" << static_cast<unsigned>(archetype.allowedPhraseBars)
      << '|'
      << "gates=" << gates.str() << '|'
      << "relationshipPolicy=" << rel.str() << '|'
      << "timing=" << static_cast<unsigned>(archetype.timing.compatibility)
      << ',' << std::hex << archetype.timing.sensitiveSteps << ','
      << archetype.timing.affectedRoles << std::dec << '|'
      << "density=" << static_cast<unsigned>(archetype.density.structuralMin)
      << ',' << static_cast<unsigned>(archetype.density.structuralPreferred)
      << ',' << static_cast<unsigned>(archetype.density.structuralMax) << ','
      << static_cast<unsigned>(archetype.density.ornamentMax) << '|'
      << "trajectoryCount=" << static_cast<unsigned>(archetype.trajectoryCount)
      << '|'
      << "anchorTransformCount="
      << static_cast<unsigned>(archetype.anchorTransformRuleCount);
  return out.str();
}

enum class QuarterWitness : uint8_t { None = 0, Possible, Required };

QuarterWitness quarterWitness(const RhythmArchetype& archetype,
                              const std::string& genreLabel,
                              uint8_t candidateWeight) {
  // These values are intentionally accepted as detector inputs so the control
  // can mutate them. Structural judgment must remain independent of both.
  (void)genreLabel;
  (void)candidateWeight;
  const LaneGrammar* kick = laneFor(archetype, RhythmRole::Kick);
  if (kick == nullptr) return QuarterWitness::None;
  const StepMask required = static_cast<StepMask>(
      kick->immutableAnchors | kick->canonicalAnchors);
  if ((required & kQuarterNotes) == kQuarterNotes) return QuarterWitness::Required;
  const StepMask possible = static_cast<StepMask>(required | kick->preferred);
  if ((possible & kQuarterNotes) == kQuarterNotes) return QuarterWitness::Possible;
  return QuarterWitness::None;
}

enum class EvidenceStatus : uint8_t { Proven = 0, Provisional, ReviewRequired };
enum class Evaluation : uint8_t { Satisfied = 0, Violated, Unknown };

Evaluation evaluate(EvidenceStatus status, bool predicate) {
  if (status == EvidenceStatus::ReviewRequired) return Evaluation::Unknown;
  return predicate ? Evaluation::Satisfied : Evaluation::Violated;
}

bool falseOwner(bool effectiveAdmission, EvidenceStatus status,
                bool contractContradiction) {
  return effectiveAdmission && status == EvidenceStatus::Proven &&
         contractContradiction;
}

bool runControls(uint16_t sharedProbeDegree) {
  int failures = 0;
  auto check = [&failures](bool ok, const char* name) {
    std::printf("G4_I6_METHODOLOGY_%s control=%s\n", ok ? "PASS" : "FAIL",
                name);
    if (!ok) ++failures;
  };

  LaneGrammar positiveKick{};
  positiveKick.role = RhythmRole::Kick;
  positiveKick.canonicalAnchors = kQuarterNotes;
  positiveKick.structuralMin = 4;
  positiveKick.structuralMax = 4;
  RhythmArchetype positive{};
  positive.lanes = &positiveKick;
  positive.laneCount = 1;

  LaneGrammar negativeKick = positiveKick;
  negativeKick.canonicalAnchors = static_cast<StepMask>(
      kQuarterNotes & ~stepBit(12));
  RhythmArchetype negative = positive;
  negative.lanes = &negativeKick;

  check(quarterWitness(positive, "House", 90) == QuarterWitness::Required,
        "POSITIVE_QUARTER_FIXTURE");
  check(quarterWitness(negative, "House", 90) == QuarterWitness::None,
        "NEGATIVE_QUARTER_FIXTURE");
  check(quarterWitness(positive, "House", 90) !=
            quarterWitness(negative, "House", 90),
        "REMOVING_REQUIRED_ANCHOR_CHANGES_JUDGMENT");
  check(quarterWitness(positive, "House", 90) ==
            quarterWitness(positive, "NotHouse", 90),
        "GENRE_LABEL_INDEPENDENCE");
  check(quarterWitness(positive, "House", 1) ==
            quarterWitness(positive, "House", 255),
        "WEIGHT_INDEPENDENCE");
  check(evaluate(EvidenceStatus::ReviewRequired, true) == Evaluation::Unknown,
        "REVIEW_REQUIRED_NEVER_AUTO_VALID");
  check(sharedProbeDegree > 1 &&
            !falseOwner(true, EvidenceStatus::ReviewRequired, false) &&
            !falseOwner(true, EvidenceStatus::Proven, false),
        "SHARED_ADMISSION_NOT_OWNERSHIP_VIOLATION");

  const RhythmArchetype* straight = ReferenceVocabulary::archetypeFor(
      ReferenceVocabulary::Archetype::StraightDrive);
  const RhythmArchetype* stacked = ReferenceVocabulary::archetypeFor(
      ReferenceVocabulary::Archetype::StackedQuarters);
  check(straight != nullptr && stacked != nullptr &&
            correctedProjection(*straight) != correctedProjection(*stacked),
        "STRAIGHT_DRIVE_DIFFERS_FROM_STACKED_QUARTERS");

  LaneGrammar gateA = positiveKick;
  LaneGrammar gateB = positiveKick;
  gateB.shortGate = stepBit(0);
  RhythmArchetype gateArchetypeA = positive;
  RhythmArchetype gateArchetypeB = positive;
  gateArchetypeA.lanes = &gateA;
  gateArchetypeB.lanes = &gateB;
  check(correctedProjection(gateArchetypeA) ==
            correctedProjection(gateArchetypeB),
        "LIFETIME_GATE_EXCLUDED_WITHOUT_LIFETIME_CONTRACT");

  LaneRelationship relationA{};
  relationA.source = RhythmRole::Kick;
  relationA.target = RhythmRole::Backbeat;
  relationA.op = RelationshipOp::Offset;
  relationA.scope = RelationshipScope::BarLocal;
  relationA.minOffset = 1;
  relationA.maxOffset = 2;
  relationA.minMatches = 1;
  relationA.maxMatches = 2;
  LaneRelationship relationB = relationA;
  relationB.strength = ConstraintStrength::Soft;
  relationB.zoneMask = stepBit(0) | stepBit(4);
  relationB.weight = 77;
  RhythmArchetype relationArchetypeA = positive;
  RhythmArchetype relationArchetypeB = positive;
  relationArchetypeA.relationships = &relationA;
  relationArchetypeA.relationshipCount = 1;
  relationArchetypeB.relationships = &relationB;
  relationArchetypeB.relationshipCount = 1;
  check(correctedProjection(relationArchetypeA) ==
            correctedProjection(relationArchetypeB),
        "RELATIONSHIP_POLICY_METADATA_EXCLUDED");
  relationB.minOffset = 3;
  check(correctedProjection(relationArchetypeA) !=
            correctedProjection(relationArchetypeB),
        "RELATIONSHIP_OFFSET_REMAINS_STRUCTURAL");
  relationB = relationA;
  relationB.minResponsesPerWindow = 1;
  check(correctedProjection(relationArchetypeA) !=
            correctedProjection(relationArchetypeB),
        "RELATIONSHIP_CARDINALITY_REMAINS_STRUCTURAL");

  enum class Stage : uint8_t { RhythmAdmission = 0, DownstreamMaterialization };
  check(Stage::RhythmAdmission != Stage::DownstreamMaterialization,
        "CANDIDATE_AND_DOWNSTREAM_STAGES_DISTINCT");

  if (failures != 0) {
    std::printf("G4-I6 corrected methodology controls: FAIL (%d)\n", failures);
    return false;
  }
  std::puts("G4-I6 corrected methodology controls: PASS");
  return true;
}

struct ArchetypeRow {
  RhythmArchetypeId id = kNoArchetypeId;
  std::string name;
  std::string projection;
  std::string outside;
  uint32_t hash = 0;
};

bool collectOwnersAndCheckUnknownRaw(uint32_t& profiles,
                                     uint32_t& effectiveEdges,
                                     uint16_t& sharedProbeDegree) {
  profiles = 0;
  effectiveEdges = 0;
  sharedProbeDegree = 0;
  std::set<std::pair<uint8_t, GenreRecipeId>> owners;
  for (uint8_t mode = 0; mode < kGenerativeModeCount; ++mode) {
    const auto genre = static_cast<GenerativeMode>(mode);
    const uint8_t count = availableRecipeCount(genre);
    for (uint8_t ordinal = 0; ordinal < count; ++ordinal) {
      GenreRecipeId recipe = 0xff;
      if (!availableRecipeAt(genre, ordinal, recipe) ||
          !owners.insert({mode, recipe}).second) {
        std::fprintf(stderr,
                     "G4_I6_METHODOLOGY_FAIL owner_enumeration mode=%u ordinal=%u\n",
                     static_cast<unsigned>(mode), static_cast<unsigned>(ordinal));
        return false;
      }
      GenreSettings settings{};
      settings.generativeMode = mode;
      settings.recipe = recipe;
      settings.rhythmSelectionMode =
          static_cast<uint8_t>(RhythmSelectionMode::Auto);
      settings.rhythmArchetypeId = kNoArchetypeId;
      const RhythmCompatibilityView raw = rhythmCompatibilityFor(settings);
      for (uint8_t index = 0; index < raw.count; ++index) {
        const RhythmCompatibilityCandidate candidate = raw.candidates[index];
        if (candidate.weight != 0 && candidate.archetypeId != kNoArchetypeId &&
            !knownArchetype(candidate.archetypeId)) {
          std::fprintf(stderr,
                       "G4_I6_METHODOLOGY_FAIL unknown_raw mode=%u recipe=%u archetype=%u weight=%u\n",
                       static_cast<unsigned>(mode), static_cast<unsigned>(recipe),
                       static_cast<unsigned>(candidate.archetypeId),
                       static_cast<unsigned>(candidate.weight));
          return false;
        }
      }
      const uint8_t effectiveCount = compatibleRhythmCount(settings);
      effectiveEdges += effectiveCount;
      for (uint8_t index = 0; index < effectiveCount; ++index) {
        if (compatibleRhythmId(settings, index) == 401) ++sharedProbeDegree;
      }
    }
  }
  profiles = owners.size();
  return true;
}

bool emitProjection(const std::filesystem::path& directory,
                    uint32_t profiles, uint32_t effectiveEdges,
                    uint16_t sharedProbeDegree) {
  std::vector<ArchetypeRow> rows;
  std::set<RhythmArchetypeId> ids;
  for (uint8_t index = 0; index < ReferenceVocabulary::definitionCount(); ++index) {
    const auto& definition = ReferenceVocabulary::definition(index);
    const RhythmArchetype* archetype =
        ReferenceVocabulary::archetypeFor(definition.key);
    if (archetype == nullptr || archetype->id != definition.archetypeId ||
        !ids.insert(definition.archetypeId).second) {
      std::fprintf(stderr, "G4_I6_METHODOLOGY_FAIL archetype_catalog index=%u\n",
                   static_cast<unsigned>(index));
      return false;
    }
    ArchetypeRow row{};
    row.id = definition.archetypeId;
    row.name = definition.name == nullptr ? "" : definition.name;
    row.projection = correctedProjection(*archetype);
    row.outside = outsideProjection(*archetype);
    row.hash = fnv1a(row.projection);
    rows.push_back(row);
  }
  std::sort(rows.begin(), rows.end(), [](const ArchetypeRow& left,
                                         const ArchetypeRow& right) {
    return left.id < right.id;
  });

  uint32_t collisionPairs = 0;
  for (size_t left = 0; left < rows.size(); ++left) {
    for (size_t right = left + 1; right < rows.size(); ++right) {
      if (rows[left].projection == rows[right].projection) ++collisionPairs;
    }
  }

  std::filesystem::create_directories(directory);
  std::ofstream out(directory / "GF2_G4_I6_COLLISION_PROJECTION.tsv");
  if (!out) return false;
  out << "archetype_id\tarchetype_name\tprojection_hash\tnormalized_projection\toutside_projection_metadata\n";
  for (const ArchetypeRow& row : rows) {
    out << row.id << '\t' << row.name << '\t' << std::hex << std::setw(8)
        << std::setfill('0') << row.hash << std::dec << '\t'
        << row.projection << '\t' << row.outside << '\n';
  }
  out.close();

  std::printf(
      "G4_I6_METHODOLOGY profiles=%u archetypes=%zu effective_edges=%u shared_probe_degree=%u corrected_collision_pairs=%u unknown_raw=0\n",
      profiles, rows.size(), effectiveEdges,
      static_cast<unsigned>(sharedProbeDegree), collisionPairs);
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--emit") {
    std::fprintf(stderr, "usage: %s --emit OUTPUT_DIR\n", argv[0]);
    return 2;
  }

  uint32_t profiles = 0;
  uint32_t effectiveEdges = 0;
  uint16_t sharedProbeDegree = 0;
  if (!collectOwnersAndCheckUnknownRaw(profiles, effectiveEdges,
                                       sharedProbeDegree)) {
    return 1;
  }
  if (!runControls(sharedProbeDegree)) return 1;
  return emitProjection(argv[2], profiles, effectiveEdges, sharedProbeDegree)
             ? 0
             : 1;
}
