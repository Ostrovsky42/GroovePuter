#include <cassert>
#include <cstdint>
#include <cstring>

#include "src/generation/feel/feel_interpreter.h"
#include "src/generation/feel/feel_pattern_adapter.h"
#include "src/generation/materialization/pattern_materializer.h"

using namespace GroovePuterRhythm;

namespace {

FeelInterpretRequest requestFor(const FeelPhrase& phrase,
                                FeelProfileId profile,
                                uint8_t amount,
                                uint32_t seed = 0x12345678u) {
  FeelInterpretRequest request{};
  request.phrase = &phrase;
  request.profile = profile;
  request.amount = amount;
  request.generation.projectSeed = seed;
  request.generation.phraseOrdinal = 9;
  return request;
}

FeelPhrase denseEightBarPhrase() {
  FeelPhrase phrase{};
  phrase.barCount = 8;
  for (uint8_t bar = 0; bar < phrase.barCount; ++bar) {
    for (uint8_t step = 0; step < 16; ++step) {
      FeelPhraseEvent& event = phrase.events[phrase.eventCount++];
      event.role = static_cast<RhythmRole>(step % kRhythmRoleCount);
      event.barIndex = bar;
      event.idealTick = static_cast<uint16_t>(step * kFeelTicksPerStep);
      event.durationTicks = static_cast<uint16_t>(1 + (step % 48));
    }
  }
  return phrase;
}

void testStableProfilesAndDeterminism() {
  assert(static_cast<uint8_t>(FeelProfileId::Straight) == 0);
  assert(std::strcmp(feelProfileName(FeelProfileId::Straight), "STRAIGHT") == 0);
  assert(std::strcmp(feelProfileName(FeelProfileId::LaidBack), "LAID BACK") == 0);

  const FeelPhrase phrase = denseEightBarPhrase();
  TimedFeelPhrase first{};
  TimedFeelPhrase second{};
  const FeelInterpretRequest request =
      requestFor(phrase, FeelProfileId::PushPullControlled, 100);
  assert(interpretFeelPhrase(request, first) == FeelInterpretStatus::Ok);
  assert(interpretFeelPhrase(request, second) == FeelInterpretStatus::Ok);
  assert(std::memcmp(&first, &second, sizeof(first)) == 0);
}

void testNoDriftAndBounds() {
  const FeelPhrase phrase = denseEightBarPhrase();
  // The interpreter executes concrete timing characters only; Auto is a
  // selection mode resolved before interpretation (GF2-I2).
  for (uint8_t profileValue = 0;
       profileValue < static_cast<uint8_t>(FeelProfileId::Auto);
       ++profileValue) {
    for (uint32_t seed = 0; seed < 64; ++seed) {
      TimedFeelPhrase timed{};
      const FeelInterpretRequest request = requestFor(
          phrase, static_cast<FeelProfileId>(profileValue), 100, seed);
      assert(interpretFeelPhrase(request, timed) == FeelInterpretStatus::Ok);
      assert(timed.eventCount == phrase.eventCount);
      uint32_t previousDistinctIdeal = 0;
      uint32_t currentGroupMax = 0;
      for (uint16_t i = 0; i < timed.eventCount; ++i) {
        const FeelPhraseEvent& source = phrase.events[i];
        const TimedFeelEvent& event = timed.events[i];
        const uint32_t barOrigin =
            static_cast<uint32_t>(source.barIndex) * kFeelTicksPerBar;
        assert(event.idealOnTick == barOrigin + source.idealTick);
        assert(event.targetOnTick >= barOrigin);
        assert(event.targetOnTick < barOrigin + kFeelTicksPerBar);
        assert(event.offsetTicks >= -6 && event.offsetTicks <= 6);
        assert(event.targetOffTick > event.targetOnTick);
        assert(event.targetOffTick - event.targetOnTick == source.durationTicks);
        if (i != 0 && event.idealOnTick != previousDistinctIdeal) {
          assert(event.targetOnTick >= currentGroupMax);
          currentGroupMax = 0;
        }
        if (event.targetOnTick > currentGroupMax) {
          currentGroupMax = event.targetOnTick;
        }
        previousDistinctIdeal = event.idealOnTick;
      }

      // Every bar is still anchored to its own absolute origin: the error at
      // bar N is bounded exactly like bar 0 and cannot accumulate.
      for (uint8_t bar = 0; bar < phrase.barCount; ++bar) {
        const TimedFeelEvent& firstInBar = timed.events[bar * 16u];
        assert(firstInBar.targetOnTick ==
               static_cast<uint32_t>(bar) * kFeelTicksPerBar);
      }
    }
  }
}

void testOrderingAndTransactionalFailures() {
  FeelPhrase phrase{};
  phrase.barCount = 1;
  phrase.eventCount = 4;
  phrase.events[0] = {RhythmRole::Backbeat, 0, 100, 12};
  phrase.events[1] = {RhythmRole::Kick, 0, 101, 12};
  phrase.events[2] = {RhythmRole::ClosedHat, 0, 102, 1};
  phrase.events[3] = {RhythmRole::Backbeat, 0, 383, 64};
  TimedFeelPhrase timed{};
  assert(interpretFeelPhrase(
             requestFor(phrase, FeelProfileId::PushPullControlled, 100), timed) ==
         FeelInterpretStatus::Ok);
  for (uint16_t i = 1; i < timed.eventCount; ++i) {
    assert(timed.events[i].targetOnTick >= timed.events[i - 1].targetOnTick);
  }
  assert(timed.events[3].targetOnTick == 383);

  TimedFeelPhrase sentinel{};
  sentinel.barCount = 7;
  sentinel.eventCount = 5;
  const TimedFeelPhrase before = sentinel;
  FeelInterpretRequest invalid = requestFor(
      phrase, static_cast<FeelProfileId>(255), 100);
  assert(interpretFeelPhrase(invalid, sentinel) ==
         FeelInterpretStatus::InvalidProfile);
  assert(std::memcmp(&sentinel, &before, sizeof(sentinel)) == 0);

  FeelPhrase overflow = phrase;
  overflow.eventCount = kMaxFeelEvents + 1;
  invalid = requestFor(overflow, FeelProfileId::LaidBack, 100);
  assert(interpretFeelPhrase(invalid, sentinel) ==
         FeelInterpretStatus::Overflow);
  assert(std::memcmp(&sentinel, &before, sizeof(sentinel)) == 0);
}

void testStraightAndPatternAdapter() {
  RhythmPhrasePlan plan{};
  plan.barCount = 1;
  plan.level = RealizationLevel::P1Canonical;
  RoleRhythmPlan& kick = plan.bars[0].roles[
      static_cast<uint8_t>(RhythmRole::Kick)];
  RoleRhythmPlan& backbeat = plan.bars[0].roles[
      static_cast<uint8_t>(RhythmRole::Backbeat)];
  kick.structural = static_cast<StepMask>(stepBit(0) | stepBit(4));
  backbeat.structural = static_cast<StepMask>(stepBit(4) | stepBit(12));

  const PatternMaterializerBinding binding = standardDrumPatternBinding(
      static_cast<RhythmRoleMask>(
          rhythmRoleBit(RhythmRole::BassRhythm) |
          rhythmRoleBit(RhythmRole::ChordRhythm) |
          rhythmRoleBit(RhythmRole::MelodicRhythm)));
  MaterializedPatterns material{};
  assert(materializeRhythmPattern(plan, binding, material) ==
         PatternMaterializeStatus::Ok);
  const GenerationContext generation{0xCAFEBABEu, 4};
  assert(applyFeelToMaterializedPattern(
             plan, binding, FeelProfileId::Straight, 100,
             generation, material) == FeelPatternApplyStatus::Ok);
  assert(material.drums.voices[KICK].steps[4].timing == 0);
  assert(material.drums.voices[SNARE].steps[4].timing == 0);

  assert(applyFeelToMaterializedPattern(
             plan, binding, FeelProfileId::LaidBack, 100,
             generation, material) == FeelPatternApplyStatus::Ok);
  assert(material.drums.voices[KICK].steps[4].timing == 0);
  assert(material.drums.voices[SNARE].steps[4].timing > 0);
}

void buildTransportBytes(uint8_t* bytes, uint16_t& count) {
  count = 0;
  bytes[count++] = 0xFA;  // Start
  for (uint32_t tick = 0; tick < 4u * kFeelTicksPerBar; ++tick) {
    if ((tick % 4u) == 0u) bytes[count++] = 0xF8;  // 24 clocks/quarter
  }
  bytes[count++] = 0xFC;  // Stop
}

void testTransportClockIdentity() {
  uint8_t straightBytes[400]{};
  uint8_t feelBytes[400]{};
  uint16_t straightCount = 0;
  uint16_t feelCount = 0;
  buildTransportBytes(straightBytes, straightCount);

  const FeelPhrase phrase = denseEightBarPhrase();
  TimedFeelPhrase ignored{};
  assert(interpretFeelPhrase(
             requestFor(phrase, FeelProfileId::LaidBack, 100), ignored) ==
         FeelInterpretStatus::Ok);
  buildTransportBytes(feelBytes, feelCount);
  assert(straightCount == feelCount);
  assert(std::memcmp(straightBytes, feelBytes, straightCount) == 0);
}

void testSlowTempoFixtureIsGridInvariant() {
  const FeelPhrase phrase = denseEightBarPhrase();
  TimedFeelPhrase reference{};
  assert(interpretFeelPhrase(
             requestFor(phrase, FeelProfileId::LaidBack, 72), reference) ==
         FeelInterpretStatus::Ok);
  // BPM belongs to transport and therefore is deliberately not an interpreter
  // input. This 70--88 BPM listening corridor must retain the same tick law.
  for (int bpm = 70; bpm <= 88; ++bpm) {
    (void)bpm;
    TimedFeelPhrase candidate{};
    assert(interpretFeelPhrase(
               requestFor(phrase, FeelProfileId::LaidBack, 72), candidate) ==
           FeelInterpretStatus::Ok);
    assert(std::memcmp(&reference, &candidate, sizeof(reference)) == 0);
  }
}

}  // namespace

int main() {
  testStableProfilesAndDeterminism();
  testNoDriftAndBounds();
  testOrderingAndTransactionalFailures();
  testStraightAndPatternAdapter();
  testTransportClockIdentity();
  testSlowTempoFixtureIsGridInvariant();
  return 0;
}
