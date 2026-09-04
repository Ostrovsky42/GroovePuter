#include <cassert>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "src/phrase/runtime_synth_playback.h"

namespace {

using PhraseRuntime::RuntimeSynthEvent;
using PhraseRuntime::RuntimeSynthPlaybackActionType;
using PhraseRuntime::RuntimeSynthPlaybackActions;
using PhraseRuntime::RuntimeSynthPlaybackState;

RuntimeSynthEvent event(uint8_t note, uint16_t durationSubticks = 192) {
  RuntimeSynthEvent value{};
  value.startTick = 0;
  value.durationSubticks = durationSubticks;
  value.note = note;
  value.velocity = 103;
  value.probability = 100;
  value.flags = PhraseRuntime::kEventAccent;
  return value;
}

void requireAction(const RuntimeSynthPlaybackActions& actions,
                   uint8_t index,
                   RuntimeSynthPlaybackActionType type,
                   uint8_t note) {
  assert(index < actions.count);
  assert(actions.values[index].type == type);
  assert(actions.values[index].event.note == note);
}

PhraseRuntime::PatternProjectionSettings longGateSettings() {
  PhraseRuntime::PatternProjectionSettings settings{};
  settings.synthIndex = 0;
  settings.swingPercent = 50;
  settings.swingEnabled = false;
  settings.gateLengthRatio = 2.0f;
  return settings;
}

SynthPattern conditionalLifetimePattern(uint8_t probability,
                                        bool ghost,
                                        bool tieAfterConditional) {
  SynthPattern pattern{};
  for (int step = 0; step < SynthPattern::kSteps; ++step) {
    pattern.steps[step] = SynthStep{};
    pattern.steps[step].note = -1;
  }
  pattern.steps[0].note = 60;
  pattern.steps[0].probability = 100;
  pattern.steps[1].note = -2;
  pattern.steps[2].note = 64;
  pattern.steps[2].probability = probability;
  pattern.steps[2].ghost = ghost;
  if (tieAfterConditional) pattern.steps[3].note = -2;
  return pattern;
}

PhraseRuntime::RuntimeSynthEventBuffer project(
    const SynthPattern& pattern) {
  PhraseRuntime::RuntimeSynthEventBuffer projected{};
  assert(PhraseRuntime::projectPatternToRuntimeEvents(
             pattern, longGateSettings(), projected) ==
         PhraseRuntime::PatternProjectionStatus::Ready);
  return projected;
}

void testFirstOnsetStartsAndOwnsDeadline() {
  RuntimeSynthPlaybackState state{};
  const RuntimeSynthEvent note = event(60, 160);
  const auto actions = state.acceptOnset(note, 1000);

  assert(actions.count == 1);
  requireAction(actions, 0, RuntimeSynthPlaybackActionType::Start, 60);
  assert(state.active());
  assert(state.activeNote() == 60);
  assert(state.releaseAtSubtick() == 1160);
}

void testReplacingOnsetReleasesBeforeStart() {
  RuntimeSynthPlaybackState state{};
  (void)state.acceptOnset(event(60, 320), 1000);

  const auto actions = state.acceptOnset(event(64, 192), 1120);
  assert(actions.count == 2);
  requireAction(actions, 0, RuntimeSynthPlaybackActionType::Release, 60);
  requireAction(actions, 1, RuntimeSynthPlaybackActionType::Start, 64);
  assert(state.activeNote() == 64);
  assert(state.releaseAtSubtick() == 1312);
}

void testNaturalExpiryReleasesExactlyOnce() {
  RuntimeSynthPlaybackState state{};
  (void)state.acceptOnset(event(67, 64), 2000);

  auto before = state.releaseDue(2063);
  assert(before.count == 0);
  assert(state.active());

  auto exact = state.releaseDue(2064);
  assert(exact.count == 1);
  requireAction(exact, 0, RuntimeSynthPlaybackActionType::Release, 67);
  assert(!state.active());

  auto repeated = state.releaseDue(4096);
  assert(repeated.count == 0);
}

void testZeroDurationStillHasOneSubtickLifetime() {
  RuntimeSynthPlaybackState state{};
  (void)state.acceptOnset(event(69, 0), 500);
  assert(state.releaseAtSubtick() == 501);
  assert(state.releaseDue(500).count == 0);
  assert(state.releaseDue(501).count == 1);
}

void testRetriggerDoesNotExtendLifetime() {
  RuntimeSynthPlaybackState state{};
  const RuntimeSynthEvent base = event(55, 400);
  (void)state.acceptOnset(base, 700);
  const uint32_t deadline = state.releaseAtSubtick();

  RuntimeSynthEvent retrigger = base;
  retrigger.velocity = 91;
  const auto actions = state.acceptRetrigger(retrigger);
  assert(actions.count == 1);
  requireAction(actions, 0, RuntimeSynthPlaybackActionType::Retrigger, 55);
  assert(state.releaseAtSubtick() == deadline);

  const auto release = state.releaseDue(deadline);
  assert(release.count == 1);
}

void testRetriggerWithoutActiveLifetimeIsIgnored() {
  RuntimeSynthPlaybackState state{};
  const auto actions = state.acceptRetrigger(event(72));
  assert(actions.count == 0);
  assert(!state.active());
}

void testOrdinaryBarWrapIsNotABarrier() {
  RuntimeSynthPlaybackState state{};
  const uint32_t barEndSubtick =
      static_cast<uint32_t>(PhraseRuntime::kTicksPerBar) *
      PhraseRuntime::kSubticksPerTick;
  const uint32_t start = barEndSubtick - 8;
  (void)state.acceptOnset(event(48, 32), start);

  assert(state.releaseDue(barEndSubtick - 1).count == 0);
  assert(state.releaseDue(barEndSubtick).count == 0);
  assert(state.active());
  assert(state.releaseDue(start + 32).count == 1);
}

void testGuaranteedOnsetMayPreclipTiedLifetime() {
  const auto projected = project(
      conditionalLifetimePattern(100, false, false));
  assert(projected.count == 2);
  const RuntimeSynthEvent& first = projected.events[0];
  const RuntimeSynthEvent& guaranteed = projected.events[1];
  const uint32_t guaranteedStart =
      static_cast<uint32_t>(guaranteed.startTick) *
      PhraseRuntime::kSubticksPerTick;
  assert(guaranteed.probability == 100);
  assert((guaranteed.flags & PhraseRuntime::kEventGhost) == 0);
  assert(first.durationSubticks == guaranteedStart);
}

void testProbabilityZeroOnsetMustNotPreclipTiedLifetime() {
  const auto projected = project(
      conditionalLifetimePattern(0, false, false));
  assert(projected.count == 2);
  const RuntimeSynthEvent& first = projected.events[0];
  const RuntimeSynthEvent& conditional = projected.events[1];
  const uint32_t conditionalStart =
      static_cast<uint32_t>(conditional.startTick) *
      PhraseRuntime::kSubticksPerTick;
  assert(conditional.probability == 0);
  assert(first.durationSubticks > conditionalStart);
}

void testAcceptedConditionalOnsetReleasesThenStarts() {
  const auto projected = project(
      conditionalLifetimePattern(63, true, false));
  assert(projected.count == 2);
  const RuntimeSynthEvent& first = projected.events[0];
  const RuntimeSynthEvent& conditional = projected.events[1];
  const uint32_t conditionalStart =
      static_cast<uint32_t>(conditional.startTick) *
      PhraseRuntime::kSubticksPerTick;
  assert(conditional.probability < 100);
  assert((conditional.flags & PhraseRuntime::kEventGhost) != 0);
  assert(first.durationSubticks > conditionalStart);

  RuntimeSynthPlaybackState state{};
  (void)state.acceptOnset(first, 0);
  const auto replacement = state.acceptOnset(conditional, conditionalStart);
  assert(replacement.count == 2);
  requireAction(replacement, 0, RuntimeSynthPlaybackActionType::Release, 60);
  requireAction(replacement, 1, RuntimeSynthPlaybackActionType::Start, 64);
}

void testRejectedConditionalOnsetLetsLaterTieExtendOldLifetime() {
  const auto projected = project(
      conditionalLifetimePattern(100, true, true));
  assert(projected.count == 2);
  const RuntimeSynthEvent& first = projected.events[0];
  const RuntimeSynthEvent& conditional = projected.events[1];
  const uint32_t conditionalStart =
      static_cast<uint32_t>(conditional.startTick) *
      PhraseRuntime::kSubticksPerTick;
  const uint32_t laterTieStart = 72u * PhraseRuntime::kSubticksPerTick;
  assert((conditional.flags & PhraseRuntime::kEventGhost) != 0);
  assert(first.durationSubticks > conditionalStart);
  assert(first.durationSubticks > laterTieStart);

  RuntimeSynthPlaybackState state{};
  (void)state.acceptOnset(first, 0);
  // Runtime ghost/probability rejected the conditional token, so no
  // acceptOnset() call occurs here. The sounding note must remain alive.
  assert(state.releaseDue(conditionalStart).count == 0);
  assert(state.active());
  assert(state.releaseDue(laterTieStart).count == 0);
  assert(state.active());
}

void testHardBarrierReleasesExactlyOnce() {
  RuntimeSynthPlaybackState state{};
  (void)state.acceptOnset(event(76, 512), 3000);

  const auto barrier = state.hardBarrier();
  assert(barrier.count == 1);
  requireAction(barrier, 0, RuntimeSynthPlaybackActionType::Release, 76);
  assert(!state.active());
  assert(state.hardBarrier().count == 0);
  assert(state.releaseDue(999999).count == 0);
}

void testPlaybackStateIsFixedAndTriviallyCopyable() {
  static_assert(std::is_trivially_copyable<RuntimeSynthPlaybackState>::value,
                "P2 playback state must remain fixed/trivially copyable");
  static_assert(std::is_trivially_copyable<RuntimeSynthPlaybackActions>::value,
                "P2 action batch must remain fixed/trivially copyable");
  static_assert(sizeof(RuntimeSynthPlaybackActions) <= 32,
                "P2 action batch grew unexpectedly");
}

}  // namespace

int main() {
  testFirstOnsetStartsAndOwnsDeadline();
  testReplacingOnsetReleasesBeforeStart();
  testNaturalExpiryReleasesExactlyOnce();
  testZeroDurationStillHasOneSubtickLifetime();
  testRetriggerDoesNotExtendLifetime();
  testRetriggerWithoutActiveLifetimeIsIgnored();
  testOrdinaryBarWrapIsNotABarrier();
  testGuaranteedOnsetMayPreclipTiedLifetime();
  testProbabilityZeroOnsetMustNotPreclipTiedLifetime();
  testAcceptedConditionalOnsetReleasesThenStarts();
  testRejectedConditionalOnsetLetsLaterTieExtendOldLifetime();
  testHardBarrierReleasesExactlyOnce();
  testPlaybackStateIsFixedAndTriviallyCopyable();
  std::puts("PATTERN/PHRASE P2 runtime lifetime owner: PASS");
  return 0;
}
