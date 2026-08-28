#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <unordered_set>

#include "scenes.h"
#include "src/generation/composition/tonal_profile.h"
#include "src/generation/migration/phrase_execution.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/rhythm/rhythm_realizer.h"
#include "src/generation/roles/bass_rhythm.h"
#include "src/generation/roles/chord_rhythm.h"
#include "src/generation/roles/melodic_motif.h"
#include "src/generation/roles/melodic_pitch_intent.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint32_t kIdentityDomain =
    static_cast<uint32_t>(kUnspecifiedPhraseGenerationIdentity);
constexpr uint8_t kRecipeDomain =
    static_cast<uint8_t>(kDustyJazzRecipeId + 1u);
constexpr uint8_t kLengthDomain[] = {1, 2, 4, 8};
constexpr StepMask kLaterThanStepZeroMask =
    static_cast<StepMask>(kAllSteps & ~stepBit(0));

static_assert(kIdentityDomain == 65535u,
              "C2-C0 corpus assumes only 0xFFFF is the identity sentinel");
static_assert(kRecipeDomain == 18u,
              "C2-C0 recipe domain must track append-only production IDs");

struct BarObservation {
  bool valid = false;
  bool allowSparse = false;
  RhythmFamily family = RhythmFamily::FourFloor;
  SemanticSynthBRole role = SemanticSynthBRole::Chord;
  BassRhythmStatus bassStatus = BassRhythmStatus::InvalidRequest;
  ChordRhythmStatus chordStatus = ChordRhythmStatus::InvalidRequest;
  MelodicRhythmId melodicRhythm = MelodicRhythmId::Auto;
  MotifShapeId motifShape = MotifShapeId::Auto;
  MelodicMotifStatus melodicStatus = MelodicMotifStatus::InvalidRequest;
  MelodicPitchIntentStatus pitchStatus =
      MelodicPitchIntentStatus::InvalidRequest;
  StepMask kickOnsets = 0;
  StepMask bassOnsets = 0;
  StepMask chordOnsets = 0;
  StepMask chordContinuations = 0;
  StepMask protectedMelodic = 0;
  StepMask admittedOnsets = 0;
  StepMask admittedContinuations = 0;
};

struct Origin {
  GenreSettings settings{};
  uint8_t profileRecipe = 0;
  uint8_t phraseBars = 0;
  uint16_t identity = 0;
  uint8_t boundary = 0;
};

struct BarSignature {
  uint8_t allowSparse = 0;
  uint8_t family = 0;
  uint8_t role = 0;
  uint8_t bassStatus = 0;
  uint8_t chordStatus = 0;
  uint8_t melodicRhythm = 0;
  uint8_t motifShape = 0;
  uint8_t melodicStatus = 0;
  uint8_t pitchStatus = 0;
  StepMask kickOnsets = 0;
  StepMask bassOnsets = 0;
  StepMask chordOnsets = 0;
  StepMask chordContinuations = 0;
  StepMask protectedMelodic = 0;
  StepMask admittedOnsets = 0;
  StepMask admittedContinuations = 0;

  bool operator==(const BarSignature& other) const {
    return allowSparse == other.allowSparse &&
           family == other.family && role == other.role &&
           bassStatus == other.bassStatus && chordStatus == other.chordStatus &&
           melodicRhythm == other.melodicRhythm &&
           motifShape == other.motifShape &&
           melodicStatus == other.melodicStatus &&
           pitchStatus == other.pitchStatus &&
           kickOnsets == other.kickOnsets && bassOnsets == other.bassOnsets &&
           chordOnsets == other.chordOnsets &&
           chordContinuations == other.chordContinuations &&
           protectedMelodic == other.protectedMelodic &&
           admittedOnsets == other.admittedOnsets &&
           admittedContinuations == other.admittedContinuations;
  }
};

struct BoundarySignature {
  uint8_t phraseBars = 0;
  uint8_t boundary = 0;
  uint8_t evolutionSeam = 0;
  uint16_t archetypeId = 0;
  BarSignature outgoing{};
  BarSignature incoming{};

  bool operator==(const BoundarySignature& other) const {
    return phraseBars == other.phraseBars &&
           boundary == other.boundary &&
           evolutionSeam == other.evolutionSeam &&
           archetypeId == other.archetypeId &&
           outgoing == other.outgoing && incoming == other.incoming;
  }
};

struct TerminalSignature {
  uint8_t phraseBars = 0;
  uint16_t archetypeId = 0;
  BarSignature terminal{};

  bool operator==(const TerminalSignature& other) const {
    return phraseBars == other.phraseBars &&
           archetypeId == other.archetypeId && terminal == other.terminal;
  }
};

inline void hashMix(size_t& seed, uint64_t value) {
  seed ^= static_cast<size_t>(value + 0x9e3779b97f4a7c15ULL +
                              (static_cast<uint64_t>(seed) << 6u) +
                              (static_cast<uint64_t>(seed) >> 2u));
}

struct BarSignatureHash {
  size_t operator()(const BarSignature& value) const {
    size_t seed = 0;
    hashMix(seed, value.allowSparse);
    hashMix(seed, value.family);
    hashMix(seed, value.role);
    hashMix(seed, value.bassStatus);
    hashMix(seed, value.chordStatus);
    hashMix(seed, value.melodicRhythm);
    hashMix(seed, value.motifShape);
    hashMix(seed, value.melodicStatus);
    hashMix(seed, value.pitchStatus);
    hashMix(seed, value.kickOnsets);
    hashMix(seed, value.bassOnsets);
    hashMix(seed, value.chordOnsets);
    hashMix(seed, value.chordContinuations);
    hashMix(seed, value.protectedMelodic);
    hashMix(seed, value.admittedOnsets);
    hashMix(seed, value.admittedContinuations);
    return seed;
  }
};

struct BoundarySignatureHash {
  size_t operator()(const BoundarySignature& value) const {
    size_t seed = 0;
    hashMix(seed, value.phraseBars);
    hashMix(seed, value.boundary);
    hashMix(seed, value.evolutionSeam);
    hashMix(seed, value.archetypeId);
    const BarSignatureHash barHash{};
    hashMix(seed, barHash(value.outgoing));
    hashMix(seed, barHash(value.incoming));
    return seed;
  }
};

struct TerminalSignatureHash {
  size_t operator()(const TerminalSignature& value) const {
    size_t seed = 0;
    hashMix(seed, value.phraseBars);
    hashMix(seed, value.archetypeId);
    hashMix(seed, BarSignatureHash{}(value.terminal));
    return seed;
  }
};

enum class BoundaryClass : uint8_t {
  AOnset = 0,
  AContinuation,
  AOverlap,
  B,
  H,
  N0,
  N1,
  N3,
  Other,
  Count,
};

constexpr size_t kBoundaryClassCount =
    static_cast<size_t>(BoundaryClass::Count);

const char* className(BoundaryClass value) {
  switch (value) {
    case BoundaryClass::AOnset: return "A_ONSET";
    case BoundaryClass::AContinuation: return "A_CONTINUATION";
    case BoundaryClass::AOverlap: return "A_OVERLAP";
    case BoundaryClass::B: return "B";
    case BoundaryClass::H: return "H";
    case BoundaryClass::N0: return "N0";
    case BoundaryClass::N1: return "N1";
    case BoundaryClass::N3: return "N3";
    case BoundaryClass::Other: return "OTHER";
    case BoundaryClass::Count: break;
  }
  return "INVALID";
}

struct SignatureEvidence {
  uint64_t occurrences = 0;
  BoundaryClass classification = BoundaryClass::Other;
  Origin first{};
  bool hasDistinct = false;
  Origin distinct{};
  bool hasProfileDistinct = false;
  Origin profileDistinct{};
};

struct CorpusStats {
  uint64_t requestTuples = 0;
  uint64_t activeSettings = 0;
  uint64_t legacySettings = 0;
  uint64_t totalPhrases = 0;
  uint64_t totalAdjacentBoundaries = 0;
  uint64_t pureMelodicBoundaries = 0;
  uint64_t pureMelodicNonEmptyIncoming = 0;
  std::array<uint64_t, kBoundaryClassCount> raw{};
  std::array<uint64_t, kBoundaryClassCount> unique{};
  uint64_t n2Raw = 0;
  uint64_t n2Unique = 0;
  uint64_t collisionGroups = 0;
  uint64_t collisionReplays = 0;
  uint64_t collisionProfileDiverseGroups = 0;
  uint64_t aOnsetIntra = 0;
  uint64_t aOnsetSeam = 0;
  std::array<uint64_t, kGenerativeModeCount> aOnsetByMode{};
  std::array<uint64_t, kRecipeDomain> aOnsetByRecipe{};
  std::array<std::array<uint64_t, kRecipeDomain>, kGenerativeModeCount>
      aOnsetByProfile{};
  std::array<uint64_t, 4> aOnsetByLength{};
  std::array<uint64_t, 7> aOnset8ByBoundary{};
  uint64_t defaultAOnset = 0;
};

using SignatureMap = std::unordered_map<BoundarySignature, SignatureEvidence,
                                        BoundarySignatureHash>;
using TerminalSet = std::unordered_set<TerminalSignature, TerminalSignatureHash>;

GenreSettings autoSettings(uint8_t mode, uint8_t recipe) {
  GenreSettings value{};
  value.generativeMode = mode;
  value.recipe = recipe;
  value.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  return value;
}

PhraseExecutionMaterializationSettings materializationSettings() {
  PhraseExecutionMaterializationSettings value{};
  value.level = RealizationLevel::P2Variation;
  value.generationAttemptOrdinal = 0;
  value.feelProfile = FeelProfileId::Straight;
  value.feelAmount = 0;
  value.tonalMaterializationEnabled = true;
  value.rootPitchClass = 0;
  value.scaleTypeValue = kScaleDorian;
  return value;
}

StrongRhythmMigrationContext selectionContext() {
  const PhraseExecutionMaterializationSettings materialization =
      materializationSettings();
  StrongRhythmMigrationContext value{};
  value.patternAddress = 0;
  value.level = materialization.level;
  value.generationAttemptOrdinal = materialization.generationAttemptOrdinal;
  value.feelProfile = materialization.feelProfile;
  value.feelAmount = materialization.feelAmount;
  value.tonalMaterializationEnabled =
      materialization.tonalMaterializationEnabled;
  value.rootPitchClass = materialization.rootPitchClass;
  value.scaleTypeValue = materialization.scaleTypeValue;
  return value;
}

StepMask roleOnsets(const RoleRhythmPlan& role) {
  return static_cast<StepMask>(role.structural | role.secondary | role.ghosts);
}

StepMask protectedSpaceForTest(const RhythmArchetype& archetype,
                               RhythmRole role) {
  StepMask result = 0;
  const RhythmRoleMask roleBit = rhythmRoleBit(role);
  for (uint8_t index = 0; index < archetype.protectedSpaceCount; ++index) {
    const ProtectedSpace& space = archetype.protectedSpaces[index];
    if ((space.affectedRoles & roleBit) != 0) {
      result = static_cast<StepMask>(result | space.steps);
    }
  }
  return result;
}

bool sparseSemanticBarsAllowedForTest(const GenreSettings& settings,
                                      RhythmFamily family) {
  return family == RhythmFamily::DubPulse ||
         family == RhythmFamily::SparsePulse ||
         family == RhythmFamily::HipHopBackbeat ||
         settings.generativeMode ==
             static_cast<uint8_t>(GenerativeMode::LoFi) ||
         settings.generativeMode ==
             static_cast<uint8_t>(GenerativeMode::HipHop) ||
         settings.generativeMode ==
             static_cast<uint8_t>(GenerativeMode::FunkSoul);
}

SemanticSynthBRole semanticRole(CompositionSecondaryRole role) {
  switch (role) {
    case CompositionSecondaryRole::Chord:
      return SemanticSynthBRole::Chord;
    case CompositionSecondaryRole::Melodic:
      return SemanticSynthBRole::Melodic;
    case CompositionSecondaryRole::ChordWithMelodicFill:
      return SemanticSynthBRole::ChordWithMelodicFill;
    case CompositionSecondaryRole::Count:
      break;
  }
  return SemanticSynthBRole::Chord;
}

StepMask admittedMelodicContinuationsForTest(
    StepMask originalOnsets,
    StepMask originalContinuations,
    StepMask admittedOnsets,
    StepMask blocked) {
  StepMask result = 0;
  bool active = false;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    const StepMask bit = stepBit(step);
    if ((originalOnsets & bit) != 0) {
      active = (admittedOnsets & bit) != 0;
      continue;
    }
    if ((originalContinuations & bit) != 0 && active &&
        (blocked & bit) == 0) {
      result = static_cast<StepMask>(result | bit);
      continue;
    }
    active = false;
  }
  return result;
}

BarObservation observeProductionBar(
    const GenreSettings& settings,
    const StrongRhythmFrozenSelection& selection,
    uint8_t phraseBarOrdinal) {
  BarObservation observed{};
  observed.role = semanticRole(selection.composition.secondaryRole);

  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(
          selection.composition.rhythmArchetypeId);
  if (definition == nullptr) return observed;
  const RhythmArchetype* archetype =
      ReferenceVocabulary::archetypeFor(definition->key);
  if (archetype == nullptr) return observed;
  observed.family = definition->family;
  observed.allowSparse =
      sparseSemanticBarsAllowedForTest(settings, definition->family);

  RhythmRealizationRequest realizationRequest{};
  realizationRequest.catalog = &ReferenceVocabulary::catalog();
  realizationRequest.archetypeId = definition->archetypeId;
  realizationRequest.phraseBars = 1;
  realizationRequest.level = RealizationLevel::P2Variation;
  realizationRequest.generation = selection.realizationGeneration;
  const RhythmRealizationResult realization =
      realizeRhythmPhrase(realizationRequest);
  if (realization.status != RealizationStatus::Ok &&
      realization.status != RealizationStatus::ValidButSparse) {
    return observed;
  }
  observed.kickOnsets = roleOnsets(
      realization.plan.bars[0].roles[
          static_cast<uint8_t>(RhythmRole::Kick)]);

  const uint8_t barOrdinal = phraseVocabularyBarOrdinal(phraseBarOrdinal);

  BassRhythmRequest bassRequest{};
  bassRequest.requestedId = selection.composition.bassRhythm;
  bassRequest.family = definition->family;
  bassRequest.archetypeId = definition->archetypeId;
  bassRequest.kickOnsets = observed.kickOnsets;
  bassRequest.protectedSpace =
      protectedSpaceForTest(*archetype, RhythmRole::BassRhythm);
  bassRequest.generation = selection.realizationGeneration;
  bassRequest.barOrdinal = barOrdinal;
  bassRequest.allowEmptyBar = observed.allowSparse;
  const BassRhythmResult bass = realizeBassRhythm(bassRequest);
  observed.bassStatus = bass.status;
  if (bass.status != BassRhythmStatus::Ok &&
      bass.status != BassRhythmStatus::ValidButEmpty) {
    return observed;
  }
  observed.bassOnsets = bass.plan.onsets;

  ChordRhythmRequest chordRequest{};
  chordRequest.requestedId = selection.composition.chordRhythm;
  chordRequest.family = definition->family;
  chordRequest.archetypeId = definition->archetypeId;
  chordRequest.bassOnsets = bass.plan.onsets;
  chordRequest.protectedSpace =
      protectedSpaceForTest(*archetype, RhythmRole::ChordRhythm);
  chordRequest.generation = selection.realizationGeneration;
  chordRequest.barOrdinal = barOrdinal;
  chordRequest.allowEmptyBar = observed.allowSparse;
  const ChordRhythmResult chord = realizeChordRhythm(chordRequest);
  observed.chordStatus = chord.status;
  if (chord.status != ChordRhythmStatus::Ok &&
      chord.status != ChordRhythmStatus::ValidButEmpty) {
    return observed;
  }
  observed.chordOnsets = chord.plan.onsets;
  observed.chordContinuations = chord.plan.continuations;

  MelodicMotifRequest melodicRequest{};
  melodicRequest.requestedRhythm = selection.composition.melodicRhythm;
  melodicRequest.requestedShape = selection.composition.motifShape;
  melodicRequest.family = definition->family;
  melodicRequest.archetypeId = definition->archetypeId;
  melodicRequest.bassOnsets = bass.plan.onsets;
  melodicRequest.chordOnsets =
      observed.role == SemanticSynthBRole::Melodic ? 0 : chord.plan.onsets;
  observed.protectedMelodic =
      protectedSpaceForTest(*archetype, RhythmRole::MelodicRhythm);
  melodicRequest.protectedSpace = observed.protectedMelodic;
  melodicRequest.generation = selection.realizationGeneration;
  melodicRequest.barOrdinal = barOrdinal;
  melodicRequest.allowEmptyBar = observed.allowSparse;
  const MelodicMotifResult melodic = realizeMelodicMotif(melodicRequest);
  observed.melodicStatus = melodic.status;
  observed.melodicRhythm = melodic.plan.rhythmId;
  observed.motifShape = melodic.plan.motif.shape;
  if (melodic.status != MelodicMotifStatus::Ok &&
      melodic.status != MelodicMotifStatus::ValidButEmpty) {
    return observed;
  }

  const TonalGenerationProfile tonalProfile = tonalGenerationProfileFor(settings);
  MelodicPitchIntentRequest pitchRequest{};
  pitchRequest.rhythmPlan = melodic.plan;
  pitchRequest.archetypeId = definition->archetypeId;
  pitchRequest.generation = melodicRequest.generation;
  pitchRequest.barOrdinal = barOrdinal;
  pitchRequest.policy = tonalProfile.melodicPolicy;
  pitchRequest.allowedOnsetSteps = kAllSteps;
  pitchRequest.allowedContinuationSteps = kAllSteps;
  pitchRequest.allowEmptyBar = observed.allowSparse;
  const MelodicPitchIntentResult pitch =
      realizeMelodicPitchIntent(pitchRequest);
  observed.pitchStatus = pitch.status;
  if (pitch.status != MelodicPitchIntentStatus::Ok &&
      pitch.status != MelodicPitchIntentStatus::ValidButEmpty) {
    return observed;
  }

  if (observed.role == SemanticSynthBRole::ChordWithMelodicFill) {
    const StepMask chordOccupied = static_cast<StepMask>(
        chord.plan.onsets | chord.plan.continuations);
    observed.admittedOnsets = static_cast<StepMask>(
        pitch.plan.onsets & ~chordOccupied);
    observed.admittedContinuations =
        admittedMelodicContinuationsForTest(
            pitch.plan.onsets, pitch.plan.continuations,
            observed.admittedOnsets, chordOccupied);
  } else if (observed.role == SemanticSynthBRole::Melodic) {
    observed.admittedOnsets = pitch.plan.onsets;
    observed.admittedContinuations = pitch.plan.continuations;
  }

  observed.valid = true;
  return observed;
}

BarSignature barSignature(const BarObservation& value) {
  BarSignature result{};
  result.allowSparse = value.allowSparse ? 1u : 0u;
  result.family = static_cast<uint8_t>(value.family);
  result.role = static_cast<uint8_t>(value.role);
  result.bassStatus = static_cast<uint8_t>(value.bassStatus);
  result.chordStatus = static_cast<uint8_t>(value.chordStatus);
  result.melodicRhythm = static_cast<uint8_t>(value.melodicRhythm);
  result.motifShape = static_cast<uint8_t>(value.motifShape);
  result.melodicStatus = static_cast<uint8_t>(value.melodicStatus);
  result.pitchStatus = static_cast<uint8_t>(value.pitchStatus);
  result.kickOnsets = value.kickOnsets;
  result.bassOnsets = value.bassOnsets;
  result.chordOnsets = value.chordOnsets;
  result.chordContinuations = value.chordContinuations;
  result.protectedMelodic = value.protectedMelodic;
  result.admittedOnsets = value.admittedOnsets;
  result.admittedContinuations = value.admittedContinuations;
  return result;
}

int firstLogicalStep(StepMask mask) {
  for (int step = 0; step < static_cast<int>(kStepsPerBar); ++step) {
    if ((mask & stepBit(static_cast<uint8_t>(step))) != 0) return step;
  }
  return -1;
}

int lastLogicalStep(StepMask mask) {
  for (int step = static_cast<int>(kStepsPerBar) - 1; step >= 0; --step) {
    if ((mask & stepBit(static_cast<uint8_t>(step))) != 0) return step;
  }
  return -1;
}

BoundaryClass classifyBoundary(const BarObservation& outgoing,
                               const BarObservation& incoming) {
  assert(outgoing.valid && incoming.valid);
  if (outgoing.role == SemanticSynthBRole::Chord ||
      incoming.role == SemanticSynthBRole::Chord) {
    return BoundaryClass::Other;
  }

  const StepMask outgoingOccupied = static_cast<StepMask>(
      outgoing.admittedOnsets | outgoing.admittedContinuations);
  const StepMask incomingOccupied = static_cast<StepMask>(
      incoming.admittedOnsets | incoming.admittedContinuations);
  const bool incomingEmpty =
      incoming.melodicStatus == MelodicMotifStatus::ValidButEmpty ||
      incoming.pitchStatus == MelodicPitchIntentStatus::ValidButEmpty ||
      incomingOccupied == 0;
  if (incomingEmpty) return BoundaryClass::N1;

  if ((incoming.admittedOnsets & stepBit(0)) != 0)
    return BoundaryClass::N0;

  const bool laterIncoming =
      (incoming.admittedOnsets & kLaterThanStepZeroMask) != 0;
  const bool outgoingAny = outgoingOccupied != 0;
  const bool outgoing15 = (outgoingOccupied & stepBit(15)) != 0;

  if (outgoing.role == SemanticSynthBRole::ChordWithMelodicFill ||
      incoming.role == SemanticSynthBRole::ChordWithMelodicFill) {
    if (outgoingAny && laterIncoming) return BoundaryClass::H;
    if (!outgoing15) return BoundaryClass::N3;
    return BoundaryClass::Other;
  }

  assert(outgoing.role == SemanticSynthBRole::Melodic);
  assert(incoming.role == SemanticSynthBRole::Melodic);
  if (laterIncoming && outgoing15) {
    const bool onset15 =
        (outgoing.admittedOnsets & stepBit(15)) != 0;
    const bool continuation15 =
        (outgoing.admittedContinuations & stepBit(15)) != 0;
    if (onset15 && continuation15) return BoundaryClass::AOverlap;
    if (onset15) return BoundaryClass::AOnset;
    if (continuation15) return BoundaryClass::AContinuation;
  }
  if (laterIncoming && outgoingAny && !outgoing15)
    return BoundaryClass::B;
  if (!outgoing15) return BoundaryClass::N3;
  return BoundaryClass::Other;
}

bool sameOrigin(const Origin& a, const Origin& b) {
  return a.settings.generativeMode == b.settings.generativeMode &&
         a.settings.recipe == b.settings.recipe &&
         a.profileRecipe == b.profileRecipe &&
         a.phraseBars == b.phraseBars && a.identity == b.identity &&
         a.boundary == b.boundary;
}

bool profileDistinct(const Origin& a, const Origin& b) {
  return a.settings.generativeMode != b.settings.generativeMode ||
         a.profileRecipe != b.profileRecipe;
}

BoundarySignature makeBoundarySignature(
    uint8_t phraseBars,
    uint8_t boundary,
    RhythmArchetypeId archetypeId,
    const BarObservation& outgoing,
    const BarObservation& incoming) {
  BoundarySignature result{};
  result.phraseBars = phraseBars;
  result.boundary = boundary;
  result.evolutionSeam =
      phraseBars == 8 && boundary == 3 ? 1u : 0u;
  result.archetypeId = archetypeId;
  result.outgoing = barSignature(outgoing);
  result.incoming = barSignature(incoming);
  return result;
}

bool sameSelectionCore(const StrongRhythmFrozenSelection& a,
                       const StrongRhythmFrozenSelection& b) {
  return a.route == b.route &&
         a.composition.rhythmArchetypeId == b.composition.rhythmArchetypeId &&
         a.composition.bassRhythm == b.composition.bassRhythm &&
         a.composition.chordRhythm == b.composition.chordRhythm &&
         a.composition.progression == b.composition.progression &&
         a.composition.melodicRhythm == b.composition.melodicRhythm &&
         a.composition.motifShape == b.composition.motifShape &&
         a.composition.secondaryRole == b.composition.secondaryRole &&
         a.selectionGeneration.projectSeed == b.selectionGeneration.projectSeed &&
         a.selectionGeneration.phraseOrdinal == b.selectionGeneration.phraseOrdinal &&
         a.realizationGeneration.projectSeed == b.realizationGeneration.projectSeed &&
         a.realizationGeneration.phraseOrdinal == b.realizationGeneration.phraseOrdinal;
}

uint8_t collectAdmittedLengths(const GenerationProfileView& profile,
                               uint8_t* destination) {
  bool admitted[9]{};
  for (uint8_t index = 0; index < profile.phraseLaws.count; ++index) {
    const WeightedIdentityCandidate candidate = profile.phraseLaws.candidates[index];
    if (candidate.weight == 0) continue;
    const uint8_t bars = static_cast<uint8_t>(candidate.id & 0x0Fu);
    if (bars < 9) admitted[bars] = true;
  }
  uint8_t count = 0;
  for (const uint8_t bars : kLengthDomain) {
    if (admitted[bars]) destination[count++] = bars;
  }
  return count;
}

size_t lengthIndex(uint8_t phraseBars) {
  switch (phraseBars) {
    case 1: return 0;
    case 2: return 1;
    case 4: return 2;
    case 8: return 3;
    default: break;
  }
  assert(false);
  return 0;
}

bool isPureMelodicBoundary(const BarObservation& outgoing,
                           const BarObservation& incoming) {
  return outgoing.role == SemanticSynthBRole::Melodic &&
         incoming.role == SemanticSynthBRole::Melodic;
}

bool incomingMelodicNonEmpty(const BarObservation& incoming) {
  return incoming.melodicStatus != MelodicMotifStatus::ValidButEmpty &&
         incoming.pitchStatus != MelodicPitchIntentStatus::ValidButEmpty &&
         static_cast<StepMask>(incoming.admittedOnsets |
                               incoming.admittedContinuations) != 0;
}

void addSignature(SignatureMap& signatures,
                  const BoundarySignature& signature,
                  BoundaryClass classification,
                  const Origin& origin) {
  auto inserted = signatures.emplace(signature, SignatureEvidence{});
  SignatureEvidence& evidence = inserted.first->second;
  if (inserted.second) {
    evidence.classification = classification;
    evidence.first = origin;
  } else {
    assert(evidence.classification == classification);
    if (!evidence.hasDistinct && !sameOrigin(evidence.first, origin)) {
      evidence.hasDistinct = true;
      evidence.distinct = origin;
    }
    if (!evidence.hasProfileDistinct &&
        profileDistinct(evidence.first, origin)) {
      evidence.hasProfileDistinct = true;
      evidence.profileDistinct = origin;
    }
  }
  ++evidence.occurrences;
}

void updateAOnsetDistribution(CorpusStats& stats,
                              const Origin& origin) {
  ++stats.aOnsetByMode[origin.settings.generativeMode];
  ++stats.aOnsetByRecipe[origin.settings.recipe];
  ++stats.aOnsetByProfile[origin.settings.generativeMode]
                         [origin.profileRecipe];
  ++stats.aOnsetByLength[lengthIndex(origin.phraseBars)];
  if (origin.phraseBars == 8 && origin.boundary < 7)
    ++stats.aOnset8ByBoundary[origin.boundary];
  if (origin.phraseBars == 8 && origin.boundary == 3)
    ++stats.aOnsetSeam;
  else
    ++stats.aOnsetIntra;
  if (origin.settings.generativeMode ==
          static_cast<uint8_t>(GenerativeMode::Acid) &&
      origin.settings.recipe == kBaseRecipeId) {
    ++stats.defaultAOnset;
  }
}

struct ReplayResult {
  BoundarySignature signature{};
  BoundaryClass classification = BoundaryClass::Other;
};

ReplayResult replayOrigin(const Origin& origin) {
  StrongRhythmFrozenSelection selection{};
  PhraseLengthRequestResult length{};
  const StrongRhythmMigrationResult resolved =
      resolveStrongRhythmFrozenSelectionForPhraseBars(
          origin.settings, selectionContext(), origin.identity,
          origin.phraseBars, length, selection);
  assert(resolved.status == StrongRhythmMigrationStatus::Applied);
  assert(length.status == PhraseLengthRequestStatus::Accepted);
  assert(selection.resolved);
  const BarObservation outgoing =
      observeProductionBar(origin.settings, selection, origin.boundary);
  const BarObservation incoming = observeProductionBar(
      origin.settings, selection,
      static_cast<uint8_t>(origin.boundary + 1u));
  assert(outgoing.valid && incoming.valid);
  ReplayResult result{};
  result.signature = makeBoundarySignature(
      origin.phraseBars, origin.boundary,
      selection.composition.rhythmArchetypeId, outgoing, incoming);
  result.classification = classifyBoundary(outgoing, incoming);
  return result;
}

void validateCollisionGroups(SignatureMap& signatures, CorpusStats& stats) {
  for (const auto& entry : signatures) {
    const BoundarySignature& signature = entry.first;
    const SignatureEvidence& evidence = entry.second;
    if (evidence.occurrences <= 1) continue;
    ++stats.collisionGroups;
    assert(evidence.hasDistinct);

    const ReplayResult first = replayOrigin(evidence.first);
    const ReplayResult second = replayOrigin(evidence.distinct);
    assert(first.signature == signature);
    assert(second.signature == signature);
    assert(first.classification == evidence.classification);
    assert(second.classification == evidence.classification);
    stats.collisionReplays += 2;

    if (evidence.hasProfileDistinct) {
      const ReplayResult profileReplay = replayOrigin(evidence.profileDistinct);
      assert(profileReplay.signature == signature);
      assert(profileReplay.classification == evidence.classification);
      ++stats.collisionProfileDiverseGroups;
      ++stats.collisionReplays;
    }
  }
}

BarObservation manualPureBar(StepMask onsets,
                             StepMask continuations,
                             bool empty = false) {
  BarObservation value{};
  value.valid = true;
  value.role = SemanticSynthBRole::Melodic;
  value.family = RhythmFamily::FourFloor;
  value.bassStatus = BassRhythmStatus::Ok;
  value.chordStatus = ChordRhythmStatus::Ok;
  value.melodicRhythm = MelodicRhythmId::SparseCall;
  value.motifShape = MotifShapeId::SourceOrder;
  value.melodicStatus = empty ? MelodicMotifStatus::ValidButEmpty
                              : MelodicMotifStatus::Ok;
  value.pitchStatus = empty ? MelodicPitchIntentStatus::ValidButEmpty
                            : MelodicPitchIntentStatus::Ok;
  value.admittedOnsets = onsets;
  value.admittedContinuations = continuations;
  return value;
}

void validateM1LControls() {
  const BarObservation sparse = manualPureBar(stepBit(2), 0);
  const BarObservation empty = manualPureBar(0, 0, true);
  assert(classifyBoundary(sparse, empty) == BoundaryClass::N1);
  assert(classifyBoundary(empty, sparse) == BoundaryClass::N3);
  assert(classifyBoundary(sparse, empty) == BoundaryClass::N1);
  std::puts("C2-C0 M1L SPARSE: 0->1=N1 1->2=N3 2->3=N1");

  const StepMask callMask = static_cast<StepMask>(stepBit(6) | stepBit(14));
  const BarObservation call = manualPureBar(callMask, 0);
  assert(classifyBoundary(call, call) == BoundaryClass::B);
  std::puts("C2-C0 M1L CALL_STYLE: 0->1=B 1->2=B 2->3=B");
}

void validateContinuationDependencyProof() {
  const ReferenceVocabulary::Definition& definition =
      ReferenceVocabulary::definition(0);
  assert(definition.archetypeId != kNoArchetypeId);

  for (uint8_t rhythmValue = 1;
       rhythmValue < static_cast<uint8_t>(MelodicRhythmId::Count);
       ++rhythmValue) {
    for (uint8_t bar = 0; bar < kGrooveVocabularyPhraseBars; ++bar) {
      MelodicMotifRequest request{};
      request.requestedRhythm =
          static_cast<MelodicRhythmId>(rhythmValue);
      request.requestedShape = MotifShapeId::SourceOrder;
      request.family = definition.family;
      request.archetypeId = definition.archetypeId;
      request.generation = GenerationContext{};
      request.barOrdinal = bar;
      request.allowEmptyBar = true;
      const MelodicMotifResult melodic = realizeMelodicMotif(request);
      assert(melodic.status == MelodicMotifStatus::Ok ||
             melodic.status == MelodicMotifStatus::ValidButEmpty);
      assert((melodic.plan.continuations & stepBit(15)) == 0);
    }
  }

  for (uint8_t mode = 0; mode < kGenerativeModeCount; ++mode) {
    for (uint8_t recipe = 0; recipe < kRecipeDomain; ++recipe) {
      const GenreSettings settings = autoSettings(mode, recipe);
      const TonalGenerationProfile tonal = tonalGenerationProfileFor(settings);
      const uint16_t preserve =
          melodicRhythmOperationBit(MelodicRhythmOperationId::Preserve);
      assert(tonal.melodicPolicy.allowedRhythmOperations == preserve);
      assert(tonal.melodicPolicy.preferredRhythmOperations == 0);
    }
  }

  std::puts(
      "C2-C0 CONTINUATION SOURCE PROOF: motif continuation15=0 for all "
      "frozen melodic rhythms/bars; all tonal profiles=PRESERVE");
}

void printOrigin(const char* label, const Origin& origin) {
  const ReplayResult replay = replayOrigin(origin);
  StrongRhythmFrozenSelection selection{};
  PhraseLengthRequestResult length{};
  const StrongRhythmMigrationResult resolved =
      resolveStrongRhythmFrozenSelectionForPhraseBars(
          origin.settings, selectionContext(), origin.identity,
          origin.phraseBars, length, selection);
  assert(resolved.status == StrongRhythmMigrationStatus::Applied);
  const BarObservation outgoing =
      observeProductionBar(origin.settings, selection, origin.boundary);
  const BarObservation incoming = observeProductionBar(
      origin.settings, selection,
      static_cast<uint8_t>(origin.boundary + 1u));
  std::printf(
      "C2-C0 %s: class=%s mode=%u recipe=%u profile_recipe=%u bars=%u "
      "identity=%u boundary=%u->%u archetype=%u progression=%u rhythm=%s "
      "motif=%s out_on=0x%04x out_cont=0x%04x in_on=0x%04x "
      "in_cont=0x%04x out_last=%d in_first=%d\n",
      label, className(replay.classification),
      static_cast<unsigned>(origin.settings.generativeMode),
      static_cast<unsigned>(origin.settings.recipe),
      static_cast<unsigned>(origin.profileRecipe),
      static_cast<unsigned>(origin.phraseBars),
      static_cast<unsigned>(origin.identity),
      static_cast<unsigned>(origin.boundary),
      static_cast<unsigned>(origin.boundary + 1u),
      static_cast<unsigned>(selection.composition.rhythmArchetypeId),
      static_cast<unsigned>(selection.composition.progression),
      melodicRhythmName(outgoing.melodicRhythm),
      motifShapeName(outgoing.motifShape),
      static_cast<unsigned>(outgoing.admittedOnsets),
      static_cast<unsigned>(outgoing.admittedContinuations),
      static_cast<unsigned>(incoming.admittedOnsets),
      static_cast<unsigned>(incoming.admittedContinuations),
      lastLogicalStep(static_cast<StepMask>(outgoing.admittedOnsets |
                                            outgoing.admittedContinuations)),
      firstLogicalStep(incoming.admittedOnsets));
}

void validateKnownAOnset() {
  Origin known{};
  known.settings = autoSettings(
      static_cast<uint8_t>(GenerativeMode::Acid), kBaseRecipeId);
  known.profileRecipe = kBaseRecipeId;
  known.phraseBars = 2;
  known.identity = 2;
  known.boundary = 0;
  const ReplayResult replay = replayOrigin(known);
  assert(replay.classification == BoundaryClass::AOnset);
  assert(replay.signature.outgoing.admittedOnsets == 0x0009u);
  assert(replay.signature.outgoing.admittedContinuations == 0);
  assert(replay.signature.incoming.admittedOnsets == 0x0009u);
  assert(replay.signature.incoming.admittedContinuations == 0);
  printOrigin("KNOWN_MINIMAL_A", known);
}

void validateOptionalH2(const Origin& origin) {
  PhraseExecutionScratch scratch{};
  PreparedPhraseExecution prepared{};
  const PhraseExecutionStatus status = preparePhraseExecution(
      origin.settings, materializationSettings(), origin.identity,
      origin.phraseBars, scratch, prepared);
  assert(status == PhraseExecutionStatus::Ready);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  assert(prepared.progressionSource.id == ProgressionId::TwoFiveOne);
  assert(prepared.progressionSource.period == 3);
  assert(origin.phraseBars == 8 && origin.boundary == 3);
  const PhraseHarmonicBarProjection& outgoing = prepared.harmonicClock.bars[3];
  const PhraseHarmonicBarProjection& incoming = prepared.harmonicClock.bars[4];
  assert(outgoing.harmonicRhythm.eventCount == 2);
  assert(incoming.harmonicRhythm.eventCount == 2);
  assert(outgoing.eventRange.firstOrdinal == 6);
  assert(incoming.eventRange.firstOrdinal == 8);
  std::printf(
      "C2-C0 OPTIONAL H1-F1/H2: FOUND identity=%u mode=%u recipe=%u "
      "boundary=3->4 progression=TWO_FIVE_ONE period=3 ordinals=7->8\n",
      static_cast<unsigned>(origin.identity),
      static_cast<unsigned>(origin.settings.generativeMode),
      static_cast<unsigned>(origin.settings.recipe));
}

void printPercent(const char* label, uint64_t numerator, uint64_t denominator) {
  const double percent = denominator == 0
      ? 0.0
      : 100.0 * static_cast<double>(numerator) /
            static_cast<double>(denominator);
  std::printf("C2-C0 %s: %llu/%llu %.9f%%\n", label,
              static_cast<unsigned long long>(numerator),
              static_cast<unsigned long long>(denominator), percent);
}

}  // namespace

int main(int argc, char** argv) {
  bool smoke = false;
  if (argc == 2 && std::strcmp(argv[1], "--smoke") == 0) smoke = true;
  assert(argc == 1 || smoke);

  validateContinuationDependencyProof();
  validateM1LControls();
  validateKnownAOnset();

  if (smoke) {
    std::puts("C2-C0 SMOKE: structural/classifier/known-witness gates OK");
    return 0;
  }

  CorpusStats stats{};
  stats.requestTuples =
      static_cast<uint64_t>(kGenerativeModeCount) * kRecipeDomain *
      kIdentityDomain * 4u;

  SignatureMap signatures;
  signatures.reserve(131072);
  TerminalSet terminalSignatures;
  terminalSignatures.reserve(32768);

  Origin firstLengthA[4]{};
  bool hasLengthA[4]{};
  Origin firstSeamA{};
  bool hasSeamA = false;
  Origin firstNonDefaultA{};
  bool hasNonDefaultA = false;
  Origin optionalH2{};
  bool hasOptionalH2 = false;

  const StrongRhythmMigrationContext context = selectionContext();

  for (uint8_t mode = 0; mode < kGenerativeModeCount; ++mode) {
    for (uint8_t recipe = 0; recipe < kRecipeDomain; ++recipe) {
      const GenreSettings settings = autoSettings(mode, recipe);
      const StrongRhythmRoute route = selectStrongRhythmRoute(settings);
      if (route == StrongRhythmRoute::Legacy) {
        ++stats.legacySettings;
        continue;
      }
      ++stats.activeSettings;

      const GenerationProfileView profile = generationProfileFor(settings);
      assert(isValidGenerationProfile(profile));
      uint8_t admittedLengths[4]{};
      const uint8_t admittedLengthCount =
          collectAdmittedLengths(profile, admittedLengths);
      assert(admittedLengthCount > 0);

      for (uint32_t identityValue = 0;
           identityValue < kIdentityDomain; ++identityValue) {
        const uint16_t identity = static_cast<uint16_t>(identityValue);
        StrongRhythmFrozenSelection coreSelection{};
        bool hasCoreSelection = false;
        std::array<BarObservation, kGrooveVocabularyPhraseBars> vocabulary{};

        for (uint8_t lengthOrdinal = 0;
             lengthOrdinal < admittedLengthCount; ++lengthOrdinal) {
          const uint8_t phraseBars = admittedLengths[lengthOrdinal];
          StrongRhythmFrozenSelection selection{};
          PhraseLengthRequestResult length{};
          const StrongRhythmMigrationResult resolved =
              resolveStrongRhythmFrozenSelectionForPhraseBars(
                  settings, context, identity, phraseBars, length, selection);
          assert(resolved.status == StrongRhythmMigrationStatus::Applied);
          assert(length.status == PhraseLengthRequestStatus::Accepted);
          assert(selection.resolved);
          assert(selection.phraseGenerationIdentity == identity);
          assert(selection.composition.phraseBars == phraseBars);

          if (!hasCoreSelection) {
            coreSelection = selection;
            hasCoreSelection = true;
            for (uint8_t bar = 0;
                 bar < kGrooveVocabularyPhraseBars; ++bar) {
              vocabulary[bar] = observeProductionBar(settings, selection, bar);
              assert(vocabulary[bar].valid);
            }
          } else {
            assert(sameSelectionCore(coreSelection, selection));
          }

          ++stats.totalPhrases;
          ++stats.n2Raw;
          const uint8_t terminalBar = static_cast<uint8_t>(phraseBars - 1u);
          TerminalSignature terminal{};
          terminal.phraseBars = phraseBars;
          terminal.archetypeId = selection.composition.rhythmArchetypeId;
          terminal.terminal =
              barSignature(vocabulary[phraseVocabularyBarOrdinal(terminalBar)]);
          terminalSignatures.insert(terminal);

          const uint8_t boundaryCount = static_cast<uint8_t>(phraseBars - 1u);
          for (uint8_t boundary = 0; boundary < boundaryCount; ++boundary) {
            const BarObservation& outgoing =
                vocabulary[phraseVocabularyBarOrdinal(boundary)];
            const BarObservation& incoming =
                vocabulary[phraseVocabularyBarOrdinal(
                    static_cast<uint8_t>(boundary + 1u))];
            const BoundaryClass classification =
                classifyBoundary(outgoing, incoming);
            const size_t classIndex = static_cast<size_t>(classification);
            assert(classIndex < kBoundaryClassCount);
            ++stats.raw[classIndex];
            ++stats.totalAdjacentBoundaries;

            if (isPureMelodicBoundary(outgoing, incoming)) {
              ++stats.pureMelodicBoundaries;
              if (incomingMelodicNonEmpty(incoming))
                ++stats.pureMelodicNonEmptyIncoming;
            }

            Origin origin{};
            origin.settings = settings;
            origin.profileRecipe = profile.recipe;
            origin.phraseBars = phraseBars;
            origin.identity = identity;
            origin.boundary = boundary;

            const BoundarySignature signature = makeBoundarySignature(
                phraseBars, boundary,
                selection.composition.rhythmArchetypeId,
                outgoing, incoming);
            addSignature(signatures, signature, classification, origin);

            if (classification == BoundaryClass::AOnset) {
              updateAOnsetDistribution(stats, origin);
              const size_t index = lengthIndex(phraseBars);
              if (!hasLengthA[index]) {
                hasLengthA[index] = true;
                firstLengthA[index] = origin;
              }
              if (!hasSeamA && phraseBars == 8 && boundary == 3) {
                hasSeamA = true;
                firstSeamA = origin;
              }
              if (!hasNonDefaultA &&
                  (mode != static_cast<uint8_t>(GenerativeMode::Acid) ||
                   recipe != kBaseRecipeId)) {
                hasNonDefaultA = true;
                firstNonDefaultA = origin;
              }
              if (!hasOptionalH2 && phraseBars == 8 && boundary == 3 &&
                  selection.composition.progression ==
                      ProgressionId::TwoFiveOne) {
                hasOptionalH2 = true;
                optionalH2 = origin;
              }
            }
          }
        }
      }
    }
  }

  stats.n2Unique = terminalSignatures.size();
  for (const auto& entry : signatures) {
    const size_t index = static_cast<size_t>(entry.second.classification);
    assert(index < kBoundaryClassCount);
    ++stats.unique[index];
  }
  validateCollisionGroups(signatures, stats);

  uint64_t classSum = 0;
  for (uint64_t value : stats.raw) classSum += value;
  assert(classSum == stats.totalAdjacentBoundaries);
  assert(stats.raw[static_cast<size_t>(BoundaryClass::AOnset)] > 0);
  assert(stats.raw[static_cast<size_t>(BoundaryClass::AContinuation)] == 0);
  assert(stats.raw[static_cast<size_t>(BoundaryClass::AOverlap)] == 0);
  assert(stats.defaultAOnset > 0);

  std::printf(
      "C2-C0 CORPUS DOMAIN: modes=%u recipes=%u identities=%u lengths=4 "
      "attempt=0 rhythm_selection=AUTO request_tuples=%llu\n",
      static_cast<unsigned>(kGenerativeModeCount),
      static_cast<unsigned>(kRecipeDomain),
      static_cast<unsigned>(kIdentityDomain),
      static_cast<unsigned long long>(stats.requestTuples));
  std::printf(
      "C2-C0 CORPUS SETTINGS: active=%llu legacy=%llu\n",
      static_cast<unsigned long long>(stats.activeSettings),
      static_cast<unsigned long long>(stats.legacySettings));
  std::printf(
      "C2-C0 CORPUS TOTALS: phrases=%llu adjacent_boundaries=%llu "
      "unique_boundary_signatures=%llu pure_melodic=%llu "
      "pure_melodic_nonempty_incoming=%llu\n",
      static_cast<unsigned long long>(stats.totalPhrases),
      static_cast<unsigned long long>(stats.totalAdjacentBoundaries),
      static_cast<unsigned long long>(signatures.size()),
      static_cast<unsigned long long>(stats.pureMelodicBoundaries),
      static_cast<unsigned long long>(stats.pureMelodicNonEmptyIncoming));

  for (size_t index = 0; index < kBoundaryClassCount; ++index) {
    const BoundaryClass classification =
        static_cast<BoundaryClass>(index);
    std::printf("C2-C0 CLASS %s: raw=%llu unique=%llu\n",
                className(classification),
                static_cast<unsigned long long>(stats.raw[index]),
                static_cast<unsigned long long>(stats.unique[index]));
  }
  std::printf("C2-C0 CLASS N2_TERMINAL: raw=%llu unique=%llu "
              "denominator=terminal_phrase_controls_not_adjacent\n",
              static_cast<unsigned long long>(stats.n2Raw),
              static_cast<unsigned long long>(stats.n2Unique));

  const uint64_t aOnset =
      stats.raw[static_cast<size_t>(BoundaryClass::AOnset)];
  printPercent("A_ONSET / ALL_ADJACENT", aOnset,
               stats.totalAdjacentBoundaries);
  printPercent("A_ONSET / PURE_MELODIC", aOnset,
               stats.pureMelodicBoundaries);
  printPercent("A_ONSET / PURE_MELODIC_NONEMPTY_INCOMING", aOnset,
               stats.pureMelodicNonEmptyIncoming);

  for (uint8_t mode = 0; mode < kGenerativeModeCount; ++mode) {
    if (stats.aOnsetByMode[mode] == 0) continue;
    std::printf("C2-C0 A_ONSET MODE %u: %llu\n",
                static_cast<unsigned>(mode),
                static_cast<unsigned long long>(stats.aOnsetByMode[mode]));
  }
  for (uint8_t recipe = 0; recipe < kRecipeDomain; ++recipe) {
    if (stats.aOnsetByRecipe[recipe] == 0) continue;
    std::printf("C2-C0 A_ONSET RECIPE %u: %llu\n",
                static_cast<unsigned>(recipe),
                static_cast<unsigned long long>(stats.aOnsetByRecipe[recipe]));
  }
  for (uint8_t mode = 0; mode < kGenerativeModeCount; ++mode) {
    for (uint8_t profileRecipe = 0;
         profileRecipe < kRecipeDomain; ++profileRecipe) {
      const uint64_t value = stats.aOnsetByProfile[mode][profileRecipe];
      if (value == 0) continue;
      std::printf("C2-C0 A_ONSET PROFILE mode=%u profile_recipe=%u: %llu\n",
                  static_cast<unsigned>(mode),
                  static_cast<unsigned>(profileRecipe),
                  static_cast<unsigned long long>(value));
    }
  }
  for (size_t index = 0; index < 4; ++index) {
    std::printf("C2-C0 A_ONSET LENGTH %u: %llu\n",
                static_cast<unsigned>(kLengthDomain[index]),
                static_cast<unsigned long long>(stats.aOnsetByLength[index]));
  }
  std::printf("C2-C0 A_ONSET LOCATION: intra=%llu seam_3_to_4=%llu\n",
              static_cast<unsigned long long>(stats.aOnsetIntra),
              static_cast<unsigned long long>(stats.aOnsetSeam));
  for (uint8_t boundary = 0; boundary < 7; ++boundary) {
    std::printf("C2-C0 A_ONSET 8BAR BOUNDARY %u->%u: %llu\n",
                static_cast<unsigned>(boundary),
                static_cast<unsigned>(boundary + 1u),
                static_cast<unsigned long long>(
                    stats.aOnset8ByBoundary[boundary]));
  }

  std::printf(
      "C2-C0 DEFAULT PATH: GenreSettings{} => mode=0(Acid) recipe=0 "
      "rhythm_selection=AUTO; A_ONSET raw=%llu reachable=%s\n",
      static_cast<unsigned long long>(stats.defaultAOnset),
      stats.defaultAOnset > 0 ? "YES" : "NO");

  std::printf(
      "C2-C0 SIGNATURE COLLISION VALIDATION: groups=%llu replays=%llu "
      "profile_diverse_groups=%llu PASS\n",
      static_cast<unsigned long long>(stats.collisionGroups),
      static_cast<unsigned long long>(stats.collisionReplays),
      static_cast<unsigned long long>(stats.collisionProfileDiverseGroups));

  for (size_t index = 0; index < 4; ++index) {
    if (!hasLengthA[index]) continue;
    char label[32]{};
    std::snprintf(label, sizeof(label), "A_REP_LENGTH_%u",
                  static_cast<unsigned>(kLengthDomain[index]));
    printOrigin(label, firstLengthA[index]);
  }
  if (hasSeamA) printOrigin("A_REP_8BAR_SEAM", firstSeamA);
  if (hasNonDefaultA) printOrigin("A_REP_NONDEFAULT", firstNonDefaultA);

  if (hasOptionalH2) {
    validateOptionalH2(optionalH2);
  } else {
    std::puts(
        "C2-C0 OPTIONAL H1-F1/H2: UNREACHABLE IN CHARACTERIZED CORPUS");
  }

  std::puts(
      "C2-C0 A_CONTINUATION: UNREACHABLE UNDER FROZEN ATTEMPT-0 "
      "PRODUCTION SEMANTICS (source proof + exhaustive corpus zero)");
  std::puts("C2-C0 A_OVERLAP: 0; no semantic ownership ambiguity observed");
  std::puts(
      "C2-C0 DECISION A: NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE");
  std::puts("C2-C0 NEXT PRODUCER SCOPE: A-ONSET ONLY");
  return 0;
}
