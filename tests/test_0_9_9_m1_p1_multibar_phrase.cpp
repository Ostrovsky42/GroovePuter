#include <cassert>
#include <cstdio>

#include "scenes.h"
#include "src/generation/migration/strong_rhythm_migration.h"

using namespace GroovePuterRhythm;

struct Bar { DrumPatternSet drums{}; SynthPattern synthA{}, synthB{}; StrongRhythmMigrationResult result{}; };

static GenreSettings settings() {
  GenreSettings value{};
  value.generativeMode = static_cast<uint8_t>(GenerativeMode::Electro);
  value.recipe = kBaseRecipeId;
  value.rhythmSelectionMode = static_cast<uint8_t>(RhythmSelectionMode::Auto);
  value.rhythmArchetypeId = kNoArchetypeId;
  return value;
}

static StrongRhythmMigrationContext context(unsigned address, unsigned bar) {
  StrongRhythmMigrationContext value{};
  value.patternAddress = static_cast<int16_t>(address);
  value.phraseBarOrdinal = static_cast<uint8_t>(bar);
  value.level = RealizationLevel::P2Variation;
  value.feelProfile = FeelProfileId::Straight;
  value.tonalMaterializationEnabled = true;
  value.scaleTypeValue = kScaleDorian;
  return value;
}

static bool sameSynth(const SynthPattern& a, const SynthPattern& b) {
  for (int i = 0; i < SynthPattern::kSteps; ++i) {
    const SynthStep& x = a.steps[i]; const SynthStep& y = b.steps[i];
    if (x.note != y.note || x.slide != y.slide || x.accent != y.accent ||
        x.ghost != y.ghost || x.velocity != y.velocity ||
        x.timing != y.timing || x.fx != y.fx || x.fxParam != y.fxParam ||
        x.probability != y.probability) return false;
  }
  return true;
}

static unsigned notes(const SynthPattern& pattern) {
  unsigned result = 0;
  for (const SynthStep& step : pattern.steps) if (step.note >= 0) ++result;
  return result;
}

static void materialize(const StrongRhythmFrozenSelection& selection,
                        unsigned start, Bar* bars) {
  for (unsigned bar = 0; bar < 4; ++bar) {
    bars[bar].synthB.steps[0].note = 99;  // stale-destination sentinel
    const StrongRhythmMigrationContext c = context(start + bar, bar);
    bars[bar].result = migrateStrongRhythmFrozenMaterial(
        settings(), selection, c, bars[bar].drums, bars[bar].synthA,
        bars[bar].synthB);
    assert(bars[bar].result.status == StrongRhythmMigrationStatus::Applied);
  }
}

static void printBars(const char* label, const Bar* bars) {
  std::printf("%s", label);
  for (unsigned bar = 0; bar < 4; ++bar)
    std::printf(" %u", notes(bars[bar].synthB));
  std::puts("");
}

static void printPatterns(const char* label, const SynthPattern* patterns) {
  std::printf("%s", label);
  for (unsigned bar = 0; bar < 4; ++bar)
    std::printf(" %u", notes(patterns[bar]));
  std::puts("");
}

int main() {
  StrongRhythmFrozenSelection sparse{};
  const StrongRhythmMigrationResult selection =
      resolveStrongRhythmFrozenSelection(settings(), context(19, 0), 19, sparse);
  assert(selection.status == StrongRhythmMigrationStatus::Applied);
  assert(sparse.resolved && selection.synthBRole == SemanticSynthBRole::Melodic);
  assert(selection.melodicRhythmId == MelodicRhythmId::SparseCall);
  assert(selection.motifShapeId == MotifShapeId::Mirror && selection.phraseBars == 4);

  Bar rangeA[4]{}, rangeB[4]{};
  materialize(sparse, 40, rangeA);
  materialize(sparse, 120, rangeB);
  for (unsigned bar = 0; bar < 4; ++bar) {
    assert(rangeA[bar].result.archetype == rangeB[bar].result.archetype);
    assert(rangeA[bar].result.bassRhythmId == rangeB[bar].result.bassRhythmId);
    assert(rangeA[bar].result.chordRhythmId == rangeB[bar].result.chordRhythmId);
    assert(rangeA[bar].result.melodicRhythmId == rangeB[bar].result.melodicRhythmId);
    assert(rangeA[bar].result.motifShapeId == rangeB[bar].result.motifShapeId);
    assert(sameSynth(rangeA[bar].synthA, rangeB[bar].synthA));
    assert(sameSynth(rangeA[bar].synthB, rangeB[bar].synthB));
  }
  assert(notes(rangeA[0].synthB) > 0 && notes(rangeA[1].synthB) == 0);
  assert(notes(rangeA[2].synthB) > 0 && notes(rangeA[3].synthB) == 0);
  printBars("P1_P2_W", rangeA);

  StrongRhythmFrozenSelection callResponse{};
  const StrongRhythmMigrationResult p3Selection =
      resolveStrongRhythmFrozenSelection(settings(), context(7, 0), 7, callResponse);
  assert(p3Selection.status == StrongRhythmMigrationStatus::Applied);
  assert(p3Selection.synthBRole == SemanticSynthBRole::Melodic);
  assert(p3Selection.melodicRhythmId == MelodicRhythmId::DelayedAnswer);
  assert(p3Selection.motifShapeId == MotifShapeId::CallResponse);
  Bar callBars[4]{};
  materialize(callResponse, 160, callBars);
  for (const Bar& bar : callBars) assert(notes(bar.synthB) >= 2);
  printBars("P3_W", callBars);

  // C is generated once and copied, never independently regenerated.
  SynthPattern control[4]{};
  DrumPatternSet controlDrums{};
  SynthPattern controlA{};
  const StrongRhythmMigrationResult controlResult =
      migrateStrongRhythmMaterial(settings(), context(19, 0), controlDrums,
                                  controlA, control[0]);
  assert(controlResult.status == StrongRhythmMigrationStatus::Applied);
  for (unsigned bar = 1; bar < 4; ++bar) control[bar] = control[0];
  for (unsigned bar = 1; bar < 4; ++bar) assert(sameSynth(control[0], control[bar]));
  printPatterns("C", control);
  return 0;
}
