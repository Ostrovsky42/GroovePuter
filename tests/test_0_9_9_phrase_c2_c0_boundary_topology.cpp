#include <cassert>
#include <cstdint>
#include <cstdio>

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

constexpr uint32_t kTargetIdentityDomain = 8192u;
constexpr StepMask kLaterThanStepZeroMask =
    static_cast<StepMask>(kAllSteps & ~stepBit(0));

struct PhysicalBar {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
};

struct BarObservation {
  bool valid = false;
  SemanticSynthBRole role = SemanticSynthBRole::Chord;
  MelodicRhythmId melodicRhythm = MelodicRhythmId::Auto;
  MotifShapeId motifShape = MotifShapeId::Auto;
  MelodicMotifStatus melodicStatus = MelodicMotifStatus::InvalidRequest;
  MelodicPitchIntentStatus pitchStatus = MelodicPitchIntentStatus::InvalidRequest;
  StepMask kickOnsets = 0;
  StepMask bassOnsets = 0;
  StepMask chordOnsets = 0;
  StepMask chordContinuations = 0;
  StepMask protectedMelodic = 0;
  StepMask admittedOnsets = 0;
  StepMask admittedContinuations = 0;
};

struct BoundaryWitness {
  bool found = false;
  GenreSettings settings{};
  uint8_t phraseBars = 0;
  uint16_t phraseGenerationIdentity = 0;
  uint8_t boundaryBar = 0;
  StrongRhythmFrozenSelection selection{};
  BarObservation outgoing{};
  BarObservation incoming{};
};

GenreSettings genre(GenerativeMode mode, GenreRecipeId recipe = kBaseRecipeId) {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(mode);
  value.recipe = recipe;
  value.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  value.rhythmArchetypeId = kNoArchetypeId;
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

PhysicalBar seededPhysicalBar() {
  PhysicalBar value{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    value.synthA.steps[step].note = static_cast<int8_t>(36 + (step % 12));
    value.synthB.steps[step].note = static_cast<int8_t>(60 + (step % 12));
    value.synthA.steps[step].velocity = 100;
    value.synthB.steps[step].velocity = 100;
    value.synthA.steps[step].probability = 100;
    value.synthB.steps[step].probability = 100;
  }
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
  const bool allowSparse =
      sparseSemanticBarsAllowedForTest(settings, definition->family);

  BassRhythmRequest bassRequest{};
  bassRequest.requestedId = selection.composition.bassRhythm;
  bassRequest.family = definition->family;
  bassRequest.archetypeId = definition->archetypeId;
  bassRequest.kickOnsets = observed.kickOnsets;
  bassRequest.protectedSpace =
      protectedSpaceForTest(*archetype, RhythmRole::BassRhythm);
  bassRequest.generation = selection.realizationGeneration;
  bassRequest.barOrdinal = barOrdinal;
  bassRequest.allowEmptyBar = allowSparse;
  const BassRhythmResult bass = realizeBassRhythm(bassRequest);
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
  chordRequest.allowEmptyBar = allowSparse;
  const ChordRhythmResult chord = realizeChordRhythm(chordRequest);
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
  melodicRequest.allowEmptyBar = allowSparse;
  const MelodicMotifResult melodic = realizeMelodicMotif(melodicRequest);
  observed.melodicStatus = melodic.status;
  observed.melodicRhythm = melodic.plan.rhythmId;
  observed.motifShape = melodic.plan.motif.shape;
  if (melodic.status != MelodicMotifStatus::Ok &&
      melodic.status != MelodicMotifStatus::ValidButEmpty) {
    return observed;
  }

  const TonalGenerationProfile tonalProfile =
      tonalGenerationProfileFor(settings);
  MelodicPitchIntentRequest pitchRequest{};
  pitchRequest.rhythmPlan = melodic.plan;
  pitchRequest.archetypeId = definition->archetypeId;
  pitchRequest.generation = melodicRequest.generation;
  pitchRequest.barOrdinal = barOrdinal;
  pitchRequest.policy = tonalProfile.melodicPolicy;
  pitchRequest.allowedOnsetSteps = kAllSteps;
  pitchRequest.allowedContinuationSteps = kAllSteps;
  pitchRequest.allowEmptyBar = allowSparse;
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

bool isPureA(const BarObservation& outgoing,
             const BarObservation& incoming) {
  if (!outgoing.valid || !incoming.valid ||
      outgoing.role != SemanticSynthBRole::Melodic ||
      incoming.role != SemanticSynthBRole::Melodic ||
      outgoing.melodicStatus != MelodicMotifStatus::Ok ||
      incoming.melodicStatus != MelodicMotifStatus::Ok ||
      incoming.pitchStatus != MelodicPitchIntentStatus::Ok) {
    return false;
  }
  const StepMask outgoingOccupied = static_cast<StepMask>(
      outgoing.admittedOnsets | outgoing.admittedContinuations);
  return (outgoingOccupied & stepBit(15)) != 0 &&
         (incoming.admittedOnsets & stepBit(0)) == 0 &&
         (incoming.admittedOnsets & kLaterThanStepZeroMask) != 0;
}

BoundaryWitness findDefaultPureA() {
  constexpr uint8_t lengths[] = {2, 4, 8};
  const StrongRhythmMigrationContext context = selectionContext();

  for (uint8_t modeValue = 0;
       modeValue < static_cast<uint8_t>(kGenerativeModeCount);
       ++modeValue) {
    const GenreSettings settings =
        genre(static_cast<GenerativeMode>(modeValue));
    const GenerationProfileView profile = generationProfileFor(settings);
    if (!isValidGenerationProfile(profile) ||
        profile.secondaryRole != CompositionSecondaryRole::Melodic) {
      continue;
    }

    for (uint32_t identityValue = 0;
         identityValue < kTargetIdentityDomain;
         ++identityValue) {
      const uint16_t identity = static_cast<uint16_t>(identityValue);
      for (const uint8_t phraseBars : lengths) {
        StrongRhythmFrozenSelection selection{};
        PhraseLengthRequestResult length{};
        const StrongRhythmMigrationResult resolved =
            resolveStrongRhythmFrozenSelectionForPhraseBars(
                settings, context, identity, phraseBars, length, selection);
        if (resolved.status != StrongRhythmMigrationStatus::Applied ||
            length.status != PhraseLengthRequestStatus::Accepted ||
            !selection.resolved) {
          continue;
        }

        BarObservation bars[kMaxSemanticPhraseBars]{};
        bool valid = true;
        for (uint8_t bar = 0; bar < phraseBars; ++bar) {
          bars[bar] = observeProductionBar(settings, selection, bar);
          if (!bars[bar].valid) {
            valid = false;
            break;
          }
        }
        if (!valid) continue;

        for (uint8_t boundary = 0;
             static_cast<uint8_t>(boundary + 1u) < phraseBars;
             ++boundary) {
          if (!isPureA(bars[boundary], bars[boundary + 1u])) continue;
          BoundaryWitness witness{};
          witness.found = true;
          witness.settings = settings;
          witness.phraseBars = phraseBars;
          witness.phraseGenerationIdentity = identity;
          witness.boundaryBar = boundary;
          witness.selection = selection;
          witness.outgoing = bars[boundary];
          witness.incoming = bars[boundary + 1u];
          return witness;
        }
      }
    }
  }
  return {};
}

void validateThroughP1R(const BoundaryWitness& witness) {
  assert(witness.found);
  PhraseExecutionScratch scratch{};
  PreparedPhraseExecution prepared{};
  const PhraseExecutionStatus prepareStatus = preparePhraseExecution(
      witness.settings, materializationSettings(),
      witness.phraseGenerationIdentity, witness.phraseBars,
      scratch, prepared);
  assert(prepareStatus == PhraseExecutionStatus::Ready);
  assert(prepared.status == PhraseExecutionStatus::Ready);
  assert(prepared.selection.composition.secondaryRole ==
         CompositionSecondaryRole::Melodic);
  assert(prepared.selection.composition.melodicRhythm ==
         witness.selection.composition.melodicRhythm);
  assert(prepared.selection.composition.motifShape ==
         witness.selection.composition.motifShape);

  PhysicalBar outgoing = seededPhysicalBar();
  PhysicalBar incoming = seededPhysicalBar();
  const StrongRhythmMigrationResult outgoingResult =
      materializePreparedPhraseBar(
          prepared, witness.boundaryBar, 120,
          outgoing.drums, outgoing.synthA, outgoing.synthB);
  const StrongRhythmMigrationResult incomingResult =
      materializePreparedPhraseBar(
          prepared, static_cast<uint8_t>(witness.boundaryBar + 1u), 121,
          incoming.drums, incoming.synthA, incoming.synthB);
  assert(outgoingResult.status == StrongRhythmMigrationStatus::Applied);
  assert(incomingResult.status == StrongRhythmMigrationStatus::Applied);
  assert(outgoingResult.synthBRole == SemanticSynthBRole::Melodic);
  assert(incomingResult.synthBRole == SemanticSynthBRole::Melodic);
  assert(outgoingResult.melodicRhythmId == witness.outgoing.melodicRhythm);
  assert(incomingResult.melodicRhythmId == witness.incoming.melodicRhythm);
  assert(outgoingResult.motifShapeId == witness.outgoing.motifShape);
  assert(incomingResult.motifShapeId == witness.incoming.motifShape);
  assert(outgoingResult.melodicMotifStatus == witness.outgoing.melodicStatus);
  assert(incomingResult.melodicMotifStatus == witness.incoming.melodicStatus);

  if ((witness.outgoing.admittedOnsets & stepBit(15)) != 0) {
    assert(outgoing.synthB.steps[15].note >= 0);
  }
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((witness.incoming.admittedOnsets & stepBit(step)) == 0) continue;
    assert(incoming.synthB.steps[step].note >= 0);
  }
}

void printWitness(const BoundaryWitness& witness) {
  if (!witness.found) {
    std::puts("C2-C0 TARGET PURE A: none in bounded default search");
    return;
  }
  std::printf(
      "C2-C0 TARGET PURE A: mode=%u recipe=%u bars=%u identity=%u "
      "boundary=%u->%u archetype=%u progression=%u rhythm=%s motif=%s "
      "out_on=0x%04x out_cont=0x%04x in_on=0x%04x in_cont=0x%04x\n",
      static_cast<unsigned>(witness.settings.generativeMode),
      static_cast<unsigned>(witness.settings.recipe),
      static_cast<unsigned>(witness.phraseBars),
      static_cast<unsigned>(witness.phraseGenerationIdentity),
      static_cast<unsigned>(witness.boundaryBar),
      static_cast<unsigned>(witness.boundaryBar + 1u),
      static_cast<unsigned>(witness.selection.composition.rhythmArchetypeId),
      static_cast<unsigned>(witness.selection.composition.progression),
      melodicRhythmName(witness.outgoing.melodicRhythm),
      motifShapeName(witness.outgoing.motifShape),
      static_cast<unsigned>(witness.outgoing.admittedOnsets),
      static_cast<unsigned>(witness.outgoing.admittedContinuations),
      static_cast<unsigned>(witness.incoming.admittedOnsets),
      static_cast<unsigned>(witness.incoming.admittedContinuations));
}

}  // namespace

int main() {
  const BoundaryWitness witness = findDefaultPureA();
  printWitness(witness);
  assert(witness.found);
  assert(isPureA(witness.outgoing, witness.incoming));
  validateThroughP1R(witness);

  const bool onset15 =
      (witness.outgoing.admittedOnsets & stepBit(15)) != 0;
  const bool continuation15 =
      (witness.outgoing.admittedContinuations & stepBit(15)) != 0;
  assert(onset15 || continuation15);
  if (onset15 && continuation15) {
    std::puts("C2-C0 TARGET SUBCLASS: A_OVERLAP");
  } else if (onset15) {
    std::puts("C2-C0 TARGET SUBCLASS: A_ONSET");
  } else {
    std::puts("C2-C0 TARGET SUBCLASS: A_CONTINUATION");
  }
  std::puts("C2-C0 TARGET RESULT A: NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE");
  return 0;
}
