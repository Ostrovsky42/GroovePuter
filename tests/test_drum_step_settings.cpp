// DRUMS STEP SETTINGS (Alt+E): field limits, FLAM/ROLL cycle, area anchor.
#include <cstdio>
#include <cstdlib>

#include "src/state/drum_step_edit.h"
#include "src/state/step_setting_marks.h"

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
  expect(step.velocity == 5, "velocity from 1 lands on the 5 grid");
  pressTimes(step, Edit::kFieldVelocity, 1, 19);
  expect(step.velocity == 100, "the default 100 is reachable from 1");
  pressTimes(step, Edit::kFieldVelocity, 1, 10);
  pressTimes(step, Edit::kFieldVelocity, -1, 6);
  expect(step.velocity == 100, "the default 100 is reachable from 127");
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

namespace Marks = GroovePuterUndo::StepSettingMarks;

void testMarks() {
  DrumStep d{};
  SynthStep y{};
  for (int f = 0; f < 4; ++f) {
    expect(!Marks::drumFieldCustom(d, f), "default drum step has no mark");
    expect(!Marks::synthFieldCustom(y, f), "default synth step has no mark");
  }
  pressTimes(d, Edit::kFieldProb, -1, 1);
  expect(Marks::drumFieldCustom(d, 0) && !Marks::drumFieldCustom(d, 1),
         "only PROB is marked");
  pressTimes(d, Edit::kFieldProb, 1, 1);
  expect(!Marks::drumFieldCustom(d, 0), "back to 100 clears the mark");
  pressTimes(d, Edit::kFieldTiming, 1, 1);
  expect(Marks::drumFieldCustom(d, 1), "TIMING +1 is marked");
  pressTimes(d, Edit::kFieldTiming, -1, 1);
  expect(!Marks::drumFieldCustom(d, 1), "TIMING 0 clears the mark");
  pressTimes(d, Edit::kFieldOrnament, 1, 1);
  expect(Marks::drumFieldCustom(d, 3), "FLAM is marked");
  pressTimes(d, Edit::kFieldOrnament, 1, 2);
  expect(!Marks::drumFieldCustom(d, 3), "FLAM -> ROLL -> OFF clears the mark");
  d.fx = DRUM_FX_RETRIG;
  expect(Marks::drumFieldCustom(d, 3), "another step FX is marked");

  y.velocity = 90;
  y.ghost = 1;
  y.timing = -3;
  y.probability = 50;
  for (int f = 0; f < 4; ++f) {
    expect(Marks::synthFieldCustom(y, f), "every changed synth field is marked");
  }
  y = SynthStep{};
  expect(!Marks::synthFieldCustom(y, 2), "velocity 100 is not marked");
  expect(!Marks::synthFieldCustom(y, 9), "an unknown field is never marked");
}

void testTimingMarkSide() {
  expect(Marks::timingMarkSide(0) == 0, "TIMING 0 has no grid mark");
  expect(Marks::timingMarkSide(1) == 1 && Marks::timingMarkSide(23) == 1, "late TIMING marks the right");
  expect(Marks::timingMarkSide(-1) == -1 && Marks::timingMarkSide(-23) == -1, "early TIMING marks the left");
  DrumStep d{};
  SynthStep y{};
  expect(Marks::timingMarkSide(d.timing) == 0 && Marks::timingMarkSide(y.timing) == 0,
         "a plain step has no grid mark");
}

void testStepOnGrid() {
  expect(Marks::stepOnGrid(97, 1, 5, 0, 100) == 100, "97 up -> 100");
  expect(Marks::stepOnGrid(97, -1, 5, 0, 100) == 95, "97 down -> 95");
  expect(Marks::stepOnGrid(100, 1, 5, 0, 100) == 100, "clamped at max");
  expect(Marks::stepOnGrid(0, -1, 5, 0, 100) == 0, "clamped at min");
  expect(Marks::stepOnGrid(127, -1, 5, 1, 127) == 125, "127 down -> 125");
  expect(Marks::stepOnGrid(127, 1, 5, 1, 127) == 127, "127 up stays");
  expect(Marks::stepOnGrid(1, -1, 5, 1, 127) == 1, "1 down stays");
  expect(Marks::stepOnGrid(1, 1, 5, 1, 127) == 5, "1 up -> 5");
}

void testMixedDiffers() {
  DrumStep a{}, b{};
  for (int f = 0; f < 4; ++f) expect(!Marks::drumFieldDiffers(a, b, f), "equal drum steps are not MIXED");
  b.probability = 50; b.timing = 3; b.velocity = 90; b.fx = DRUM_FX_FLAM;
  for (int f = 0; f < 4; ++f) expect(Marks::drumFieldDiffers(a, b, f), "differing drum field is MIXED");
  SynthStep x{}, y{};
  for (int f = 0; f < 4; ++f) expect(!Marks::synthFieldDiffers(x, y, f), "equal synth steps are not MIXED");
  y.probability = 50; y.timing = -2; y.velocity = 90; y.ghost = 1;
  for (int f = 0; f < 4; ++f) expect(Marks::synthFieldDiffers(x, y, f), "differing synth field is MIXED");
  y.probability = 100; y.timing = 0; y.velocity = 100;
  expect(!Marks::synthFieldDiffers(x, y, 0) && Marks::synthFieldDiffers(x, y, 3), "only CHANCE differs");
}
}  // namespace

int main() {
  testMixedDiffers();
  testMarks();
  testTimingMarkSide();
  testStepOnGrid();
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
