#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

#include "src/phrase/runtime_synth_events.h"

namespace {

using namespace PhraseRuntime;

SynthPattern emptyPattern() {
  SynthPattern pattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
  }
  return pattern;
}

PatternProjectionSettings settingsFor(uint8_t synthIndex,
                                      float gateLengthRatio = 0.5f,
                                      uint8_t swingPercent = 50,
                                      bool swingEnabled = false) {
  PatternProjectionSettings settings{};
  settings.synthIndex = synthIndex;
  settings.gateLengthRatio = gateLengthRatio;
  settings.swingPercent = swingPercent;
  settings.swingEnabled = swingEnabled;
  return settings;
}

uint16_t expectedBaseDuration(uint8_t synthIndex, float gateLengthRatio) {
  float gate = gateLengthRatio;
  if (gate < 0.1f) gate = 0.5f;
  float effective = gate * (synthIndex == 0 ? 0.85f : 1.05f);
  if (synthIndex == 0 && effective < 0.15f) effective = 0.15f;
  if (synthIndex == 1 && effective > 0.98f) effective = 0.98f;
  long subticks = std::lround(24.0f * effective * kSubticksPerTick);
  if (subticks < 1) subticks = 1;
  return static_cast<uint16_t>(subticks);
}

void testAbiAndCapacity() {
  static_assert(std::is_trivially_copyable<RuntimeSynthEvent>::value,
                "runtime event must remain trivially copyable");
  static_assert(std::is_trivially_copyable<RuntimeSynthEventBuffer>::value,
                "runtime buffer must remain trivially copyable");
  static_assert(sizeof(RuntimeSynthEvent) == 10,
                "P1 runtime event ABI changed");
  static_assert(sizeof(RuntimeSynthEventBuffer) == 1284,
                "P1 runtime buffer ABI changed");
  static_assert(kTicksPerBar == 384, "bar clock changed");
  static_assert(kSubticksPerTick == 16, "duration precision changed");
  static_assert(kMaxPhraseBars == 8, "phrase length budget changed");
  static_assert(kMaxSynthEvents == 128, "event capacity changed");
  std::puts("P1-A PASS: fixed-capacity ABI and phrase budget frozen");
}

void testSimpleProjectionAndArticulation() {
  SynthPattern pattern = emptyPattern();
  SynthStep& step = pattern.steps[2];
  step.note = 60;
  step.velocity = 88;
  step.probability = 77;
  step.accent = true;
  step.slide = true;
  step.ghost = true;
  step.fx = 2;
  step.fxParam = 3;

  RuntimeSynthEventBuffer out{};
  const auto status = projectPatternToRuntimeEvents(
      pattern, settingsFor(0), out);
  assert(status == PatternProjectionStatus::Ready);
  assert(out.lengthTicks == 384);
  assert(out.count == 1);
  const RuntimeSynthEvent& event = out.events[0];
  assert(event.startTick == 48);
  assert(event.durationSubticks == expectedBaseDuration(0, 0.5f));
  assert(event.note == 60);
  assert(event.velocity == 88);
  assert(event.probability == 77);
  assert((event.flags & kEventAccent) != 0);
  assert((event.flags & kEventSlide) != 0);
  assert((event.flags & kEventGhost) != 0);
  assert(event.fx == 2);
  assert(event.fxParam == 3);
  std::puts("P1-B PASS: onset/articulation projection is explicit data");
}

void testSynthGateScaling() {
  SynthPattern pattern = emptyPattern();
  pattern.steps[0].note = 60;

  RuntimeSynthEventBuffer a{};
  RuntimeSynthEventBuffer b{};
  assert(projectPatternToRuntimeEvents(pattern, settingsFor(0), a) ==
         PatternProjectionStatus::Ready);
  assert(projectPatternToRuntimeEvents(pattern, settingsFor(1), b) ==
         PatternProjectionStatus::Ready);
  assert(a.count == 1 && b.count == 1);
  assert(a.events[0].durationSubticks == expectedBaseDuration(0, 0.5f));
  assert(b.events[0].durationSubticks == expectedBaseDuration(1, 0.5f));
  assert(a.events[0].durationSubticks != b.events[0].durationSubticks);
  std::puts("P1-C PASS: existing Synth A/B gate scaling is represented");
}

void testSwingAndMicrotimingWrap() {
  SynthPattern pattern = emptyPattern();
  pattern.steps[15].note = 60;
  pattern.steps[15].timing = 23;

  RuntimeSynthEventBuffer out{};
  assert(projectPatternToRuntimeEvents(
             pattern, settingsFor(0, 0.5f, 75, true), out) ==
         PatternProjectionStatus::Ready);
  assert(out.count == 1);
  assert(out.events[0].startTick == 11);
  std::puts("P1-D PASS: step15 swing+microtiming wraps to tick 11");
}

void testNegativeMicrotimingWrap() {
  SynthPattern pattern = emptyPattern();
  pattern.steps[0].note = 60;
  pattern.steps[0].timing = -23;

  RuntimeSynthEventBuffer out{};
  assert(projectPatternToRuntimeEvents(pattern, settingsFor(1), out) ==
         PatternProjectionStatus::Ready);
  assert(out.count == 1);
  assert(out.events[0].startTick == 361);
  std::puts("P1-E PASS: step0 negative microtiming wraps to tick 361");
}

void testLegacyTieBecomesCrossBoundaryDuration() {
  SynthPattern pattern = emptyPattern();
  pattern.steps[15].note = 60;
  pattern.steps[15].timing = 23;
  pattern.steps[0].note = -2;
  pattern.steps[0].timing = 1;

  RuntimeSynthEventBuffer out{};
  assert(projectPatternToRuntimeEvents(pattern, settingsFor(0), out) ==
         PatternProjectionStatus::Ready);
  assert(out.count == 1);
  const RuntimeSynthEvent& event = out.events[0];
  const uint16_t base = expectedBaseDuration(0, 0.5f);
  assert(event.startTick == 383);
  assert(event.durationSubticks == static_cast<uint16_t>(base * 2u));
  const uint32_t endSubtick =
      static_cast<uint32_t>(event.startTick) * kSubticksPerTick +
      event.durationSubticks;
  assert(endSubtick > static_cast<uint32_t>(kTicksPerBar) * kSubticksPerTick);
  std::puts("P1-F PASS: legacy TIE crossing is one explicit-duration event");
}

void testExpiredTieDoesNotRevive() {
  SynthPattern pattern = emptyPattern();
  pattern.steps[0].note = 60;
  pattern.steps[8].note = -2;

  RuntimeSynthEventBuffer out{};
  assert(projectPatternToRuntimeEvents(pattern, settingsFor(0), out) ==
         PatternProjectionStatus::Ready);
  assert(out.count == 1);
  assert(out.events[0].durationSubticks == expectedBaseDuration(0, 0.5f));
  std::puts("P1-G PASS: expired legacy TIE does not revive a note");
}

void testNextOnsetClipsMonophonicLifetime() {
  SynthPattern pattern = emptyPattern();
  pattern.steps[0].note = 60;
  pattern.steps[1].note = 62;

  RuntimeSynthEventBuffer out{};
  assert(projectPatternToRuntimeEvents(pattern, settingsFor(0, 2.0f), out) ==
         PatternProjectionStatus::Ready);
  assert(out.count == 2);
  assert(out.events[0].startTick == 0);
  assert(out.events[1].startTick == 24);
  assert(out.events[0].durationSubticks == 24u * kSubticksPerTick);
  std::puts("P1-H PASS: subsequent onset clips previous monophonic lifetime");
}

void testInvalidSynthIsFailureAtomic() {
  RuntimeSynthEventBuffer before{};
  before.count = 7;
  before.lengthTicks = 999;
  before.events[0].startTick = 123;
  before.events[0].durationSubticks = 456;
  before.events[0].note = 70;
  RuntimeSynthEventBuffer after = before;

  SynthPattern pattern = emptyPattern();
  pattern.steps[0].note = 60;
  assert(projectPatternToRuntimeEvents(pattern, settingsFor(2), after) ==
         PatternProjectionStatus::InvalidSynthIndex);
  assert(std::memcmp(&before, &after, sizeof(before)) == 0);
  std::puts("P1-I PASS: invalid projection leaves caller buffer untouched");
}

}  // namespace

int main() {
  testAbiAndCapacity();
  testSimpleProjectionAndArticulation();
  testSynthGateScaling();
  testSwingAndMicrotimingWrap();
  testNegativeMicrotimingWrap();
  testLegacyTieBecomesCrossBoundaryDuration();
  testExpiredTieDoesNotRevive();
  testNextOnsetClipsMonophonicLifetime();
  testInvalidSynthIsFailureAtomic();
  std::puts("PATTERN/PHRASE P1 runtime events: OK");
  return 0;
}
