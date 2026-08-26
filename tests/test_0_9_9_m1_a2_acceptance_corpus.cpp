#include <cassert>
#include <cstdio>

#include "scenes.h"
#include "src/generation/migration/strong_rhythm_migration.h"
#include "pairs.h"

using namespace GroovePuterRhythm;

struct Captured {
  StrongRhythmMigrationResult result{};
  StrongRhythmMelodicRequestProbe probe{};
};

static Captured capture(unsigned mode, unsigned recipe, unsigned address) {
  GenreSettings settings{};
  settings.generativeMode = mode;
  settings.recipe = recipe;
  settings.rhythmSelectionMode =
      static_cast<uint8_t>(RhythmSelectionMode::Auto);
  settings.rhythmArchetypeId = kNoArchetypeId;
  StrongRhythmMigrationContext context{};
  context.patternAddress = address;
  context.level = RealizationLevel::P2Variation;
  context.feelProfile = FeelProfileId::Straight;
  context.tonalMaterializationEnabled = true;
  context.scaleTypeValue = kScaleDorian;
  DrumPatternSet drums{};
  SynthPattern synthA{}, synthB{};
  Captured captured{};
  setStrongRhythmMelodicRequestProbe(&captured.probe);
  captured.result =
      migrateStrongRhythmMaterial(settings, context, drums, synthA, synthB);
  setStrongRhythmMelodicRequestProbe(nullptr);
  assert(captured.result.status == StrongRhythmMigrationStatus::Applied);
  assert(captured.probe.captured);
  return captured;
}

struct SemanticBars {
  MelodicMotifResult bars[4]{};
  unsigned distinct = 0;
};

static bool equivalent(const MelodicMotifResult& left,
                       const MelodicMotifResult& right) {
  if (left.status != right.status || left.plan.rhythmId != right.plan.rhythmId ||
      left.plan.onsets != right.plan.onsets ||
      left.plan.continuations != right.plan.continuations ||
      left.plan.motif.shape != right.plan.motif.shape ||
      left.plan.motif.sourceOrderCount != right.plan.motif.sourceOrderCount) {
    return false;
  }
  for (uint8_t i = 0; i < left.plan.motif.sourceOrderCount; ++i) {
    if (left.plan.motif.sourceOrder[i] != right.plan.motif.sourceOrder[i]) {
      return false;
    }
  }
  return true;
}

static SemanticBars replay(const StrongRhythmMelodicRequestProbe& probe) {
  SemanticBars output{};
  for (unsigned ordinal = 0; ordinal < 4; ++ordinal) {
    MelodicMotifRequest request = probe.request;
    request.barOrdinal = ordinal;
    assert(request.requestedRhythm == probe.request.requestedRhythm);
    assert(request.requestedShape == probe.request.requestedShape);
    assert(request.family == probe.request.family);
    assert(request.archetypeId == probe.request.archetypeId);
    assert(request.bassOnsets == probe.request.bassOnsets);
    assert(request.chordOnsets == probe.request.chordOnsets);
    assert(request.protectedSpace == probe.request.protectedSpace);
    assert(request.generation.projectSeed == probe.request.generation.projectSeed);
    assert(request.generation.phraseOrdinal == probe.request.generation.phraseOrdinal);
    assert(request.allowEmptyBar == probe.request.allowEmptyBar);
    output.bars[ordinal] = realizeMelodicMotif(request);
    assert(output.bars[ordinal].status == MelodicMotifStatus::Ok ||
           output.bars[ordinal].status == MelodicMotifStatus::ValidButEmpty);
  }
  output.distinct = 1;
  for (unsigned ordinal = 1; ordinal < 4; ++ordinal) {
    bool seen = false;
    for (unsigned earlier = 0; earlier < ordinal; ++earlier) {
      seen = seen || equivalent(output.bars[ordinal], output.bars[earlier]);
    }
    if (!seen) ++output.distinct;
  }
  return output;
}

static void printRow(const char* property, unsigned mode, unsigned recipe,
                     unsigned address, const Captured& captured,
                     const SemanticBars& bars, const char* classification) {
  const auto& result = captured.result;
  std::printf(
      "%s\t%u\t%u\t%u\t%u\t%u\t%u\t%04x\t%04x\t%04x\t%04x\t%04x\t%04x\t%04x\t%04x\t%u\t%s\n",
      property, mode, recipe, address,
      static_cast<unsigned>(result.melodicRhythmId),
      static_cast<unsigned>(result.motifShapeId), result.phraseBars,
      bars.bars[0].plan.onsets, bars.bars[1].plan.onsets,
      bars.bars[2].plan.onsets, bars.bars[3].plan.onsets,
      bars.bars[0].plan.continuations, bars.bars[1].plan.continuations,
      bars.bars[2].plan.continuations, bars.bars[3].plan.continuations,
      bars.distinct, classification);
}

static bool pureMelodicFourBar(const Captured& captured) {
  return captured.result.synthBRole == SemanticSynthBRole::Melodic &&
         captured.result.phraseBars == 4;
}

static unsigned onsetCount(StepMask mask) {
  unsigned count = 0;
  for (uint8_t step = 0; step < kStepsPerBar; ++step) {
    if ((mask & stepBit(step)) != 0) ++count;
  }
  return count;
}

int main() {
  std::puts("property\tmode\trecipe\tpatternAddress\tmelodicRhythm\tmotifShape\tphraseBars\tbar0_onsets\tbar1_onsets\tbar2_onsets\tbar3_onsets\tbar0_continuations\tbar1_continuations\tbar2_continuations\tbar3_continuations\tdistinctBars\tclassification");

  const Captured p1 = capture(3, 0, 19);
  assert(pureMelodicFourBar(p1));
  assert(p1.result.melodicRhythmId == MelodicRhythmId::SparseCall);
  assert(p1.result.motifShapeId == MotifShapeId::Mirror);
  const SemanticBars p1Bars = replay(p1.probe);
  const bool p1Strong = p1Bars.bars[0].status == MelodicMotifStatus::ValidButEmpty ||
                        p1Bars.bars[1].status == MelodicMotifStatus::ValidButEmpty ||
                        p1Bars.bars[2].status == MelodicMotifStatus::ValidButEmpty ||
                        p1Bars.bars[3].status == MelodicMotifStatus::ValidButEmpty;
  printRow("P1_SPACE", 3, 0, 19, p1, p1Bars,
           p1Strong ? "STRONG" : "WEAK");
  assert(p1Strong);

  const Captured repeated = capture(0, 0, 0);
  assert(pureMelodicFourBar(repeated));
  assert(repeated.result.melodicRhythmId == MelodicRhythmId::RepeatedCell);
  assert(repeated.result.motifShapeId == MotifShapeId::Pivot);
  const SemanticBars repeatedBars = replay(repeated.probe);
  printRow("P2_REPEATEDCELL", 0, 0, 0, repeated, repeatedBars,
           repeatedBars.distinct >= 2 ? "PASS" : "FAIL");

  unsigned p2Candidates = 0;
  unsigned p2Matches = 0;
  bool p2Found = false;
  unsigned p2Mode = 0, p2Recipe = 0, p2Address = 0;
  Captured p2Canonical{};
  SemanticBars p2CanonicalBars{};
  for (const auto& pair : kPairs) {
    for (unsigned address = 0; address < 256; ++address) {
      const Captured candidate = capture(pair.mode, pair.recipe, address);
      if (!pureMelodicFourBar(candidate)) continue;
      ++p2Candidates;
      const SemanticBars bars = replay(candidate.probe);
      if (bars.distinct < 2) continue;
      ++p2Matches;
      if (!p2Found) {
        p2Found = true;
        p2Mode = pair.mode;
        p2Recipe = pair.recipe;
        p2Address = address;
        p2Canonical = candidate;
        p2CanonicalBars = bars;
      }
    }
  }
  assert(p2Found);
  printRow("P2_CANONICAL", p2Mode, p2Recipe, p2Address, p2Canonical,
           p2CanonicalBars, "PASS");

  const Captured p3 = capture(3, 0, 7);
  assert(pureMelodicFourBar(p3));
  assert(p3.result.melodicRhythmId == MelodicRhythmId::DelayedAnswer);
  assert(p3.result.motifShapeId == MotifShapeId::CallResponse);
  assert(p3.probe.request.requestedShape == MotifShapeId::CallResponse);
  const SemanticBars p3Bars = replay(p3.probe);
  assert(p3Bars.bars[0].plan.motif.shape == MotifShapeId::CallResponse);
  assert(onsetCount(p3Bars.bars[0].plan.onsets) >= 2);
  printRow("P3_CALL_RESPONSE", 3, 0, 7, p3, p3Bars,
           "MULTI_ONSET_PASS");
  std::printf("# P2_CANDIDATES_CHECKED=%u P2_MATCHES=%u\n", p2Candidates,
              p2Matches);
  return 0;
}
