#include <cstdint>
#include <cstdio>
#include <fstream>

#include "scenes.h"
#include "src/dsp/genre_manager.h"
#include "src/generation/composition/generation_profile.h"
#include "src/generation/composition/rhythm_selection.h"
#include "src/generation/composition/tonal_profile.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "src/generation/rhythm/reference_vocabulary.h"
#include "src/generation/roles/bass_pitch_behavior.h"
#include "src/generation/roles/bass_rhythm.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint16_t kHistoricalIdentityCount = 128;
constexpr uint16_t kReachabilityIdentityCount = 512;
constexpr uint8_t kOwnerExpected = 3;
constexpr uint8_t kLevelCount = 3;
constexpr uint8_t kMaxBassCandidates = 16;
constexpr uint16_t kHistoricalRows =
    static_cast<uint16_t>(kHistoricalIdentityCount * kOwnerExpected * kLevelCount);

constexpr RealizationLevel kLevels[] = {
    RealizationLevel::P1Canonical,
    RealizationLevel::P2Variation,
    RealizationLevel::P3Transformation,
};

enum class ArticulationClass : uint8_t {
  Flattened = 0,
  Detached,
  Connected,
};

struct SemanticCandidate {
  uint8_t id = 0;
  uint16_t weight = 0;
  ArticulationClass articulation = ArticulationClass::Flattened;
};

struct SemanticOwnerModel {
  const char* label = nullptr;
  uint8_t kickRelationship = 0;
  int16_t gridWindow = 0;
  uint8_t count = 0;
  SemanticCandidate candidates[kMaxBassCandidates]{};
};

struct OwnerCapability {
  bool detachedSelectable = false;
  bool connectedSelectable = false;
  bool detachedReachable[kLevelCount]{};
  bool connectedReachable[kLevelCount]{};
  uint16_t detachedWitness[kLevelCount]{};
  uint16_t connectedWitness[kLevelCount]{};
};

struct CorpusBucket {
  uint16_t rows = 0;
  uint16_t allDetached = 0;
  uint16_t allConnected = 0;
  uint16_t mixed = 0;
  uint16_t continuationRows = 0;
  uint16_t continuationEvents = 0;
  uint16_t slideRows = 0;
};

struct CorpusEvidence {
  uint16_t attempted = 0;
  uint16_t successful = 0;
  uint16_t failed = 0;
  uint16_t allDetached = 0;
  uint16_t allConnected = 0;
  uint16_t mixed = 0;
  uint16_t continuationRows = 0;
  uint16_t continuationEvents = 0;
  uint16_t slideRows = 0;
  CorpusBucket bucket[kOwnerExpected][kLevelCount]{};
};

uint8_t popcount16(uint16_t value) {
  uint8_t count = 0;
  while (value != 0) {
    value = static_cast<uint16_t>(value & static_cast<uint16_t>(value - 1u));
    ++count;
  }
  return count;
}

const char* ownerName(GenreRecipeId recipe) {
  switch (recipe) {
    case 0: return "BASE";
    case 6: return "CHICAGO_JACK";
    case 7: return "ROLLING_ACID";
    default: return "UNKNOWN";
  }
}

const char* levelName(RealizationLevel level) {
  switch (level) {
    case RealizationLevel::P1Canonical: return "P1";
    case RealizationLevel::P2Variation: return "P2";
    case RealizationLevel::P3Transformation: return "P3";
    case RealizationLevel::Count: break;
  }
  return "INVALID";
}

GenreSettings settingsFor(GenreRecipeId recipe) {
  GenreSettings settings{};
  settings.generativeMode = static_cast<uint8_t>(GenerativeMode::Acid);
  settings.recipe = recipe;
  settings.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  return settings;
}

StrongRhythmMigrationContext contextFor(RealizationLevel level) {
  StrongRhythmMigrationContext context{};
  context.patternAddress = 0;
  context.level = level;
  context.generationAttemptOrdinal = 0;
  context.feelProfile = FeelProfileId::Straight;
  context.feelAmount = 0;
  context.tonalMaterializationEnabled = true;
  context.rootPitchClass = 0;
  context.scaleTypeValue = kScaleDorian;
  return context;
}

const RhythmArchetype* archetypeForId(RhythmArchetypeId id) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(id);
  return definition == nullptr ? nullptr
                               : ReferenceVocabulary::archetypeFor(definition->key);
}

StepMask protectedSpaceFor(const RhythmArchetype& archetype,
                           RhythmRole role) {
  StepMask result = 0;
  const RhythmRoleMask bit = rhythmRoleBit(role);
  for (uint8_t index = 0; index < archetype.protectedSpaceCount; ++index) {
    if ((archetype.protectedSpaces[index].affectedRoles & bit) != 0) {
      result = static_cast<StepMask>(result | archetype.protectedSpaces[index].steps);
    }
  }
  return result;
}

StepMask kickOnsets(const DrumPatternSet& drums) {
  StepMask result = 0;
  for (uint8_t step = 0; step < DrumPattern::kSteps; ++step) {
    if (drums.voices[KICK].steps[step].hit) {
      result = static_cast<StepMask>(result | stepBit(step));
    }
  }
  return result;
}

bool acidAllowsSparseBar(RhythmFamily family) {
  return family == RhythmFamily::SparsePulse;
}

bool reconstructBass(const GenreSettings& settings,
                     const StrongRhythmFrozenSelection& selection,
                     const DrumPatternSet& drums,
                     BassRhythmPlan& rhythm,
                     BassPitchBehaviorPlan& pitch) {
  const ReferenceVocabulary::Definition* definition =
      ReferenceVocabulary::definitionForId(selection.composition.rhythmArchetypeId);
  const RhythmArchetype* archetype =
      archetypeForId(selection.composition.rhythmArchetypeId);
  if (definition == nullptr || archetype == nullptr) return false;

  BassRhythmRequest rhythmRequest{};
  rhythmRequest.requestedId = selection.composition.bassRhythm;
  rhythmRequest.family = definition->family;
  rhythmRequest.archetypeId = definition->archetypeId;
  rhythmRequest.kickOnsets = kickOnsets(drums);
  rhythmRequest.protectedSpace = protectedSpaceFor(*archetype, RhythmRole::BassRhythm);
  rhythmRequest.generation = selection.realizationGeneration;
  rhythmRequest.barOrdinal = 0;
  rhythmRequest.allowEmptyBar = acidAllowsSparseBar(definition->family);
  const BassRhythmResult rhythmResult = realizeBassRhythm(rhythmRequest);
  if (rhythmResult.status != BassRhythmStatus::Ok &&
      rhythmResult.status != BassRhythmStatus::ValidButEmpty) {
    return false;
  }

  BassPitchBehaviorRequest pitchRequest{};
  pitchRequest.rhythmPlan = rhythmResult.plan;
  pitchRequest.archetypeId = definition->archetypeId;
  pitchRequest.generation = rhythmRequest.generation;
  pitchRequest.barOrdinal = 0;
  pitchRequest.policy = tonalGenerationProfileFor(settings).bassPolicy;
  const BassPitchBehaviorResult pitchResult = realizeBassPitchBehavior(pitchRequest);
  if (pitchResult.status != BassPitchBehaviorStatus::Ok &&
      pitchResult.status != BassPitchBehaviorStatus::ValidButEmpty) {
    return false;
  }

  rhythm = rhythmResult.plan;
  pitch = pitchResult.plan;
  return true;
}

bool qualifiesContinuation(bool semanticOnset,
                           bool semanticContinuation,
                           bool predecessorActive,
                           bool predecessorSameOwner,
                           bool physicalNotePresent,
                           bool physicalContinuationCarrier) {
  return !semanticOnset && semanticContinuation && predecessorActive &&
         predecessorSameOwner && physicalNotePresent &&
         physicalContinuationCarrier;
}

bool continuationTopologyFaithful(const BassPitchBehaviorPlan& pitch,
                                  const SynthPattern& synthA) {
  if ((pitch.onsets & pitch.continuations) != 0) return false;
  bool active = false;
  for (uint8_t step = 0; step < SynthPattern::kSteps; ++step) {
    const StepMask bit = stepBit(step);
    const bool onset = (pitch.onsets & bit) != 0;
    const bool continuation = (pitch.continuations & bit) != 0;
    const bool notePresent = synthA.steps[step].note >= 0;
    const bool carrier = synthA.steps[step].slide;

    if (onset) {
      if (!notePresent) return false;
      active = true;
      continue;
    }
    if (continuation) {
      if (!qualifiesContinuation(false, true, active, true,
                                 notePresent, carrier)) {
        return false;
      }
      continue;
    }
    if (notePresent) return false;
    active = false;
  }
  return true;
}

bool falsePositiveContinuationDefense() {
  const bool positive = qualifiesContinuation(false, true, true, true, true, true);
  const bool freshOnset = qualifiesContinuation(true, false, false, true, true, false);
  const bool slideOnset = qualifiesContinuation(true, false, false, true, true, true);
  const bool stalePrevious = qualifiesContinuation(false, true, false, true, true, true);
  const bool unrelatedHeld = qualifiesContinuation(false, false, true, false, true, true);
  const bool ghost = qualifiesContinuation(false, true, true, true, false, true);
  const bool ownershipLeak = qualifiesContinuation(false, true, true, false, true, true);
  const bool pass = positive && !freshOnset && !slideOnset && !stalePrevious &&
                    !unrelatedHeld && !ghost && !ownershipLeak;
  std::printf(
      "G4_CC1A_P1_FALSE_POSITIVE_DEFENSE fresh_onset=%s slide_onset_without_predecessor=%s stale_previous=%s unrelated_held=%s ghost_note=%s ownership_leak=%s positive_control=%s\n",
      !freshOnset ? "PASS" : "FAIL", !slideOnset ? "PASS" : "FAIL",
      !stalePrevious ? "PASS" : "FAIL", !unrelatedHeld ? "PASS" : "FAIL",
      !ghost ? "PASS" : "FAIL", !ownershipLeak ? "PASS" : "FAIL",
      positive ? "PASS" : "FAIL");
  return pass;
}

bool materializeIdentity(const GenreSettings& settings,
                         RealizationLevel level,
                         uint16_t identity,
                         StrongRhythmFrozenSelection& selection,
                         BassPitchBehaviorPlan& pitch,
                         SynthPattern& synthA) {
  StrongRhythmMigrationContext context = contextFor(level);
  const StrongRhythmMigrationResult selected =
      resolveStrongRhythmFrozenSelection(settings, context, identity, selection);
  if (selected.status != StrongRhythmMigrationStatus::Applied || !selection.resolved) {
    return false;
  }

  DrumPatternSet drums{};
  SynthPattern synthB{};
  const StrongRhythmMigrationResult realized =
      migrateStrongRhythmFrozenMaterial(settings, selection, context,
                                        drums, synthA, synthB);
  if (realized.status != StrongRhythmMigrationStatus::Applied) return false;

  BassRhythmPlan rhythm{};
  if (!reconstructBass(settings, selection, drums, rhythm, pitch)) return false;
  if (rhythm.id != realized.bassRhythmId ||
      pitch.contour != realized.bassPitchContour ||
      pitch.continuations != rhythm.continuations) {
    return false;
  }
  return continuationTopologyFaithful(pitch, synthA);
}

bool identityCanContinue(const GenreSettings& settings, BassRhythmId id) {
  const RhythmCompatibilityView compatibility = rhythmCompatibilityFor(settings);
  if (compatibility.candidates == nullptr) return false;

  for (uint8_t candidateIndex = 0; candidateIndex < compatibility.count;
       ++candidateIndex) {
    const ReferenceVocabulary::Definition* definition =
        ReferenceVocabulary::definitionForId(
            compatibility.candidates[candidateIndex].archetypeId);
    const RhythmArchetype* archetype =
        archetypeForId(compatibility.candidates[candidateIndex].archetypeId);
    if (definition == nullptr || archetype == nullptr) return false;

    for (uint8_t bar = 0; bar < 4; ++bar) {
      BassRhythmRequest request{};
      request.requestedId = id;
      request.family = definition->family;
      request.archetypeId = definition->archetypeId;
      request.kickOnsets = static_cast<StepMask>(
          stepBit(0) | stepBit(4) | stepBit(8) | stepBit(12));
      request.protectedSpace = protectedSpaceFor(*archetype, RhythmRole::BassRhythm);
      request.generation.projectSeed = 0x43433150u;
      request.generation.phraseOrdinal = 1;
      request.barOrdinal = bar;
      request.allowEmptyBar = acidAllowsSparseBar(definition->family);
      const BassRhythmResult result = realizeBassRhythm(request);
      if ((result.status == BassRhythmStatus::Ok ||
           result.status == BassRhythmStatus::ValidButEmpty) &&
          result.plan.continuations != 0) {
        return true;
      }
    }
  }
  return false;
}

bool articulationCapability(const SemanticOwnerModel& model) {
  bool detached = false;
  bool connected = false;
  for (uint8_t index = 0; index < model.count; ++index) {
    detached = detached ||
               model.candidates[index].articulation == ArticulationClass::Detached;
    connected = connected ||
                model.candidates[index].articulation == ArticulationClass::Connected;
  }
  return detached && connected;
}

bool buildSemanticOwnerModel(const GenreSettings& settings,
                             SemanticOwnerModel& model) {
  const GenerationProfileView profile = generationProfileFor(settings);
  if (profile.bassRhythms.candidates == nullptr || profile.bassRhythms.count == 0 ||
      profile.bassRhythms.count > kMaxBassCandidates) {
    return false;
  }
  model.label = ownerName(settings.recipe);
  model.kickRelationship = 0;
  model.gridWindow = 3;
  model.count = profile.bassRhythms.count;
  for (uint8_t index = 0; index < profile.bassRhythms.count; ++index) {
    const WeightedIdentityCandidate candidate = profile.bassRhythms.candidates[index];
    const BassRhythmId id = static_cast<BassRhythmId>(candidate.id);
    model.candidates[index].id = candidate.id;
    model.candidates[index].weight = candidate.weight;
    model.candidates[index].articulation = identityCanContinue(settings, id)
        ? ArticulationClass::Connected
        : ArticulationClass::Detached;
  }
  return true;
}

bool adversarialControls(const GenreSettings& settings) {
  SemanticOwnerModel baseline{};
  if (!buildSemanticOwnerModel(settings, baseline) ||
      !articulationCapability(baseline)) {
    return false;
  }

  SemanticOwnerModel flatten = baseline;
  for (uint8_t i = 0; i < flatten.count; ++i) {
    flatten.candidates[i].articulation = ArticulationClass::Flattened;
  }
  const bool flattenFails = !articulationCapability(flatten);

  SemanticOwnerModel noConnected = baseline;
  for (uint8_t i = 0; i < noConnected.count; ++i) {
    if (noConnected.candidates[i].articulation == ArticulationClass::Connected) {
      noConnected.candidates[i].articulation = ArticulationClass::Flattened;
    }
  }
  const bool removeConnectedFails = !articulationCapability(noConnected);

  SemanticOwnerModel noDetached = baseline;
  for (uint8_t i = 0; i < noDetached.count; ++i) {
    if (noDetached.candidates[i].articulation == ArticulationClass::Detached) {
      noDetached.candidates[i].articulation = ArticulationClass::Flattened;
    }
  }
  const bool removeDetachedFails = !articulationCapability(noDetached);

  SemanticOwnerModel labelMutation = baseline;
  labelMutation.label = "MUTATED_DISPLAY_LABEL";
  const bool labelIndependent = articulationCapability(labelMutation);

  SemanticOwnerModel weightMutation = baseline;
  for (uint8_t i = 0; i < weightMutation.count; ++i) {
    weightMutation.candidates[i].weight =
        static_cast<uint16_t>(1u + static_cast<uint16_t>(i) * 997u);
  }
  const bool weightIndependent = articulationCapability(weightMutation);

  bool kickIndependent = true;
  for (uint8_t relationship = 0; relationship < 4; ++relationship) {
    SemanticOwnerModel mutation = baseline;
    mutation.kickRelationship = relationship;
    kickIndependent = kickIndependent && articulationCapability(mutation);
  }

  bool gridIndependent = true;
  constexpr int16_t kGridMutations[] = {-8, 0, 1, 3, 7, 16};
  for (int16_t window : kGridMutations) {
    SemanticOwnerModel mutation = baseline;
    mutation.gridWindow = window;
    gridIndependent = gridIndependent && articulationCapability(mutation);
  }

  const bool pass = flattenFails && removeConnectedFails && removeDetachedFails &&
                    labelIndependent && weightIndependent && kickIndependent &&
                    gridIndependent;
  std::printf(
      "G4_CC1A_P1_ADVERSARIAL owner=%s flatten_articulation=%s remove_connected_path=%s remove_detached_path=%s label_independence=%s weight_independence=%s kick_independence=%s grid_independence=%s\n",
      ownerName(settings.recipe), flattenFails ? "PASS" : "FAIL",
      removeConnectedFails ? "PASS" : "FAIL",
      removeDetachedFails ? "PASS" : "FAIL",
      labelIndependent ? "PASS" : "FAIL",
      weightIndependent ? "PASS" : "FAIL",
      kickIndependent ? "PASS" : "FAIL",
      gridIndependent ? "PASS" : "FAIL");
  return pass;
}

bool collectOwnerCapability(const GenreSettings& settings,
                            OwnerCapability& capability) {
  SemanticOwnerModel semantic{};
  if (!buildSemanticOwnerModel(settings, semantic)) return false;

  std::printf("G4_CC1A_P1_SELECTOR owner=%s", ownerName(settings.recipe));
  for (uint8_t index = 0; index < semantic.count; ++index) {
    const SemanticCandidate& candidate = semantic.candidates[index];
    const BassRhythmId id = static_cast<BassRhythmId>(candidate.id);
    const bool connected = candidate.articulation == ArticulationClass::Connected;
    capability.connectedSelectable = capability.connectedSelectable || connected;
    capability.detachedSelectable = capability.detachedSelectable || !connected;
    std::printf(" id=%u:%s:w%u:%s", static_cast<unsigned>(candidate.id),
                bassRhythmName(id), static_cast<unsigned>(candidate.weight),
                connected ? "CONNECTED" : "DETACHED");
  }
  std::putchar('\n');

  if (!articulationCapability(semantic)) return false;

  for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
    for (uint16_t identity = 1; identity <= kReachabilityIdentityCount; ++identity) {
      StrongRhythmFrozenSelection selection{};
      BassPitchBehaviorPlan pitch{};
      SynthPattern synthA{};
      if (!materializeIdentity(settings, kLevels[levelIndex], identity,
                               selection, pitch, synthA)) {
        return false;
      }
      const bool connected = pitch.continuations != 0;
      const bool detached = pitch.onsets != 0 && pitch.continuations == 0;
      if (detached && !capability.detachedReachable[levelIndex]) {
        capability.detachedReachable[levelIndex] = true;
        capability.detachedWitness[levelIndex] = identity;
      }
      if (connected && !capability.connectedReachable[levelIndex]) {
        capability.connectedReachable[levelIndex] = true;
        capability.connectedWitness[levelIndex] = identity;
      }
      if (capability.detachedReachable[levelIndex] &&
          capability.connectedReachable[levelIndex]) {
        break;
      }
    }

    std::printf(
        "G4_CC1A_P1_REACH owner=%s level=%s detached=%u detached_witness=%u connected=%u connected_witness=%u search=1..%u\n",
        ownerName(settings.recipe), levelName(kLevels[levelIndex]),
        capability.detachedReachable[levelIndex] ? 1u : 0u,
        capability.detachedWitness[levelIndex],
        capability.connectedReachable[levelIndex] ? 1u : 0u,
        capability.connectedWitness[levelIndex], kReachabilityIdentityCount);
  }

  return true;
}

bool ownerCapabilityPass(const OwnerCapability& capability) {
  bool pass = capability.detachedSelectable && capability.connectedSelectable;
  for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
    pass = pass && capability.detachedReachable[levelIndex] &&
           capability.connectedReachable[levelIndex];
  }
  return pass;
}

bool collectHistoricalCorpus(CorpusEvidence& evidence,
                             const OwnerCapability capabilities[kOwnerExpected],
                             const char* censusPath) {
  std::ofstream census;
  if (censusPath != nullptr) {
    census.open(censusPath);
    if (!census) return false;
    census << "owner\trecipe\tidentity\tlevel\tarchetype\tbass_identity\tonset_mask\tcontinuation_mask\tattack_count\tcontinuation_count\tslide_onsets\tclass\towner_contract\n";
  }

  const uint8_t ownerCount = availableRecipeCount(GenerativeMode::Acid);
  if (ownerCount != kOwnerExpected) return false;
  for (uint8_t ownerIndex = 0; ownerIndex < ownerCount; ++ownerIndex) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ownerIndex, recipe)) return false;
    const GenreSettings settings = settingsFor(recipe);
    const bool contractPass = ownerCapabilityPass(capabilities[ownerIndex]);

    for (uint16_t identity = 1; identity <= kHistoricalIdentityCount; ++identity) {
      for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
        ++evidence.attempted;
        StrongRhythmFrozenSelection selection{};
        BassPitchBehaviorPlan pitch{};
        SynthPattern synthA{};
        if (!materializeIdentity(settings, kLevels[levelIndex], identity,
                                 selection, pitch, synthA)) {
          ++evidence.failed;
          continue;
        }

        ++evidence.successful;
        CorpusBucket& bucket = evidence.bucket[ownerIndex][levelIndex];
        ++bucket.rows;
        const uint8_t attacks = popcount16(pitch.onsets);
        const uint8_t continuations = popcount16(pitch.continuations);
        const uint8_t slides = popcount16(pitch.slideIntoOnsets);
        const bool allDetached = attacks != 0 && continuations == 0;
        const bool allConnected = attacks == 1 && continuations != 0;
        const bool mixed = attacks > 1 && continuations != 0;
        const char* classification = "EMPTY";
        if (allDetached) classification = "DETACHED";
        if (allConnected) classification = "CONNECTED";
        if (mixed) classification = "MIXED";

        if (allDetached) {
          ++bucket.allDetached;
          ++evidence.allDetached;
        }
        if (allConnected) {
          ++bucket.allConnected;
          ++evidence.allConnected;
        }
        if (mixed) {
          ++bucket.mixed;
          ++evidence.mixed;
        }
        if (continuations != 0) {
          ++bucket.continuationRows;
          ++evidence.continuationRows;
          bucket.continuationEvents = static_cast<uint16_t>(
              bucket.continuationEvents + continuations);
          evidence.continuationEvents = static_cast<uint16_t>(
              evidence.continuationEvents + continuations);
        }
        if (slides != 0) {
          ++bucket.slideRows;
          ++evidence.slideRows;
        }

        if (census.is_open()) {
          census << ownerName(recipe) << '\t'
                 << static_cast<unsigned>(recipe) << '\t'
                 << identity << '\t' << levelName(kLevels[levelIndex]) << '\t'
                 << static_cast<unsigned>(selection.composition.rhythmArchetypeId) << '\t'
                 << static_cast<unsigned>(selection.composition.bassRhythm) << '\t'
                 << static_cast<unsigned>(pitch.onsets) << '\t'
                 << static_cast<unsigned>(pitch.continuations) << '\t'
                 << static_cast<unsigned>(attacks) << '\t'
                 << static_cast<unsigned>(continuations) << '\t'
                 << static_cast<unsigned>(slides) << '\t'
                 << classification << '\t'
                 << (contractPass ? "PASS" : "FAIL") << '\n';
        }
      }
    }
  }

  for (uint8_t ownerIndex = 0; ownerIndex < ownerCount; ++ownerIndex) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ownerIndex, recipe)) return false;
    for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
      const CorpusBucket& bucket = evidence.bucket[ownerIndex][levelIndex];
      std::printf(
          "G4_CC1A_P1_DIST owner=%s level=%s rows=%u all_detached=%u all_connected=%u mixed=%u continuation_rows=%u continuation_events=%u slide_rows=%u\n",
          ownerName(recipe), levelName(kLevels[levelIndex]), bucket.rows,
          bucket.allDetached, bucket.allConnected, bucket.mixed,
          bucket.continuationRows, bucket.continuationEvents, bucket.slideRows);
    }
  }

  std::printf(
      "G4_CC1A_P1_CORPUS attempted=%u successful=%u failed=%u all_detached=%u all_connected=%u mixed=%u continuation_rows=%u continuation_events=%u slide_rows=%u\n",
      evidence.attempted, evidence.successful, evidence.failed,
      evidence.allDetached, evidence.allConnected, evidence.mixed,
      evidence.continuationRows, evidence.continuationEvents, evidence.slideRows);

  return evidence.attempted == kHistoricalRows &&
         evidence.successful == kHistoricalRows && evidence.failed == 0 &&
         evidence.allDetached > 0 && evidence.continuationRows > 0;
}

}  // namespace

int main(int argc, char** argv) {
  bool ok = falsePositiveContinuationDefense();
  const uint8_t ownerCount = availableRecipeCount(GenerativeMode::Acid);
  if (ownerCount != kOwnerExpected) {
    std::printf("G4_CC1A_P1_FAIL owner_count=%u expected=%u\n",
                ownerCount, kOwnerExpected);
    return 1;
  }

  OwnerCapability capabilities[kOwnerExpected]{};
  bool pLevelPass[kLevelCount] = {true, true, true};
  for (uint8_t ownerIndex = 0; ownerIndex < ownerCount; ++ownerIndex) {
    GenreRecipeId recipe = 0;
    if (!availableRecipeAt(GenerativeMode::Acid, ownerIndex, recipe)) return 1;
    const GenreSettings settings = settingsFor(recipe);
    if (!collectOwnerCapability(settings, capabilities[ownerIndex])) {
      std::printf("G4_CC1A_P1_FAIL owner=%s collection_error\n", ownerName(recipe));
      return 1;
    }

    const bool ownerPass = ownerCapabilityPass(capabilities[ownerIndex]);
    for (uint8_t levelIndex = 0; levelIndex < kLevelCount; ++levelIndex) {
      pLevelPass[levelIndex] = pLevelPass[levelIndex] &&
          capabilities[ownerIndex].detachedReachable[levelIndex] &&
          capabilities[ownerIndex].connectedReachable[levelIndex];
    }
    std::printf(
        "ACID_ARTICULATION_%s owner=%s detached_selectable=%u connected_selectable=%u detached_reachable=%u connected_reachable=%u\n",
        ownerPass ? "GREEN" : "RED", ownerName(recipe),
        capabilities[ownerIndex].detachedSelectable ? 1u : 0u,
        capabilities[ownerIndex].connectedSelectable ? 1u : 0u,
        (capabilities[ownerIndex].detachedReachable[0] &&
         capabilities[ownerIndex].detachedReachable[1] &&
         capabilities[ownerIndex].detachedReachable[2]) ? 1u : 0u,
        (capabilities[ownerIndex].connectedReachable[0] &&
         capabilities[ownerIndex].connectedReachable[1] &&
         capabilities[ownerIndex].connectedReachable[2]) ? 1u : 0u);
    ok = ownerPass && adversarialControls(settings) && ok;
  }

  std::printf("G4_CC1A_P1_PLEVEL P1=%s P2=%s P3=%s\n",
              pLevelPass[0] ? "PASS" : "FAIL",
              pLevelPass[1] ? "PASS" : "FAIL",
              pLevelPass[2] ? "PASS" : "FAIL");
  ok = pLevelPass[0] && pLevelPass[1] && pLevelPass[2] && ok;

  CorpusEvidence corpus{};
  if (!collectHistoricalCorpus(corpus, capabilities,
                               argc > 1 ? argv[1] : nullptr)) {
    std::puts("G4_CC1A_P1_FAIL corpus_collection");
    ok = false;
  }

  const bool monophony = corpus.failed == 0 && corpus.continuationRows > 0;
  std::printf("G4_CC1A_P1_MONOPHONY %s continuation_rows=%u\n",
              monophony ? "PASS" : "FAIL", corpus.continuationRows);
  ok = monophony && ok;

  if (!ok) {
    std::puts("G4-CC1A-P1 Acid articulation authority restoration: RED");
    return 1;
  }

  std::puts("G4-CC1A-P1 Acid articulation authority restoration: PASS");
  return 0;
}
