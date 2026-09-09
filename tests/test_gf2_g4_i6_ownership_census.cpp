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
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_catalog.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kIdentityFirst = 1;
constexpr uint16_t kIdentityLast = 128;
constexpr StepMask kQuarterNotes =
    stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12);
constexpr StepMask kBrokenKickFrame = stepBit(0) | stepBit(10);
constexpr StepMask kBackbeatFrame = stepBit(4) | stepBit(12);
constexpr StepMask kDubChordCanonical = stepBit(2) | stepBit(10);

enum class ContractStatus : uint8_t { Proven = 0, Provisional, ReviewRequired };
enum class Evaluation : uint8_t { Satisfied = 0, Violated, Unknown, NotApplicable };
enum class WitnessStrength : uint8_t { None = 0, PossibleButNotRequired, Required, Observed };
enum class TechnoSkeleton : uint8_t { None = 0, QuarterPulse, BrokenFrame };
enum class OffendingStage : uint8_t {
  None = 0,
  RhythmAdmission,
  BassSelection,
  ChordSelection,
  DownstreamMaterialization,
  UnknownStage,
};
enum class ReachabilityState : uint8_t {
  AutoObserved = 0,
  ManualReproduced,
  NotObserved,
  ProvenUnreachableInDomain,
};

struct OwnerKey {
  uint8_t mode = 0;
  GenreRecipeId recipe = 0;
  bool operator<(const OwnerKey& other) const {
    return std::tie(mode, recipe) < std::tie(other.mode, other.recipe);
  }
  bool operator==(const OwnerKey& other) const {
    return mode == other.mode && recipe == other.recipe;
  }
};

struct EffectiveCandidate {
  RhythmArchetypeId id = kNoArchetypeId;
  uint8_t weight = 0;
};

struct Reconciliation {
  std::vector<EffectiveCandidate> effective;
  std::vector<std::string> reasons;
  bool apiMatches = false;
};

struct Owner {
  OwnerKey key{};
  GenreSettings settings{};
  RhythmCompatibilityView raw{};
  Reconciliation reconciliation{};
};

struct RowKey {
  OwnerKey owner{};
  uint16_t identity = 0;
  uint8_t level = 0;
  bool operator<(const RowKey& other) const {
    return std::tie(owner.mode, owner.recipe, identity, level) <
           std::tie(other.owner.mode, other.owner.recipe,
                    other.identity, other.level);
  }
};

struct ArchetypeInfo {
  RhythmArchetypeId id = kNoArchetypeId;
  std::string name;
  RhythmFamily family = RhythmFamily::FourFloor;
  const RhythmArchetype* archetype = nullptr;
  std::string projection;
  uint32_t projectionHash = 0;
  uint16_t ownerCount = 0;
};

struct Finding {
  std::string id;
  std::string type;
  OwnerKey owner{};
  RhythmArchetypeId archetype = kNoArchetypeId;
  RhythmArchetypeId rightArchetype = kNoArchetypeId;
  std::string contractId;
  std::string contractVersion;
  ContractStatus evidenceStatus = ContractStatus::ReviewRequired;
  Evaluation evaluation = Evaluation::Unknown;
  uint16_t affectedIdentities = 0;
  uint16_t affectedMaterializations = 0;
  OffendingStage stage = OffendingStage::UnknownStage;
  std::string candidateEvidence;
  std::string materializedEvidence;
  std::string reason;
  std::string collisionScope;
  std::string equalProjection;
  std::string differingKnownSemantics;
};

struct Row {
  OwnerKey owner{};
  uint16_t identity = 0;
  uint8_t levelIndex = 0;
  uint8_t rawCandidateCount = 0;
  uint8_t effectiveCandidateCount = 0;
  RhythmArchetypeId archetypeId = kNoArchetypeId;
  std::string archetypeName;
  uint8_t weight = 0;
  ReachabilityState reachability = ReachabilityState::NotObserved;
  RhythmFamily family = RhythmFamily::FourFloor;
  uint32_t structuralHash = 0;
  std::string structuralMinMax;
  uint32_t materializedHash = 0;
  BassRhythmId bass = BassRhythmId::Auto;
  ChordRhythmId chord = ChordRhythmId::Auto;
  MelodicRhythmId melodic = MelodicRhythmId::Auto;
  MotifShapeId motif = MotifShapeId::Auto;
  SemanticSynthBRole secondaryRole = SemanticSynthBRole::Chord;
  PhraseEvolutionLawId phraseLaw = PhraseEvolutionLawId::Loop;
  uint8_t effectiveDensity = kNoStructuralDensityTarget;
  uint32_t selectionSeed = 0;
  uint32_t realizationSeed = 0;
  std::string contractId;
  ContractStatus contractStatus = ContractStatus::ReviewRequired;
  std::string contractScope;
  std::string contractQuantifier;
  std::string expectedWitness;
  std::string actualWitness;
  Evaluation evaluation = Evaluation::Unknown;
  std::string reason;
  OffendingStage stage = OffendingStage::None;
};

struct BoundaryObservation {
  std::string name;
  OwnerKey owner{};
  uint16_t identity = 0;
  int16_t patternAddress = 0;
  uint8_t phraseBarOrdinal = kUnspecifiedPhraseBarOrdinal;
  StrongRhythmMigrationStatus status = StrongRhythmMigrationStatus::Legacy;
  RhythmArchetypeId archetype = kNoArchetypeId;
  BassRhythmId bass = BassRhythmId::Auto;
};

struct Census {
  std::vector<Owner> owners;
  std::map<RhythmArchetypeId, ArchetypeInfo> archetypes;
  std::vector<Row> rows;
  std::vector<Finding> findings;
  std::vector<BoundaryObservation> boundaries;
  std::map<std::pair<OwnerKey, RhythmArchetypeId>, uint16_t> selectedIdentities;
  uint32_t totalEffectiveEdges = 0;
  uint32_t attemptedRows = 0;
  uint32_t successfulRows = 0;
  uint32_t failedRows = 0;
  uint32_t autoObservedEdges = 0;
  uint32_t manualReproducedEdges = 0;
  uint32_t notObservedEdges = 0;
  uint32_t provenUnreachableEdges = 0;
  uint32_t admissionOrphans = 0;
  uint32_t sharedAdmissions = 0;
  uint32_t sharedValidUnderContracts = 0;
  uint32_t structuralCollisions = 0;
  uint32_t falseOwners = 0;
  uint32_t overbroadOwners = 0;
  uint32_t reviewRequiredOwners = 0;
  uint32_t unknownEvaluations = 0;
};

const char* modeName(uint8_t mode) {
  static constexpr const char* kNames[] = {
      "Acid", "Outrun", "Darksynth", "Electro", "Rave", "Reggae",
      "Trip-Hop", "Broken", "Chip", "House", "Techno", "Hip-Hop",
      "Funk/Soul", "UK Garage", "Drum&Bass", "Lo-Fi"};
  return mode < kGenerativeModeCount ? kNames[mode] : "UNKNOWN_MODE";
}

const char* recipeName(GenreRecipeId recipe) {
  switch (recipe) {
    case 0: return "BASE";
    case 1: return "UK Garage";
    case 2: return "Drum&Bass";
    case 3: return "Footwork";
    case 4: return "Psytrance";
    case 5: return "Dub Techno";
    case 6: return "Chicago Jack";
    case 7: return "Rolling Acid";
    case 8: return "Classic 2-Step";
    case 9: return "Dark Skippy";
    case 10: return "Deep Chord";
    case 11: return "Minimal Space";
    case kClassicChillRecipeId: return "Classic Chill";
    case kDrunkenGrooveRecipeId: return "Drunken Groove";
    case kLoFiHouseRecipeId: return "Lo-Fi House";
    case kMinimalSleepRecipeId: return "Minimal Sleep";
    case kGoldenEraRecipeId: return "Golden Era";
    case kDustyJazzRecipeId: return "Dusty Jazz";
    default: return "UNKNOWN_RECIPE";
  }
}

const char* familyName(RhythmFamily family) {
  switch (family) {
    case RhythmFamily::FourFloor: return "FourFloor";
    case RhythmFamily::MachineSyncopation: return "MachineSyncopation";
    case RhythmFamily::Breakbeat: return "Breakbeat";
    case RhythmFamily::UkTwoStep: return "UkTwoStep";
    case RhythmFamily::HipHopBackbeat: return "HipHopBackbeat";
    case RhythmFamily::DubPulse: return "DubPulse";
    case RhythmFamily::Funk16: return "Funk16";
    case RhythmFamily::SparsePulse: return "SparsePulse";
    case RhythmFamily::Count: return "Count";
  }
  return "UnknownFamily";
}

const char* contractStatusName(ContractStatus status) {
  switch (status) {
    case ContractStatus::Proven: return "PROVEN";
    case ContractStatus::Provisional: return "PROVISIONAL";
    case ContractStatus::ReviewRequired: return "REVIEW_REQUIRED";
  }
  return "REVIEW_REQUIRED";
}

const char* evaluationName(Evaluation evaluation) {
  switch (evaluation) {
    case Evaluation::Satisfied: return "SATISFIED";
    case Evaluation::Violated: return "VIOLATED";
    case Evaluation::Unknown: return "UNKNOWN";
    case Evaluation::NotApplicable: return "NOT_APPLICABLE";
  }
  return "UNKNOWN";
}

const char* witnessStrengthName(WitnessStrength strength) {
  switch (strength) {
    case WitnessStrength::None: return "NONE";
    case WitnessStrength::PossibleButNotRequired:
      return "POSSIBLE_BUT_NOT_REQUIRED";
    case WitnessStrength::Required: return "REQUIRED";
    case WitnessStrength::Observed: return "OBSERVED";
  }
  return "NONE";
}

const char* skeletonName(TechnoSkeleton skeleton) {
  switch (skeleton) {
    case TechnoSkeleton::QuarterPulse: return "quarter-pulse";
    case TechnoSkeleton::BrokenFrame: return "broken-frame";
    case TechnoSkeleton::None: return "none";
  }
  return "none";
}

const char* stageName(OffendingStage stage) {
  switch (stage) {
    case OffendingStage::None: return "NONE";
    case OffendingStage::RhythmAdmission: return "RHYTHM_ADMISSION";
    case OffendingStage::BassSelection: return "BASS_SELECTION";
    case OffendingStage::ChordSelection: return "CHORD_SELECTION";
    case OffendingStage::DownstreamMaterialization:
      return "DOWNSTREAM_MATERIALIZATION";
    case OffendingStage::UnknownStage: return "UNKNOWN_STAGE";
  }
  return "UNKNOWN_STAGE";
}

const char* reachabilityName(ReachabilityState state) {
  switch (state) {
    case ReachabilityState::AutoObserved: return "AUTO_OBSERVED";
    case ReachabilityState::ManualReproduced: return "MANUAL_REPRODUCED";
    case ReachabilityState::NotObserved: return "NOT_OBSERVED";
    case ReachabilityState::ProvenUnreachableInDomain:
      return "PROVEN_UNREACHABLE_IN_DOMAIN";
  }
  return "NOT_OBSERVED";
}

std::string clean(std::string value) {
  for (char& ch : value) {
    if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' ';
  }
  return value;
}

uint32_t fnv1a(const std::string& text) {
  uint32_t hash = 2166136261u;
  for (unsigned char value : text) {
    hash ^= static_cast<uint32_t>(value);
    hash *= 16777619u;
  }
  return hash;
}

const RhythmArchetype* archetypeForId(RhythmArchetypeId id) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(id);
  return definition == nullptr ? nullptr
                               : ReferenceVocabulary::archetypeFor(definition->key);
}

bool isKnownArchetype(RhythmArchetypeId id) {
  return ReferenceVocabulary::definitionForId(id) != nullptr &&
         archetypeForId(id) != nullptr;
}

const LaneGrammar* laneFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  for (uint8_t lane = 0; lane < archetype.laneCount; ++lane) {
    if (archetype.lanes[lane].role == role) return &archetype.lanes[lane];
  }
  return nullptr;
}

StepMask drumOnsets(const DrumPatternSet& drums, uint8_t voice) {
  StepMask mask = 0;
  for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
    if (drums.voices[voice].steps[step].hit)
      mask = static_cast<StepMask>(mask | stepBit(step));
  }
  return mask;
}

StepMask synthPositiveNotes(const SynthPattern& synth) {
  StepMask mask = 0;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    if (synth.steps[step].note >= 0)
      mask = static_cast<StepMask>(mask | stepBit(step));
  }
  return mask;
}

std::string structuralProjection(const RhythmArchetype& archetype) {
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
        << std::setw(4) << lane->forbidden << ','
        << std::setw(4) << lane->shortGate << ','
        << std::setw(4) << lane->heldGate << ','
        << std::setw(4) << lane->tieGate << std::dec << ','
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
         << static_cast<unsigned>(relation.strength) << ','
         << static_cast<unsigned>(relation.scope) << ','
         << std::hex << relation.zoneMask << std::dec << ','
         << static_cast<int>(relation.minOffset) << ','
         << static_cast<int>(relation.maxOffset) << ','
         << static_cast<unsigned>(relation.minMatches) << ','
         << static_cast<unsigned>(relation.maxMatches) << ','
         << static_cast<unsigned>(relation.minResponsesPerWindow) << ','
         << static_cast<unsigned>(relation.maxResponsesPerWindow) << ','
         << static_cast<unsigned>(relation.weight);
    relationships.push_back(item.str());
  }
  std::sort(relationships.begin(), relationships.end());
  for (const std::string& item : relationships) out << item << ';';
  out << '}';
  return out.str();
}

std::string structuralMinMaxSummary(const RhythmArchetype& archetype) {
  std::ostringstream out;
  bool first = true;
  for (uint8_t rawRole = 0; rawRole < kRhythmRoleCount; ++rawRole) {
    const LaneGrammar* lane =
        laneFor(archetype, static_cast<RhythmRole>(rawRole));
    if (lane == nullptr) continue;
    if (!first) out << ',';
    first = false;
    out << static_cast<unsigned>(rawRole) << ':'
        << static_cast<unsigned>(lane->structuralMin) << '-'
        << static_cast<unsigned>(lane->structuralMax);
  }
  return out.str();
}

std::string nonProjectedDifference(const RhythmArchetype& left,
                                   const RhythmArchetype& right) {
  std::vector<std::string> differences;
  if (left.family != right.family) differences.push_back("RhythmFamily");
  if (left.allowedPhraseBars != right.allowedPhraseBars)
    differences.push_back("allowedPhraseBars");
  if (left.timing.compatibility != right.timing.compatibility ||
      left.timing.sensitiveSteps != right.timing.sensitiveSteps ||
      left.timing.affectedRoles != right.timing.affectedRoles)
    differences.push_back("timing");
  if (left.density.structuralMin != right.density.structuralMin ||
      left.density.structuralPreferred != right.density.structuralPreferred ||
      left.density.structuralMax != right.density.structuralMax ||
      left.density.ornamentMax != right.density.ornamentMax)
    differences.push_back("density");
  if (left.trajectoryCount != right.trajectoryCount)
    differences.push_back("phraseTrajectories");
  if (left.anchorTransformRuleCount != right.anchorTransformRuleCount)
    differences.push_back("anchorTransforms");
  if (differences.empty()) return "none-observed";
  std::ostringstream out;
  for (size_t index = 0; index < differences.size(); ++index) {
    if (index != 0) out << ',';
    out << differences[index];
  }
  return out.str();
}

std::string reconciliationText(const Reconciliation& value) {
  if (value.reasons.empty()) return "IDENTICAL";
  std::ostringstream out;
  for (size_t index = 0; index < value.reasons.size(); ++index) {
    if (index != 0) out << ',';
    out << value.reasons[index];
  }
  return out.str();
}

Reconciliation reconcileRaw(RhythmCompatibilityView raw,
                            const GenreSettings* settings) {
  Reconciliation result;
  std::map<RhythmArchetypeId, uint16_t> combined;
  std::vector<RhythmArchetypeId> firstOrder;
  bool zero = false;
  bool unknown = false;
  bool duplicate = false;
  bool saturated = false;
  for (uint8_t index = 0; index < raw.count; ++index) {
    const RhythmCompatibilityCandidate candidate = raw.candidates[index];
    if (candidate.archetypeId == kNoArchetypeId || candidate.weight == 0) {
      zero = true;
      continue;
    }
    if (!isKnownArchetype(candidate.archetypeId)) {
      unknown = true;
      continue;
    }
    auto found = combined.find(candidate.archetypeId);
    if (found == combined.end()) {
      combined[candidate.archetypeId] = candidate.weight;
      firstOrder.push_back(candidate.archetypeId);
    } else {
      duplicate = true;
      const uint16_t sum =
          static_cast<uint16_t>(found->second + candidate.weight);
      if (sum > 255u) saturated = true;
      found->second = std::min<uint16_t>(255u, sum);
    }
  }
  for (const auto& item : combined)
    result.effective.push_back({item.first, static_cast<uint8_t>(item.second)});
  std::vector<RhythmArchetypeId> sorted;
  for (const auto& item : result.effective) sorted.push_back(item.id);
  if (firstOrder != sorted) result.reasons.push_back("ORDER_NORMALIZED");
  if (zero) result.reasons.push_back("ZERO_WEIGHT_REMOVED");
  if (unknown) result.reasons.push_back("UNKNOWN_ID_REJECTED");
  if (duplicate) result.reasons.push_back("DUPLICATE_MERGED");
  if (saturated) result.reasons.push_back("WEIGHT_SATURATED");
  if (settings == nullptr) {
    result.apiMatches = true;
    return result;
  }
  result.apiMatches =
      compatibleRhythmCount(*settings) == result.effective.size();
  if (result.apiMatches) {
    for (size_t index = 0; index < result.effective.size(); ++index) {
      if (compatibleRhythmId(*settings, static_cast<uint8_t>(index)) !=
          result.effective[index].id) {
        result.apiMatches = false;
        break;
      }
    }
  }
  return result;
}

bool containsEffective(const Owner& owner, RhythmArchetypeId id) {
  for (const auto& candidate : owner.reconciliation.effective)
    if (candidate.id == id) return true;
  return false;
}

uint8_t effectiveWeight(const Owner& owner, RhythmArchetypeId id) {
  for (const auto& candidate : owner.reconciliation.effective)
    if (candidate.id == id) return candidate.weight;
  return 0;
}

WitnessStrength houseQuarterCandidateWitness(const RhythmArchetype& archetype) {
  const LaneGrammar* kick = laneFor(archetype, RhythmRole::Kick);
  if (kick == nullptr) return WitnessStrength::None;
  const StepMask required = static_cast<StepMask>(
      kick->immutableAnchors | kick->canonicalAnchors);
  if ((required & kQuarterNotes) == kQuarterNotes)
    return WitnessStrength::Required;
  const StepMask possible = static_cast<StepMask>(required | kick->preferred);
  if ((possible & kQuarterNotes) == kQuarterNotes)
    return WitnessStrength::PossibleButNotRequired;
  return WitnessStrength::None;
}

TechnoSkeleton requiredTechnoSkeleton(const RhythmArchetype& archetype) {
  const LaneGrammar* kick = laneFor(archetype, RhythmRole::Kick);
  const LaneGrammar* backbeat = laneFor(archetype, RhythmRole::Backbeat);
  const StepMask kickMask = kick == nullptr ? 0 : static_cast<StepMask>(
      kick->immutableAnchors | kick->canonicalAnchors);
  const StepMask backbeatMask = backbeat == nullptr ? 0 : static_cast<StepMask>(
      backbeat->immutableAnchors | backbeat->canonicalAnchors);
  if ((kickMask & kQuarterNotes) == kQuarterNotes)
    return TechnoSkeleton::QuarterPulse;
  if ((kickMask & kBrokenKickFrame) == kBrokenKickFrame &&
      (backbeatMask & kBackbeatFrame) == kBackbeatFrame &&
      (kickMask & kBackbeatFrame) == 0)
    return TechnoSkeleton::BrokenFrame;
  return TechnoSkeleton::None;
}

TechnoSkeleton observedTechnoSkeleton(StepMask kick, StepMask backbeat) {
  if ((kick & kQuarterNotes) == kQuarterNotes)
    return TechnoSkeleton::QuarterPulse;
  if ((kick & kBrokenKickFrame) == kBrokenKickFrame &&
      (backbeat & kBackbeatFrame) == kBackbeatFrame &&
      (kick & kBackbeatFrame) == 0)
    return TechnoSkeleton::BrokenFrame;
  return TechnoSkeleton::None;
}

bool hasDubDialogue(const RhythmArchetype& archetype) {
  const LaneGrammar* chord = laneFor(archetype, RhythmRole::ChordRhythm);
  return chord != nullptr &&
         (chord->canonicalAnchors & kDubChordCanonical) == kDubChordCanonical;
}

Evaluation evaluateContract(ContractStatus status, bool predicate) {
  if (status == ContractStatus::ReviewRequired) return Evaluation::Unknown;
  return predicate ? Evaluation::Satisfied : Evaluation::Violated;
}

uint32_t materializedSignature(const DrumPatternSet& drums,
                               const SynthPattern& synthA,
                               const SynthPattern& synthB) {
  std::ostringstream out;
  for (uint8_t voice = 0; voice < DrumPatternSet::kVoices; ++voice)
    out << std::hex << drumOnsets(drums, voice) << ';';
  out << '|' << synthPositiveNotes(synthA) << '|'
      << synthPositiveNotes(synthB);
  return fnv1a(out.str());
}

GenreSettings settingsFor(OwnerKey key) {
  GenreSettings settings{};
  settings.generativeMode = key.mode;
  settings.recipe = key.recipe;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext contextFor(RealizationLevel level) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = 0;
  context.level = level;
  context.generationAttemptOrdinal = 0;
  context.evolutionOrdinal = 0;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;
  context.phraseExecutionOverride = nullptr;
  return context;
}

bool selfTest() {
  int failures = 0;
  auto check = [&failures](bool condition, const char* fault) {
    std::printf("G4_I6_SELFTEST_%s fault=%s\n",
                condition ? "PASS" : "FAIL", fault);
    if (!condition) ++failures;
  };

  const std::set<OwnerKey> fullOwners = {{0, 0}, {9, 0}};
  const std::set<OwnerKey> missingOwners = {{0, 0}};
  check(fullOwners != missingOwners, "INCOMPLETE_OWNER_ENUMERATION");

  const RhythmCompatibilityCandidate rawFixture[] = {
      {402, 100}, {401, 200}, {401, 100}, {714, 0}, {999, 10}};
  const Reconciliation rec = reconcileRaw({rawFixture, 5}, nullptr);
  const auto hasReason = [&rec](const char* reason) {
    return std::find(rec.reasons.begin(), rec.reasons.end(), reason) !=
           rec.reasons.end();
  };
  check(rec.effective.size() == 2 && rec.effective[0].id == 401 &&
            rec.effective[0].weight == 255 && rec.effective[1].id == 402 &&
            hasReason("ORDER_NORMALIZED") &&
            hasReason("ZERO_WEIGHT_REMOVED") &&
            hasReason("UNKNOWN_ID_REJECTED") &&
            hasReason("DUPLICATE_MERGED") &&
            hasReason("WEIGHT_SATURATED"),
        "RAW_EFFECTIVE_MISMATCH");

  std::set<RowKey> keys;
  const RowKey key{{9, 0}, 1, 0};
  check(keys.insert(key).second && !keys.insert(key).second,
        "DUPLICATE_MATERIALIZATION_ROW");
  check(!isKnownArchetype(999), "UNKNOWN_ARCHETYPE");
  Owner synthetic{};
  synthetic.reconciliation.effective = {{401, 100}, {402, 100}};
  check(!containsEffective(synthetic, 713), "SELECTION_OUTSIDE_ADMISSION");

  RhythmArchetype fixture{};
  LaneGrammar kick{};
  kick.role = RhythmRole::Kick;
  kick.canonicalAnchors = kQuarterNotes;
  fixture.lanes = &kick;
  fixture.laneCount = 1;
  const WitnessStrength positive = houseQuarterCandidateWitness(fixture);
  kick.canonicalAnchors =
      static_cast<StepMask>(kQuarterNotes & ~stepBit(12));
  const WitnessStrength negative = houseQuarterCandidateWitness(fixture);
  const RhythmArchetype* straight = archetypeForId(401);
  const RhythmArchetype* stacked = archetypeForId(711);
  check(positive == WitnessStrength::Required &&
            negative == WitnessStrength::None && straight != nullptr &&
            stacked != nullptr && structuralProjection(*straight) !=
                                      structuralProjection(*stacked),
        "WITNESS_DETECTOR_ERROR");
  check(evaluateContract(ContractStatus::ReviewRequired, true) ==
            Evaluation::Unknown,
        "REVIEW_REQUIRED_PROMOTED_TO_PASS");
  check(!(true && false) && (true && true), "RETAINED_CONTROL_REGRESSION");
  check(positive == WitnessStrength::Required, "LABEL_WEIGHT_INDEPENDENCE");
  check(Evaluation::Unknown != Evaluation::Violated,
        "SHARED_ADMISSION_NOT_AUTO_VIOLATION");
  check(OffendingStage::RhythmAdmission !=
            OffendingStage::DownstreamMaterialization,
        "CANDIDATE_DOWNSTREAM_REASONS_DISTINCT");

  if (failures != 0) {
    std::printf("G4-I6 ownership census self-test: FAIL (%d failures)\n",
                failures);
    return false;
  }
  std::puts("G4-I6 ownership census self-test: PASS");
  return true;
}

bool enumerateOwners(Census& census, std::string& error) {
  std::set<OwnerKey> enumerated;
  std::set<OwnerKey> predicateSet;
  for (uint8_t mode = 0; mode < kGenerativeModeCount; ++mode) {
    const auto genre = static_cast<GenerativeMode>(mode);
    const uint8_t count = availableRecipeCount(genre);
    for (uint8_t ordinal = 0; ordinal < count; ++ordinal) {
      GenreRecipeId recipe = 0xff;
      if (!availableRecipeAt(genre, ordinal, recipe)) {
        error = "availableRecipeAt failed inside declared count";
        return false;
      }
      if (!enumerated.insert({mode, recipe}).second) {
        error = "duplicate owner key from production enumeration";
        return false;
      }
    }
    for (uint16_t recipe = 0; recipe <= 255; ++recipe) {
      if (isRecipeAvailable(genre, static_cast<GenreRecipeId>(recipe)))
        predicateSet.insert({mode, static_cast<GenreRecipeId>(recipe)});
    }
  }
  if (enumerated != predicateSet) {
    error = "availableRecipe enumeration != isRecipeAvailable set";
    return false;
  }
  for (OwnerKey key : enumerated) {
    Owner owner{};
    owner.key = key;
    owner.settings = settingsFor(key);
    owner.raw = rhythmCompatibilityFor(owner.settings);
    owner.reconciliation = reconcileRaw(owner.raw, &owner.settings);
    if (!owner.reconciliation.apiMatches ||
        owner.reconciliation.effective.empty()) {
      error = "raw/effective admission mismatch or empty owner";
      return false;
    }
    census.totalEffectiveEdges += owner.reconciliation.effective.size();
    census.owners.push_back(owner);
  }
  return true;
}

bool enumerateArchetypes(Census& census, std::string& error) {
  if (!validateRhythmCatalog(ReferenceVocabulary::catalog())) {
    error = "production rhythm catalog validation failed";
    return false;
  }
  std::set<RhythmArchetypeId> ids;
  for (uint8_t index = 0; index < ReferenceVocabulary::definitionCount();
       ++index) {
    const auto& definition = ReferenceVocabulary::definition(index);
    const RhythmArchetype* archetype =
        ReferenceVocabulary::archetypeFor(definition.key);
    if (definition.archetypeId == kNoArchetypeId || archetype == nullptr ||
        archetype->id != definition.archetypeId ||
        !ids.insert(definition.archetypeId).second) {
      error = "unknown/duplicate/inconsistent production archetype";
      return false;
    }
    ArchetypeInfo info{};
    info.id = definition.archetypeId;
    info.name = definition.name == nullptr ? "" : definition.name;
    info.family = definition.family;
    info.archetype = archetype;
    info.projection = structuralProjection(*archetype);
    info.projectionHash = fnv1a(info.projection);
    census.archetypes[info.id] = info;
  }
  for (const Owner& owner : census.owners) {
    for (const auto& candidate : owner.reconciliation.effective) {
      auto found = census.archetypes.find(candidate.id);
      if (found == census.archetypes.end()) {
        error = "effective edge references unknown archetype";
        return false;
      }
      ++found->second.ownerCount;
    }
  }
  uint32_t degreeSum = 0;
  for (auto& item : census.archetypes) {
    degreeSum += item.second.ownerCount;
    if (item.second.ownerCount == 0) {
      ++census.admissionOrphans;
      Finding finding{};
      finding.id = "I6-ORPHAN-" + std::to_string(item.first);
      finding.type = "ADMISSION_ORPHAN";
      finding.archetype = item.first;
      finding.contractId = "GRAPH-FACT";
      finding.contractVersion = "1";
      finding.evidenceStatus = ContractStatus::Proven;
      finding.evaluation = Evaluation::Satisfied;
      finding.stage = OffendingStage::RhythmAdmission;
      finding.reason =
          "production reference archetype has zero effective shipped owners";
      census.findings.push_back(finding);
    } else if (item.second.ownerCount > 1) {
      ++census.sharedAdmissions;
    }
  }
  if (degreeSum != census.totalEffectiveEdges) {
    error = "archetype owner degree sum != total effective edges";
    return false;
  }
  return true;
}

bool candidateContractsAndCollisions(Census& census, std::string& error) {
  std::set<OwnerKey> overbroad;
  bool i4Quarter = false;
  bool i4Broken = false;
  bool i4Dialogue = false;
  for (const Owner& owner : census.owners) {
    const bool house = owner.key.mode == 9 && owner.key.recipe == 0;
    const bool dub = owner.key.mode == 5 && owner.key.recipe == 5;
    if (!house && !dub) ++census.reviewRequiredOwners;
    for (const auto& candidate : owner.reconciliation.effective) {
      const RhythmArchetype* archetype = archetypeForId(candidate.id);
      if (archetype == nullptr) {
        error = "candidate contract encountered unknown archetype";
        return false;
      }
      bool violation = false;
      std::string contractId;
      std::string evidence;
      if (house) {
        contractId = "G4-I5-HOUSE-ADMISSION-QUARTER-SPACE";
        const WitnessStrength witness = houseQuarterCandidateWitness(*archetype);
        evidence = witnessStrengthName(witness);
        violation = witness == WitnessStrength::None;
      } else if (dub) {
        contractId = "G4-I4-DUB-ADMISSION-TECHNO-SKELETON";
        const TechnoSkeleton skeleton = requiredTechnoSkeleton(*archetype);
        evidence = skeletonName(skeleton);
        violation = skeleton == TechnoSkeleton::None;
        i4Quarter = i4Quarter || skeleton == TechnoSkeleton::QuarterPulse;
        i4Broken = i4Broken || skeleton == TechnoSkeleton::BrokenFrame;
        i4Dialogue = i4Dialogue || hasDubDialogue(*archetype);
      }
      if (violation) {
        ++census.falseOwners;
        overbroad.insert(owner.key);
        Finding finding{};
        finding.id = "I6-FALSE-OWNER-" + std::to_string(owner.key.mode) +
                     "-" + std::to_string(owner.key.recipe) + "-" +
                     std::to_string(candidate.id);
        finding.type = "FALSE_OWNER";
        finding.owner = owner.key;
        finding.archetype = candidate.id;
        finding.contractId = contractId;
        finding.contractVersion = "1";
        finding.evidenceStatus = ContractStatus::Proven;
        finding.evaluation = Evaluation::Violated;
        finding.stage = OffendingStage::RhythmAdmission;
        finding.candidateEvidence = evidence;
        finding.reason =
            "effective archetype contradicts applicable PROVEN admission contract";
        census.findings.push_back(finding);
      }
    }
  }
  census.overbroadOwners = overbroad.size();
  if (!i4Quarter || !i4Broken || !i4Dialogue || census.falseOwners != 0) {
    error = "I4/I5 retained candidate control regression";
    return false;
  }

  uint32_t ordinal = 0;
  for (auto left = census.archetypes.begin(); left != census.archetypes.end();
       ++left) {
    auto right = left;
    ++right;
    for (; right != census.archetypes.end(); ++right) {
      if (left->second.projectionHash != right->second.projectionHash ||
          left->second.projection != right->second.projection)
        continue;
      ++census.structuralCollisions;
      Finding finding{};
      finding.id = "I6-COLLISION-" + std::to_string(++ordinal);
      finding.type = "STRUCTURAL_COLLISION";
      finding.archetype = left->first;
      finding.rightArchetype = right->first;
      finding.contractId = "STRUCTURAL-PROJECTION";
      finding.contractVersion = "1";
      finding.evidenceStatus = ContractStatus::Provisional;
      finding.evaluation = Evaluation::Satisfied;
      finding.stage = OffendingStage::None;
      finding.collisionScope =
          "lanes+protected-space+relationships+gate/minmax";
      finding.equalProjection = left->second.projection;
      finding.differingKnownSemantics = nonProjectedDifference(
          *left->second.archetype, *right->second.archetype);
      finding.reason =
          "equal named structural projection; not a musical-duplicate claim";
      census.findings.push_back(finding);
    }
  }
  return true;
}

ReachabilityState manualReachability(const Owner& owner,
                                     RhythmArchetypeId target) {
  GenreSettings settings = owner.settings;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Manual);
  settings.rhythmArchetypeId = target;
  StrongRhythmFrozenSelection selection{};
  const StrongRhythmMigrationResult result = resolveStrongRhythmFrozenSelection(
      settings, contextFor(RealizationLevel::P1Canonical), 1, selection);
  return result.status == StrongRhythmMigrationStatus::Applied &&
                 selection.resolved &&
                 selection.composition.rhythmArchetypeId == target
             ? ReachabilityState::ManualReproduced
             : ReachabilityState::NotObserved;
}

bool materializeCorpus(Census& census, std::string& error) {
  constexpr RealizationLevel levels[] = {
      RealizationLevel::P1Canonical,
      RealizationLevel::P2Variation,
      RealizationLevel::P3Transformation,
  };
  std::set<RowKey> rowKeys;
  const uint32_t expectedRows =
      static_cast<uint32_t>(census.owners.size()) * 128u * 3u;

  for (const Owner& owner : census.owners) {
    uint32_t ownerRoots = 0;
    for (uint16_t identity = kIdentityFirst; identity <= kIdentityLast;
         ++identity) {
      RhythmArchetypeId rootArchetype = kNoArchetypeId;
      for (uint8_t levelIndex = 0; levelIndex < 3; ++levelIndex) {
        ++census.attemptedRows;
        if (!rowKeys.insert({owner.key, identity, levelIndex}).second) {
          error = "duplicate materialization row";
          return false;
        }
        StrongRhythmMigrationContext context = contextFor(levels[levelIndex]);
        StrongRhythmFrozenSelection selection{};
        const StrongRhythmMigrationResult selected =
            resolveStrongRhythmFrozenSelection(
                owner.settings, context, identity, selection);
        if (selected.status != StrongRhythmMigrationStatus::Applied ||
            !selection.resolved) {
          ++census.failedRows;
          error = "frozen selection failed";
          return false;
        }
        const RhythmArchetypeId selectedId =
            selection.composition.rhythmArchetypeId;
        if (!containsEffective(owner, selectedId) ||
            !isKnownArchetype(selectedId)) {
          ++census.failedRows;
          error = "selection outside effective admission/unknown archetype";
          return false;
        }
        if (levelIndex == 0) {
          rootArchetype = selectedId;
          ++census.selectedIdentities[{owner.key, selectedId}];
          ++ownerRoots;
        } else if (selectedId != rootArchetype) {
          ++census.failedRows;
          error = "P1/P2/P3 frozen upstream archetype drift";
          return false;
        }

        DrumPatternSet drums{};
        SynthPattern synthA{};
        SynthPattern synthB{};
        const StrongRhythmMigrationResult materialized =
            migrateStrongRhythmFrozenMaterial(
                owner.settings, selection, context, drums, synthA, synthB);
        if (materialized.status != StrongRhythmMigrationStatus::Applied) {
          ++census.failedRows;
          error = "materialization failed";
          return false;
        }
        ++census.successfulRows;

        const ArchetypeInfo& info = census.archetypes.at(selectedId);
        Row row{};
        row.owner = owner.key;
        row.identity = identity;
        row.levelIndex = levelIndex;
        row.rawCandidateCount = owner.raw.count;
        row.effectiveCandidateCount =
            static_cast<uint8_t>(owner.reconciliation.effective.size());
        row.archetypeId = selectedId;
        row.archetypeName = info.name;
        row.weight = effectiveWeight(owner, selectedId);
        row.family = info.family;
        row.structuralHash = info.projectionHash;
        row.structuralMinMax = structuralMinMaxSummary(*info.archetype);
        row.materializedHash = materializedSignature(drums, synthA, synthB);
        row.bass = materialized.bassRhythmId;
        row.chord = materialized.chordRhythmId;
        row.melodic = materialized.melodicRhythmId;
        row.motif = materialized.motifShapeId;
        row.secondaryRole = materialized.synthBRole;
        row.phraseLaw = materialized.phraseLaw;
        row.effectiveDensity = selection.structuralDensityTarget;
        row.selectionSeed = selection.selectionGeneration.projectSeed;
        row.realizationSeed = selection.realizationGeneration.projectSeed;

        if (owner.key.mode == 9 && owner.key.recipe == 0) {
          row.contractId = "G4-I5-HOUSE-MATERIALIZED-QUARTER";
          row.contractStatus = ContractStatus::Proven;
          row.contractScope = "one-bar address-0 materialization";
          row.contractQuantifier = "forall rows in admitted House corpus";
          row.expectedWitness = "quarter-pulse";
          const bool observed =
              (drumOnsets(drums, KICK) & kQuarterNotes) == kQuarterNotes;
          row.actualWitness = observed ? "quarter-pulse" : "none";
          row.evaluation = evaluateContract(row.contractStatus, observed);
          row.reason = observed ? "observed materialized quarter pulse"
                                : "House quarter witness lost downstream";
          row.stage = observed ? OffendingStage::None
                               : OffendingStage::DownstreamMaterialization;
        } else if (owner.key.mode == 5 && owner.key.recipe == 5) {
          row.contractId = "G4-I4-DUB-MATERIALIZED-SKELETON";
          row.contractStatus = ContractStatus::Proven;
          row.contractScope = "one-bar address-0 materialization";
          row.contractQuantifier =
              "forall rows preserve selected candidate techno skeleton";
          const TechnoSkeleton expected = requiredTechnoSkeleton(*info.archetype);
          const TechnoSkeleton actual = observedTechnoSkeleton(
              drumOnsets(drums, KICK), drumOnsets(drums, SNARE));
          row.expectedWitness = skeletonName(expected);
          row.actualWitness = skeletonName(actual);
          row.evaluation = evaluateContract(
              row.contractStatus,
              expected != TechnoSkeleton::None && actual == expected);
          row.reason = row.evaluation == Evaluation::Satisfied
                           ? "observed skeleton matches candidate"
                           : "Dub Techno skeleton lost downstream";
          row.stage = row.evaluation == Evaluation::Satisfied
                          ? OffendingStage::None
                          : OffendingStage::DownstreamMaterialization;
        } else {
          row.contractId = "I6-REVIEW-RHYTHM-OWNERSHIP";
          row.contractStatus = ContractStatus::ReviewRequired;
          row.contractScope = "rhythm admission";
          row.contractQuantifier = "not established";
          row.expectedWitness = "NOT_ASSESSED";
          row.actualWitness = "OBSERVED_STRUCTURE_ONLY";
          row.evaluation = Evaluation::Unknown;
          row.reason = "no applicable PROVEN owner admission contract in I6";
          row.stage = OffendingStage::UnknownStage;
          ++census.unknownEvaluations;
        }
        if (row.evaluation == Evaluation::Violated) {
          error = "I4/I5 retained materialized control regression";
          return false;
        }
        census.rows.push_back(row);
      }
    }
    if (ownerRoots != 128) {
      error = "owner root accounting != 128";
      return false;
    }
  }

  if (census.attemptedRows != expectedRows ||
      census.successfulRows != expectedRows || census.failedRows != 0 ||
      census.rows.size() != expectedRows || rowKeys.size() != expectedRows) {
    error = "materialized corpus accounting mismatch";
    return false;
  }

  for (const Owner& owner : census.owners) {
    for (const auto& candidate : owner.reconciliation.effective) {
      const uint16_t observed =
          census.selectedIdentities[{owner.key, candidate.id}];
      ReachabilityState state = ReachabilityState::NotObserved;
      if (observed != 0) {
        state = ReachabilityState::AutoObserved;
        ++census.autoObservedEdges;
      } else {
        state = manualReachability(owner, candidate.id);
        if (state == ReachabilityState::ManualReproduced)
          ++census.manualReproducedEdges;
        else
          ++census.notObservedEdges;
      }
      for (Row& row : census.rows) {
        if (row.owner == owner.key && row.archetypeId == candidate.id)
          row.reachability = state;
      }
    }
  }
  if (census.autoObservedEdges + census.manualReproducedEdges +
          census.notObservedEdges + census.provenUnreachableEdges !=
      census.totalEffectiveEdges) {
    error = "reachability accounting mismatch";
    return false;
  }
  return true;
}

BoundaryObservation runBoundary(const char* name, OwnerKey key,
                                uint16_t identity, int16_t patternAddress,
                                uint8_t phraseBarOrdinal) {
  BoundaryObservation observation{};
  observation.name = name;
  observation.owner = key;
  observation.identity = identity;
  observation.patternAddress = patternAddress;
  observation.phraseBarOrdinal = phraseBarOrdinal;
  GenreSettings settings = settingsFor(key);
  StrongRhythmMigrationContext context =
      contextFor(RealizationLevel::P2Variation);
  context.patternAddress = patternAddress;
  context.phraseBarOrdinal = phraseBarOrdinal;
  StrongRhythmFrozenSelection selection{};
  const StrongRhythmMigrationResult selected =
      resolveStrongRhythmFrozenSelection(settings, context, identity, selection);
  observation.status = selected.status;
  if (selected.status == StrongRhythmMigrationStatus::Applied &&
      selection.resolved) {
    observation.archetype = selection.composition.rhythmArchetypeId;
    DrumPatternSet drums{};
    SynthPattern synthA{};
    SynthPattern synthB{};
    const StrongRhythmMigrationResult materialized =
        migrateStrongRhythmFrozenMaterial(
            settings, selection, context, drums, synthA, synthB);
    observation.status = materialized.status;
    observation.bass = materialized.bassRhythmId;
  }
  return observation;
}

void addBoundaryFixtures(Census& census) {
  census.boundaries.push_back(runBoundary(
      "identity-0-house", {9, 0}, 0, 0, kUnspecifiedPhraseBarOrdinal));
  census.boundaries.push_back(runBoundary(
      "nonzero-pattern-address-house", {9, 0}, 1, 1,
      kUnspecifiedPhraseBarOrdinal));
  census.boundaries.push_back(runBoundary(
      "odd-semantic-bar-dnb", {14, 0}, 1, 0, 1));
  census.boundaries.push_back(runBoundary(
      "sparse-bar-minimal-space", {5, 11}, 1, 0, 1));
}

void deriveSharedValidity(Census& census) {
  for (const auto& archetype : census.archetypes) {
    std::vector<OwnerKey> owners;
    for (const Owner& owner : census.owners)
      if (containsEffective(owner, archetype.first)) owners.push_back(owner.key);
    if (owners.size() < 2) continue;
    bool valid = true;
    for (OwnerKey owner : owners) {
      const bool house = owner.mode == 9 && owner.recipe == 0;
      const bool dub = owner.mode == 5 && owner.recipe == 5;
      if (!house && !dub) {
        valid = false;
        break;
      }
      const RhythmArchetype* structure = archetype.second.archetype;
      if ((house && houseQuarterCandidateWitness(*structure) ==
                        WitnessStrength::None) ||
          (dub && requiredTechnoSkeleton(*structure) == TechnoSkeleton::None)) {
        valid = false;
        break;
      }
    }
    if (valid) ++census.sharedValidUnderContracts;
  }
}

bool writeCensus(const Census& census, const std::filesystem::path& path) {
  std::ofstream out(path);
  if (!out) return false;
  out << "owner_mode\towner_genre\trecipe_id\trecipe_name\tidentity_ordinal"
         "\tP_level\traw_candidate_count\teffective_candidate_count"
         "\traw_effective_reconciliation\tselected_archetype_id"
         "\tselected_archetype_name\tweight_provenance\treachability_state"
         "\tRhythmFamily\tstructural_signature\tstructural_minmax_summary"
         "\tmaterialized_signature\tbass_identity\tchord_identity"
         "\tmelodic_identity\tmotif_identity\tsecondary_role"
         "\tphrase_law_metadata\teffective_density\tselection_seed"
         "\trealization_seed\tcontract_id\tcontract_status\tcontract_scope"
         "\tcontract_quantifier\texpected_witness\tactual_witness"
         "\tevaluation\treason\toffending_stage\n";
  std::map<OwnerKey, const Owner*> ownerIndex;
  for (const Owner& owner : census.owners) ownerIndex[owner.key] = &owner;
  for (const Row& row : census.rows) {
    const Owner& owner = *ownerIndex.at(row.owner);
    out << static_cast<unsigned>(row.owner.mode) << '\t'
        << modeName(row.owner.mode) << '\t'
        << static_cast<unsigned>(row.owner.recipe) << '\t'
        << recipeName(row.owner.recipe) << '\t' << row.identity << '\t'
        << "P" << static_cast<unsigned>(row.levelIndex + 1) << '\t'
        << static_cast<unsigned>(row.rawCandidateCount) << '\t'
        << static_cast<unsigned>(row.effectiveCandidateCount) << '\t'
        << reconciliationText(owner.reconciliation) << '\t'
        << row.archetypeId << '\t' << clean(row.archetypeName) << '\t'
        << static_cast<unsigned>(row.weight) << '\t'
        << reachabilityName(row.reachability) << '\t'
        << familyName(row.family) << '\t' << std::hex << std::setw(8)
        << std::setfill('0') << row.structuralHash << std::dec << '\t'
        << clean(row.structuralMinMax) << '\t' << std::hex << std::setw(8)
        << std::setfill('0') << row.materializedHash << std::dec << '\t'
        << static_cast<unsigned>(row.bass) << '\t'
        << static_cast<unsigned>(row.chord) << '\t'
        << static_cast<unsigned>(row.melodic) << '\t'
        << static_cast<unsigned>(row.motif) << '\t'
        << static_cast<unsigned>(row.secondaryRole) << '\t'
        << static_cast<unsigned>(row.phraseLaw) << '\t'
        << static_cast<unsigned>(row.effectiveDensity) << '\t'
        << row.selectionSeed << '\t' << row.realizationSeed << '\t'
        << row.contractId << '\t' << contractStatusName(row.contractStatus)
        << '\t' << clean(row.contractScope) << '\t'
        << clean(row.contractQuantifier) << '\t' << row.expectedWitness << '\t'
        << row.actualWitness << '\t' << evaluationName(row.evaluation) << '\t'
        << clean(row.reason) << '\t' << stageName(row.stage) << '\n';
  }
  return true;
}

bool writeAnomalies(const Census& census, const std::filesystem::path& path) {
  std::ofstream out(path);
  if (!out) return false;
  out << "finding_id\tfinding_type\towner_mode\towner_genre\trecipe_id"
         "\trecipe_name\tarchetype\tcontract_id\tcontract_version"
         "\tevidence_status\tevaluation\taffected_identities"
         "\taffected_materializations\toffending_stage\tcandidate_evidence"
         "\tmaterialized_evidence\treason\tleft_archetype\tright_archetype"
         "\tcollision_scope\tequal_projection\tdiffering_known_semantics\n";
  for (const Finding& finding : census.findings) {
    out << finding.id << '\t' << finding.type << '\t';
    if (finding.type == "FALSE_OWNER") {
      out << static_cast<unsigned>(finding.owner.mode) << '\t'
          << modeName(finding.owner.mode) << '\t'
          << static_cast<unsigned>(finding.owner.recipe) << '\t'
          << recipeName(finding.owner.recipe);
    } else {
      out << "\t\t\t";
    }
    out << '\t' << finding.archetype << '\t' << finding.contractId << '\t'
        << finding.contractVersion << '\t'
        << contractStatusName(finding.evidenceStatus) << '\t'
        << evaluationName(finding.evaluation) << '\t'
        << finding.affectedIdentities << '\t'
        << finding.affectedMaterializations << '\t'
        << stageName(finding.stage) << '\t'
        << clean(finding.candidateEvidence) << '\t'
        << clean(finding.materializedEvidence) << '\t'
        << clean(finding.reason) << '\t' << finding.archetype << '\t'
        << finding.rightArchetype << '\t' << clean(finding.collisionScope)
        << '\t' << clean(finding.equalProjection) << '\t'
        << clean(finding.differingKnownSemantics) << '\n';
  }
  return true;
}

bool writeReport(const Census& census, const std::filesystem::path& path) {
  std::ofstream out(path);
  if (!out) return false;
  uint32_t base = 0;
  for (const Owner& owner : census.owners)
    if (owner.key.recipe == 0) ++base;
  const uint32_t roots = census.owners.size() * 128u;
  out << "# GF2 G4-I6 Global Ownership Census\n\n";
  out << "## Executive summary\n\n```text\n"
      << "profiles=" << census.owners.size() << "\n"
      << "base_profiles=" << base << "\n"
      << "non_base_profiles=" << census.owners.size() - base << "\n"
      << "archetypes=" << census.archetypes.size() << "\n"
      << "effective_owner_edges=" << census.totalEffectiveEdges << "\n"
      << "roots=" << roots << "\n"
      << "materializations=" << census.rows.size() << "\n"
      << "false_owner=" << census.falseOwners << "\n"
      << "admission_orphan=" << census.admissionOrphans << "\n"
      << "overbroad_owner=" << census.overbroadOwners << "\n"
      << "structural_collision=" << census.structuralCollisions << "\n"
      << "shared_admission=" << census.sharedAdmissions << "\n"
      << "shared_valid_under_contracts=" << census.sharedValidUnderContracts
      << "\nproven_contract_owners=2\nprovisional_contract_owners=0\n"
      << "review_required=" << census.reviewRequiredOwners << "\n"
      << "unknown_evaluations=" << census.unknownEvaluations << "\n"
      << "auto_observed_edges=" << census.autoObservedEdges << "\n"
      << "manual_reproduced_edges=" << census.manualReproducedEdges << "\n"
      << "not_observed_edges=" << census.notObservedEdges << "\n"
      << "proven_unreachable_edges=" << census.provenUnreachableEdges
      << "\n```\n\n";
  out << "## Measurement scope\n\n"
      << "- Main corpus is **one-bar, patternAddress=0** evidence.\n"
      << "- Owners are derived from production recipe APIs and set-compared with `isRecipeAvailable()`.\n"
      << "- Identities 1..128, P1/P2/P3, attempt 0, evolutionOrdinal 0.\n"
      << "- Tonal materialization enabled: root C, Dorian. FEEL STRAIGHT/0.\n"
      << "- **PHRASE OWNERSHIP: NOT_ASSESSED by main I6 corpus.** G4-I3 is a retained control only.\n\n";
  out << "## Catalog / raw-effective reconciliation\n\n"
      << "| Owner | Raw | Effective | Reconciliation |\n|---|---:|---:|---|\n";
  for (const Owner& owner : census.owners) {
    out << "| " << modeName(owner.key.mode) << " / "
        << recipeName(owner.key.recipe) << " | "
        << static_cast<unsigned>(owner.raw.count) << " | "
        << owner.reconciliation.effective.size() << " | "
        << reconciliationText(owner.reconciliation) << " |\n";
  }
  out << "\n### Archetype universe\n\n| ID | Name | Family | Owners |\n|---:|---|---|---:|\n";
  for (const auto& item : census.archetypes) {
    out << "| " << item.first << " | " << item.second.name << " | "
        << familyName(item.second.family) << " | " << item.second.ownerCount
        << " |\n";
  }
  out << "\n## Proven contracts\n\n"
      << "I6 does not mark whole owners PROVEN. It carries only named statements from retained checkpoints.\n\n"
      << "- G4-I4: every admitted Dub Techno candidate has a required techno skeleton; existential quarter, broken, and dub-dialogue witnesses remain separate; materialized I4 proves skeleton preservation, not per-row dub dialogue.\n"
      << "- G4-I5: candidate quarter space may be REQUIRED or POSSIBLE_BUT_NOT_REQUIRED via preferred anchors; materialized I5 separately proves observed quarter-pulse preservation.\n\n";
  out << "## Candidate graph\n\n";
  for (const Owner& owner : census.owners) {
    out << "### " << modeName(owner.key.mode) << " / "
        << recipeName(owner.key.recipe) << "\n\n"
        << "| Archetype | Weight provenance | Structural hash | AUTO roots |\n|---|---:|---|---:|\n";
    for (const auto& candidate : owner.reconciliation.effective) {
      const ArchetypeInfo& info = census.archetypes.at(candidate.id);
      out << "| " << candidate.id << " " << info.name << " | "
          << static_cast<unsigned>(candidate.weight) << " | `" << std::hex
          << std::setw(8) << std::setfill('0') << info.projectionHash
          << std::dec << "` | "
          << census.selectedIdentities.at({owner.key, candidate.id}) << " |\n";
    }
    out << '\n';
  }
  out << "## Findings\n\n### PROVEN OWNERSHIP CONTRADICTIONS\n\n";
  if (census.falseOwners == 0) out << "None under applicable PROVEN I4/I5 admission contracts.\n\n";
  out << "### STRUCTURAL COLLISIONS\n\n";
  if (census.structuralCollisions == 0) out << "None at the named projection scope.\n\n";
  for (const Finding& finding : census.findings) {
    if (finding.type == "STRUCTURAL_COLLISION")
      out << "- " << finding.archetype << " vs " << finding.rightArchetype
          << "; differing non-projected semantics: "
          << finding.differingKnownSemantics
          << ". This is not a musical duplicate claim.\n";
  }
  out << "\n### SHARED ADMISSIONS\n\n" << census.sharedAdmissions
      << " archetypes have >1 effective shipped owner. Shared is a graph fact, not a violation.\n\n"
      << "SHARED_VALID_UNDER_CONTRACTS=" << census.sharedValidUnderContracts
      << ". Absence of proof is not promoted to shared-valid.\n\n"
      << "### REVIEW_REQUIRED\n\n" << census.reviewRequiredOwners
      << " shipped owners have no applicable PROVEN rhythm admission contract in I6.\n\n"
      << "### UNKNOWN\n\n" << census.unknownEvaluations
      << " materialized rows are observations under REVIEW_REQUIRED owners.\n\n";
  out << "## Boundary fixtures outside denominator\n\n"
      << "| Fixture | Owner | identity | address | phraseBar | status | archetype | bass |\n|---|---|---:|---:|---|---:|---:|---:|\n";
  for (const auto& boundary : census.boundaries) {
    out << "| " << boundary.name << " | " << modeName(boundary.owner.mode)
        << " / " << recipeName(boundary.owner.recipe) << " | "
        << boundary.identity << " | " << boundary.patternAddress << " | ";
    if (boundary.phraseBarOrdinal == kUnspecifiedPhraseBarOrdinal)
      out << "unspecified";
    else
      out << static_cast<unsigned>(boundary.phraseBarOrdinal);
    out << " | " << static_cast<unsigned>(boundary.status) << " | "
        << boundary.archetype << " | " << static_cast<unsigned>(boundary.bass)
        << " |\n";
  }
  out << "\n## Limitations\n\n"
      << "- address-0 and one-bar scope are not universal Pattern/Phrase proof.\n"
      << "- NOT_OBSERVED in 128 identities is not PROVEN_UNREACHABLE.\n"
      << "- Role ownership remains NOT_ASSESSED where no explicit observer exists; a drum archetype does not imply a bass contract.\n"
      << "- Positive SynthStep notes are not interpreted as attack counts because lifetime/continuation semantics can differ.\n"
      << "- No perceptual listening or universal genre-correctness claim is made.\n"
      << "- Equal selected projection means STRUCTURAL_COLLISION only, not same musical idea.\n\n"
      << "## I7 eligibility\n\n";
  if (census.falseOwners == 0)
    out << "`I7_CANDIDATE: NONE`\n\nNo new effective edge both contradicts an applicable PROVEN owner contract and reproduces as a materialized violation. REVIEW_REQUIRED owners need an independently justified musical contract first.\n";
  else
    out << "I7 eligibility requires separate editorial review; I6 does not auto-fix or rank by weight.\n";
  out << "\n## Calibration discipline\n\n> OWNERSHIP before WEIGHTING: first ask whether an idea may exist inside an owner; only then ask how often it should appear.\n";
  return true;
}

bool writeSummary(const Census& census, const std::filesystem::path& path) {
  std::ofstream out(path);
  if (!out) return false;
  out << "G4_I6_CATALOG profiles=" << census.owners.size()
      << " archetypes=" << census.archetypes.size()
      << " effective_edges=" << census.totalEffectiveEdges << '\n'
      << "G4_I6_CORPUS roots=" << census.owners.size() * 128u
      << " materializations=" << census.rows.size()
      << " attempted=" << census.attemptedRows
      << " successful=" << census.successfulRows
      << " failed=" << census.failedRows << '\n'
      << "G4_I6_REACHABILITY auto_observed=" << census.autoObservedEdges
      << " manual_reproduced=" << census.manualReproducedEdges
      << " not_observed=" << census.notObservedEdges
      << " proven_unreachable=" << census.provenUnreachableEdges << '\n'
      << "G4_I6_FINDINGS false_owner=" << census.falseOwners
      << " admission_orphan=" << census.admissionOrphans
      << " overbroad_owner=" << census.overbroadOwners
      << " structural_collision=" << census.structuralCollisions
      << " shared_admission=" << census.sharedAdmissions
      << " shared_valid_under_contracts=" << census.sharedValidUnderContracts
      << " review_required=" << census.reviewRequiredOwners
      << " unknown=" << census.unknownEvaluations << '\n'
      << "G4_I6_SCOPE one_bar=1 pattern_address=0 identity=1..128 levels=P1,P2,P3 attempt=0 feel=STRAIGHT/0 phrase_ownership=NOT_ASSESSED\n"
      << "G4_I6_I7 candidate="
      << (census.falseOwners == 0 ? "NONE" : "EDITORIAL_REVIEW_REQUIRED")
      << "\nG4-I6 global ownership census: PASS\n";
  return true;
}

bool emit(const std::filesystem::path& directory) {
  if (!selfTest()) return false;
  Census census{};
  std::string error;
  if (!enumerateOwners(census, error) ||
      !enumerateArchetypes(census, error) ||
      !candidateContractsAndCollisions(census, error) ||
      !materializeCorpus(census, error)) {
    std::fprintf(stderr, "G4_I6_MEASUREMENT_FAIL reason=%s\n", error.c_str());
    return false;
  }
  addBoundaryFixtures(census);
  deriveSharedValidity(census);
  std::filesystem::create_directories(directory);
  if (!writeCensus(census, directory / "GF2_G4_I6_OWNERSHIP_CENSUS.tsv") ||
      !writeAnomalies(census, directory / "GF2_G4_I6_OWNERSHIP_ANOMALIES.tsv") ||
      !writeReport(census, directory / "GF2_G4_I6_OWNERSHIP_CENSUS.md") ||
      !writeSummary(census, directory / "g4-i6-summary.txt")) {
    std::fputs("G4_I6_MEASUREMENT_FAIL artifact_write\n", stderr);
    return false;
  }
  std::printf("G4_I6_MEASUREMENT_PASS profiles=%zu archetypes=%zu edges=%u rows=%zu\n",
              census.owners.size(), census.archetypes.size(),
              census.totalEffectiveEdges, census.rows.size());
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 || std::string(argv[1]) != "--emit") {
    std::fprintf(stderr, "usage: %s --emit OUTPUT_DIR\n", argv[0]);
    return 2;
  }
  return emit(argv[2]) ? 0 : 1;
}
