// DRUMS STEP SETTINGS (Alt+E): field limits, FLAM/ROLL cycle, area anchor.
#include <cstdio>
#include <cstdlib>

#include "src/state/drum_step_edit.h"

namespace {
int g_failures = 0;

void expect(bool condition, const char* message) {
  if (!condition) {
    std::fprintf(stderr, "FAIL: %s\n", message);
    ++g_failures;
  }
}

namespace Edit = GroovePuterUndo::DrumStepEdit;

void pressTimes(DrumStep& step, int field, int direction, int times) {
  for (int i = 0; i < times; ++i) {
    const DrumStep anchor = step;
    Edit::applyField(step, anchor, field, direction);
  }
}

void testProbability() {
  DrumStep step{};
  expect(step.probability == 100, "default probability is 100");
  pressTimes(step, Edit::kFieldProb, 1, 3);
  expect(step.probability == 100, "probability stops at 100");
  pressTimes(step, Edit::kFieldProb, -1, 1);
  expect(step.probability == 95, "probability moves by 5");
  pressTimes(step, Edit::kFieldProb, -1, 40);
  expect(step.probability == 0, "probability stops at 0");
}

void testTiming() {
  DrumStep step{};
  pressTimes(step, Edit::kFieldTiming, 1, 1);
  expect(step.timing == 1, "timing moves by 1 tick");
  pressTimes(step, Edit::kFieldTiming, 1, 40);
  expect(step.timing == 23, "timing stops at +23");
  pressTimes(step, Edit::kFieldTiming, -1, 80);
  expect(step.timing == -23, "timing stops at -23");
}

void testVelocity() {
  DrumStep step{};
  expect(step.velocity == 100, "default velocity is 100");
  pressTimes(step, Edit::kFieldVelocity, 1, 10);
  expect(step.velocity == 127, "velocity stops at 127");
  pressTimes(step, Edit::kFieldVelocity, -1, 40);
  expect(step.velocity == 1, "velocity never reaches 0 (silent hit)");
  pressTimes(step, Edit::kFieldVelocity, 1, 1);
  expect(step.velocity == 6, "velocity moves by 5");
}

void testOrnamentCycle() {
  DrumStep step{};
  expect(Edit::ornamentOf(step) == Edit::Ornament::None, "default is OFF");
  pressTimes(step, Edit::kFieldOrnament, 1, 1);
  expect(step.fx == DRUM_FX_FLAM && step.fxParam == Edit::kFlamGapTicks,
         "right: OFF -> FLAM with a gap the engine plays");
  expect(step.fxParam >= 1 && step.fxParam <= 23, "FLAM gap inside 1..23");
  pressTimes(step, Edit::kFieldOrnament, 1, 1);
  expect(step.fx == DRUM_FX_ROLL && step.fxParam == Edit::kRollHits,
         "right: FLAM -> ROLL");
  expect(step.fxParam >= 2 && step.fxParam <= 12,
         "ROLL hit count inside 2..12 (below 2 the engine plays no roll)");
  pressTimes(step, Edit::kFieldOrnament, 1, 1);
  expect(step.fx == DRUM_FX_NONE && step.fxParam == 0, "right: ROLL -> OFF");
  pressTimes(step, Edit::kFieldOrnament, -1, 1);
  expect(step.fx == DRUM_FX_ROLL, "left: OFF -> ROLL");
  pressTimes(step, Edit::kFieldOrnament, -1, 1);
  expect(step.fx == DRUM_FX_FLAM, "left: ROLL -> FLAM");

  DrumStep retrig{};
  retrig.fx = DRUM_FX_RETRIG;
  retrig.fxParam = 3;
  expect(Edit::ornamentOf(retrig) == Edit::Ornament::Other, "RETRIG is another FX");
  pressTimes(retrig, Edit::kFieldOrnament, 1, 1);
  expect(retrig.fx == DRUM_FX_FLAM && retrig.fxParam == Edit::kFlamGapTicks,
         "another FX counts as OFF: right gives FLAM and a fresh param");
}

void testFieldsAreIndependent() {
  DrumStep step{};
  step.hit = 1;
  step.accent = 1;
  pressTimes(step, Edit::kFieldOrnament, 1, 1);
  pressTimes(step, Edit::kFieldTiming, -1, 2);
  expect(step.hit == 1 && step.accent == 1, "hit and accent are untouched");
  expect(step.probability == 100 && step.velocity == 100,
         "other fields are untouched");
  expect(step.fx == DRUM_FX_FLAM && step.timing == -2, "edited fields hold");
}

void testAreaAnchor() {
  DrumStep anchor{};
  anchor.velocity = 60;
  DrumStep other{};
  other.velocity = 120;
  Edit::applyField(other, anchor, Edit::kFieldVelocity, 1);
  expect(other.velocity == 65, "an area takes the first cell's value + step");
  Edit::applyField(other, anchor, Edit::kFieldCount, 1);
  expect(other.velocity == 65, "an unknown field changes nothing");
}
}  // namespace

int main() {
  testProbability();
  testTiming();
  testVelocity();
  testOrnamentCycle();
  testFieldsAreIndependent();
  testAreaAnchor();
  if (g_failures != 0) {
    std::fprintf(stderr, "drum step settings: %d failure(s)\n", g_failures);
    return EXIT_FAILURE;
  }
  std::printf("drum step settings: OK\n");
  return EXIT_SUCCESS;
}
