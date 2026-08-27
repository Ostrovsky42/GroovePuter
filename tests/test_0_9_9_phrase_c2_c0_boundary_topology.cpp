#include <cassert>
#include <cstdint>
#include <cstdio>

#include "scenes.h"
#include "src/generation/migration/phrase_execution.h"

using namespace GroovePuterRhythm;

namespace {

constexpr uint32_t kIdentityDomain = 65536u;
constexpr StepMask kLaterThanStepZeroMask = static_cast<StepMask>(0xFFFEu);

struct PhysicalBar {
  DrumPatternSet drums{};
  SynthPattern synthA{};
  SynthPattern synthB{};
};

struct BoundaryWitness {
  bool found = false;
  uint8_t generativeMode = 0;
  uint8_t phraseBars = 0;
  uint16_t phraseGenerationIdentity = 0;
  uint8_t boundaryBar = 0;
  MelodicRhythmId melodicRhythm = MelodicRhythmId::Auto;
  StepMask currentOnsets = 0;
  StepMask nextOnsets = 0;
};

GenreSettings genre(GenerativeMode mode) {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(mode);
  value.recipe = kBaseRecipeId;
  value.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
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

bool isNaturalBoundaryRhythm(MelodicRhythmId id) {
  return id == MelodicRhythmId::PickupPhrase ||
         id == MelodicRhythmId::BarEndResponse;
}

bool hasBoundaryTopology(const StrongRhythmMigrationResult& current,
                         const StrongRhythmMigrationResult& next,
                         SemanticSynthBRole requiredRole) {
  if (current.status != StrongRhythmMigrationStatus::Applied ||
      next.status != StrongRhythmMigrationStatus::Applied ||
      current.synthBRole != requiredRole || next.synthBRole != requiredRole ||
      !current.melodicRhythmApplied || !next.melodicRhythmApplied ||
      current.melodicMotifStatus != MelodicMotifStatus::Ok ||
      next.melodicMotifStatus != MelodicMotifStatus::Ok) {
    return false;
  }

  const bool currentOccupiesStep15 =
      (current.melodicFillOnsets & stepBit(15)) != 0;
  const bool nextStartsWithGap =
      (next.melodicFillOnsets & stepBit(0)) == 0;
  const bool nextHasLaterOnset =
      (next.melodicFillOnsets & kLaterThanStepZeroMask) != 0;
  return currentOccupiesStep15 && nextStartsWithGap && nextHasLaterOnset;
}

BoundaryWitness characterizeRole(SemanticSynthBRole role) {
  const uint8_t lengths[] = {2, 4, 8};

  for (uint8_t modeValue = 0;
       modeValue < static_cast<uint8_t>(kGenerativeModeCount);
       ++modeValue) {
    const GenreSettings settings =
        genre(static_cast<GenerativeMode>(modeValue));

    for (const uint8_t phraseBars : lengths) {
      for (uint32_t identityValue = 0; identityValue < kIdentityDomain;
           ++identityValue) {
        PhraseExecutionScratch scratch{};
        PreparedPhraseExecution prepared{};
        const uint16_t identity = static_cast<uint16_t>(identityValue);
        const PhraseExecutionStatus status = preparePhraseExecution(
            settings, materializationSettings(), identity, phraseBars,
            scratch, prepared);
        if (status != PhraseExecutionStatus::Ready) continue;

        const CompositionSecondaryRole expectedCompositionRole =
            role == SemanticSynthBRole::Melodic
                ? CompositionSecondaryRole::Melodic
                : CompositionSecondaryRole::ChordWithMelodicFill;
        if (prepared.selection.composition.secondaryRole !=
            expectedCompositionRole) {
          continue;
        }
        if (!isNaturalBoundaryRhythm(
                prepared.selection.composition.melodicRhythm)) {
          continue;
        }

        for (uint8_t boundaryBar = 0;
             static_cast<uint8_t>(boundaryBar + 1u) <
                 prepared.length.effectivePhraseBars;
             ++boundaryBar) {
          PhysicalBar currentPhysical = seededPhysicalBar();
          PhysicalBar nextPhysical = seededPhysicalBar();
          const StrongRhythmMigrationResult current =
              materializePreparedPhraseBar(
                  prepared, boundaryBar,
                  static_cast<int16_t>(80 + boundaryBar),
                  currentPhysical.drums, currentPhysical.synthA,
                  currentPhysical.synthB);
          const StrongRhythmMigrationResult next =
              materializePreparedPhraseBar(
                  prepared, static_cast<uint8_t>(boundaryBar + 1u),
                  static_cast<int16_t>(96 + boundaryBar),
                  nextPhysical.drums, nextPhysical.synthA,
                  nextPhysical.synthB);

          if (!hasBoundaryTopology(current, next, role)) continue;

          BoundaryWitness witness{};
          witness.found = true;
          witness.generativeMode = modeValue;
          witness.phraseBars = prepared.length.effectivePhraseBars;
          witness.phraseGenerationIdentity = identity;
          witness.boundaryBar = boundaryBar;
          witness.melodicRhythm =
              prepared.selection.composition.melodicRhythm;
          witness.currentOnsets = current.melodicFillOnsets;
          witness.nextOnsets = next.melodicFillOnsets;
          return witness;
        }
      }
    }
  }

  return {};
}

void printWitness(const char* label, const BoundaryWitness& witness) {
  if (!witness.found) {
    std::printf("%s none\n", label);
    return;
  }
  std::printf(
      "%s mode=%u bars=%u identity=%u boundary=%u rhythm=%s current=0x%04x next=0x%04x\n",
      label,
      static_cast<unsigned>(witness.generativeMode),
      static_cast<unsigned>(witness.phraseBars),
      static_cast<unsigned>(witness.phraseGenerationIdentity),
      static_cast<unsigned>(witness.boundaryBar),
      melodicRhythmName(witness.melodicRhythm),
      static_cast<unsigned>(witness.currentOnsets),
      static_cast<unsigned>(witness.nextOnsets));
}

}  // namespace

int main() {
  const BoundaryWitness pure = characterizeRole(SemanticSynthBRole::Melodic);
  printWitness("C2-C0 PURE", pure);

  if (pure.found) {
    assert(isNaturalBoundaryRhythm(pure.melodicRhythm));
    assert((pure.currentOnsets & stepBit(15)) != 0);
    assert((pure.nextOnsets & stepBit(0)) == 0);
    assert((pure.nextOnsets & kLaterThanStepZeroMask) != 0);
    std::puts("C2-C0 RESULT A: NATURAL PURE-MELODIC CROSSBAR TOPOLOGY REACHABLE");
    return 0;
  }

  const BoundaryWitness hybrid =
      characterizeRole(SemanticSynthBRole::ChordWithMelodicFill);
  printWitness("C2-C0 HYBRID", hybrid);
  if (hybrid.found) {
    std::puts("C2-C0 RESULT H CANDIDATE: HYBRID-ONLY WITNESS IN BASE-PROFILE SEARCH");
  } else {
    std::puts("C2-C0 RESULT NO-A: further B/C classification required before production work");
  }
  return 1;
}
