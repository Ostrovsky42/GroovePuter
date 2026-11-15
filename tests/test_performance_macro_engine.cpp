#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <initializer_list>

#include "src/performance/performance_macro_engine.h"

namespace {
bool trackAllocations = false;
std::size_t trackedAllocations = 0;

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Event = PhraseRuntime::RuntimeSynthEvent;
namespace Macros = GroovePuterPerformance::Macros;

Event makeEvent(uint16_t tick, uint8_t note, uint8_t velocity,
                uint8_t probability, uint8_t flags, uint16_t duration) {
  Event event{};
  event.startTick = tick;
  event.durationSubticks = duration;
  event.note = note;
  event.velocity = velocity;
  event.probability = probability;
  event.flags = flags;
  return event;
}

Buffer samplePhrase() {
  Buffer phrase{};
  phrase.count = 6;
  phrase.lengthTicks = PhraseRuntime::kTicksPerBar;
  phrase.events[0] = makeEvent(0, 60, 80, 100, 0, 96);
  phrase.events[1] = makeEvent(24, 62, 70, 60, PhraseRuntime::kEventGhost, 72);
  phrase.events[2] = makeEvent(48, 64, 70, 50, PhraseRuntime::kEventGhost, 48);
  phrase.events[3] = makeEvent(96, 65, 80, 100, 0, 96);
  phrase.events[4] = makeEvent(120, 67, 75, 80, 0, 48);
  phrase.events[5] = makeEvent(192, 64, 90, 100, 0, 96);
  return phrase;
}

bool sameEvent(const Event& a, const Event& b) {
  return a.startTick == b.startTick &&
         a.durationSubticks == b.durationSubticks && a.note == b.note &&
         a.velocity == b.velocity && a.probability == b.probability &&
         a.flags == b.flags && a.fx == b.fx && a.fxParam == b.fxParam;
}

bool sameBuffer(const Buffer& a, const Buffer& b) {
  if (a.count != b.count || a.lengthTicks != b.lengthTicks) return false;
  for (uint16_t i = 0; i < a.count; ++i) {
    if (!sameEvent(a.events[i], b.events[i])) return false;
  }
  return true;
}

Macros::HarmonyContext cMajor() {
  Macros::HarmonyContext context{};
  context.known = true;
  context.root = 0;
  context.scale = GroovePuterRhythm::kScaleMajor;
  return context;
}

void testDefaultsAreExactNoOpAndDoNotMutateSource() {
  const Buffer source = samplePhrase();
  const Buffer before = source;
  Buffer output{};
  const Macros::State state{};
  assert(state.valid());
  assert(Macros::apply(source, state, cMajor(), 0x12345678u, output) ==
         Macros::ApplyResult::Ready);
  assert(sameBuffer(output, source));
  assert(sameBuffer(source, before));
}

void testStateRangeIsFixedAndInvalidWriteIsRejected() {
  Macros::State state{};
  assert(state.set(Macros::Control::Energy, 73));
  assert(!state.set(Macros::Control::Energy, 101));
  assert(state.energy == 73);
  assert(state.set(Macros::Control::Tension, 0));
  assert(state.set(Macros::Control::Space, 100));
  assert(state.set(Macros::Control::Mutation, 100));
  assert(state.valid());
}

void testStatePackRoundTripsAsOneAtomicSnapshotWord() {
  Macros::State state{};
  assert(state.set(Macros::Control::Tension, 100));
  assert(state.set(Macros::Control::Energy, 65));
  assert(state.set(Macros::Control::Space, 10));
  assert(state.set(Macros::Control::Mutation, 99));
  const uint32_t packed = Macros::packState(state);
  const Macros::State decoded = Macros::unpackState(packed);
  assert(decoded.valid());
  assert(decoded.tension == state.tension);
  assert(decoded.energy == state.energy);
  assert(decoded.space == state.space);
  assert(decoded.mutation == state.mutation);
}

void testTensionAccentsWeakOnsetsAndEmphasizesApproaches() {
  const Buffer source = samplePhrase();
  Macros::State state{};
  assert(state.set(Macros::Control::Tension, 100));
  Buffer output{};
  assert(Macros::apply(source, state, cMajor(), 7u, output) ==
         Macros::ApplyResult::Ready);
  assert(output.events[0].velocity == source.events[0].velocity);
  assert(output.events[1].velocity > source.events[1].velocity);
  assert(output.events[2].velocity > source.events[2].velocity);
  assert(output.events[3].velocity == source.events[3].velocity);
  assert(output.events[4].velocity >= source.events[4].velocity + 24);
  assert(output.events[1].velocity >= source.events[1].velocity + 36);
  assert(output.count == source.count);
}

void testEnergyRaisesVelocityAndOptionalEventProbabilityWithinBounds() {
  const Buffer source = samplePhrase();
  Macros::State state{};
  assert(state.set(Macros::Control::Energy, 100));
  Buffer output{};
  assert(Macros::apply(source, state, cMajor(), 11u, output) ==
         Macros::ApplyResult::Ready);
  // 80% of the room below 127 at 100%: 80 + (127 - 80) * 0.8 = 118.
  assert(output.events[0].velocity == 118);
  // A loud note still rises instead of clipping at 127 (no x1.5 cliff).
  assert(Macros::detail::energyVelocity(100, 100) > 100);
  assert(Macros::detail::energyVelocity(127, 100) == 127);
  assert(Macros::detail::energyVelocity(90, 100) < 127);
  assert(output.events[1].velocity >= source.events[1].velocity);
  assert(output.events[1].probability > source.events[1].probability);
  assert(output.events[1].probability <= 100);
  assert(output.events[0].startTick == source.events[0].startTick);
  assert(output.count == source.count);
  assert(RuntimePhraseEdit::validate(output));
}

void testSpaceCreatesOffbeatRestsAndShortensGatesBoundedly() {
  const Buffer source = samplePhrase();
  Macros::State state{};
  assert(state.set(Macros::Control::Space, 100));
  Buffer output{};
  assert(Macros::apply(source, state, cMajor(), 17u, output) ==
         Macros::ApplyResult::Ready);
  assert(output.events[1].probability == 0);
  assert(output.events[2].probability == 0);
  assert(output.events[0].probability == source.events[0].probability);
  // SPACE 100%: gates keep 10% of their length.
  assert(output.events[0].durationSubticks == source.events[0].durationSubticks * 10 / 100);
  assert(output.events[0].durationSubticks > 0);
  assert(output.events[4].probability <= source.events[4].probability * 40 / 100);
  assert(output.count == source.count);
  assert(RuntimePhraseEdit::validate(output));
}

void testMutationIsSeededBoundedAndProtectsBarAnchors() {
  const Buffer source = samplePhrase();
  Macros::State state{};
  assert(state.set(Macros::Control::Mutation, 100));
  Buffer first{};
  Buffer repeated{};
  assert(Macros::apply(source, state, cMajor(), 0xA55Au, first) ==
         Macros::ApplyResult::Ready);
  assert(Macros::apply(source, state, cMajor(), 0xA55Au, repeated) ==
         Macros::ApplyResult::Ready);
  assert(sameBuffer(first, repeated));
  assert(first.events[4].note == source.events[4].note);
  assert(first.events[0].note == source.events[0].note);
  assert(first.events[3].note == source.events[3].note);
  assert(first.events[5].note == source.events[5].note);
  assert(first.events[1].note != source.events[1].note ||
         first.events[2].note != source.events[2].note ||
         first.events[4].note != source.events[4].note);
  for (uint16_t i = 0; i < source.count; ++i) {
    assert(first.events[i].startTick == source.events[i].startTick);
    assert(first.events[i].durationSubticks == source.events[i].durationSubticks);
    assert(first.events[i].note <= 127);
    assert(ProjectKey::inScale(first.events[i].note, 0,
                               GroovePuterRhythm::kScaleMajor));
  }
  assert(first.count == source.count);
  assert(sameBuffer(source, samplePhrase()));
  assert(RuntimePhraseEdit::validate(first));
}

void testUnknownHarmonyUsesConservativeMutationFallback() {
  const Buffer source = samplePhrase();
  Macros::State state{};
  assert(state.set(Macros::Control::Mutation, 100));
  Buffer output{};
  assert(Macros::apply(source, state, Macros::HarmonyContext{}, 0xBEEFu,
                       output) == Macros::ApplyResult::Ready);
  for (uint16_t i = 0; i < source.count; ++i) {
    assert(output.events[i].note == source.events[i].note);
  }
}

void testRejectedInputDoesNotPartiallyReplaceDestination() {
  const Buffer validSource = samplePhrase();
  Buffer invalidSource = validSource;
  invalidSource.events[2].durationSubticks = 0;
  Buffer destination = validSource;
  destination.events[0].note = 48;
  const Buffer before = destination;
  assert(Macros::apply(invalidSource, Macros::State{}, cMajor(), 1u,
                       destination) == Macros::ApplyResult::InvalidSource);
  assert(sameBuffer(destination, before));

  const Macros::HarmonyContext invalidHarmony{
      true, 12, GroovePuterRhythm::kScaleMajor};
  assert(Macros::apply(validSource, Macros::State{}, invalidHarmony, 1u,
                       destination) == Macros::ApplyResult::InvalidHarmony);
  assert(sameBuffer(destination, before));

  Buffer aliased = validSource;
  const Buffer aliasedBefore = aliased;
  assert(Macros::apply(aliased, Macros::State{}, cMajor(), 1u, aliased) ==
         Macros::ApplyResult::AliasedBuffers);
  assert(sameBuffer(aliased, aliasedBefore));
}

void testApplyUsesNoHeapAllocations() {
  const Buffer source = samplePhrase();
  Macros::State state{};
  assert(state.set(Macros::Control::Energy, 50));
  assert(state.set(Macros::Control::Mutation, 50));
  Buffer output{};
  trackedAllocations = 0;
  trackAllocations = true;
  const Macros::ApplyResult result =
      Macros::apply(source, state, cMajor(), 0xCAFEBABEu, output);
  trackAllocations = false;
  assert(result == Macros::ApplyResult::Ready);
  assert(trackedAllocations == 0);
}

void testEventTransformMatchesBatchViewAtEachOnset() {
  const Buffer source = samplePhrase();
  Macros::State state{};
  assert(state.set(Macros::Control::Tension, 83));
  assert(state.set(Macros::Control::Energy, 61));
  assert(state.set(Macros::Control::Space, 37));
  assert(state.set(Macros::Control::Mutation, 72));
  const auto harmony = cMajor();
  constexpr uint32_t seed = 0x31415926u;
  Buffer batch{};
  assert(Macros::apply(source, state, harmony, seed, batch) ==
         Macros::ApplyResult::Ready);

  for (uint16_t i = 0; i < source.count; ++i) {
    const Event* next = nullptr;
    uint16_t nextIndex = 0;
    for (uint16_t j = 0; j < source.count; ++j) {
      if (source.events[j].startTick <= source.events[i].startTick) continue;
      if (next == nullptr || source.events[j].startTick < next->startTick) {
        next = &source.events[j];
        nextIndex = j;
      }
    }
    Event transformed{};
    assert(Macros::transformEvent(source.events[i], next, i, nextIndex,
                                  source.lengthTicks, state, harmony, seed,
                                  transformed) ==
           Macros::ApplyResult::Ready);
    assert(sameEvent(transformed, batch.events[i]));
  }
}

// Sweep the complete control range with no ghost labels: every pitch is motif.
// Compare realtime projection to batch, check monotonicity and source ownership.
void testFullRangeProtectsMotifAndKeepsRealtimeProjectionBounded() {
  Buffer source = samplePhrase();
  for (uint16_t i = 0; i < source.count; ++i) source.events[i].flags = 0;
  const Buffer before = source;
  Buffer previous = source;
  for (uint16_t value = 0; value <= 100; ++value) {
    const Macros::State state{static_cast<int8_t>(value), 0,
        static_cast<int8_t>(value), 100};
    Buffer batch{};
    assert(Macros::apply(source, state, cMajor(), 9, batch) == Macros::ApplyResult::Ready);
    for (uint16_t i = 0; i < source.count; ++i) {
      Event output{};
      const Event* next = i + 1 < source.count ? &source.events[i + 1] : nullptr;
      trackedAllocations = 0;
      trackAllocations = true;
      const auto result = Macros::transformEvent(source.events[i], next, i, i + 1,
          source.lengthTicks, state, cMajor(), 9, output);
      trackAllocations = false;
      assert(result == Macros::ApplyResult::Ready && trackedAllocations == 0);
      assert(sameEvent(output, batch.events[i]));
      assert(output.note == source.events[i].note);
      assert(output.startTick == source.events[i].startTick);
      assert(output.flags == source.events[i].flags);
      assert(output.fx == source.events[i].fx && output.fxParam == source.events[i].fxParam);
      assert(output.durationSubticks > 0);
      assert(output.durationSubticks <= previous.events[i].durationSubticks);
      assert(output.probability <= previous.events[i].probability);
      assert(output.velocity >= previous.events[i].velocity && output.velocity <= 127);
      // The bar start (tick 0 of each bar) is never thinned; beats 2-4 may drop with SPACE.
      if (output.startTick % 384u == 0u) {
        assert(output.probability == source.events[i].probability);
      }
    }
    previous = batch;
    assert(sameBuffer(source, before));
  }
}

void testRealtimeExtremesStayMidiSafe() {
  const Macros::State state{100, 100, 100, 100};
  for (uint16_t note = 0; note <= 127; ++note) {
    for (uint16_t duration : {uint16_t{1}, uint16_t{65535}}) {
      const Event source = makeEvent(24, static_cast<uint8_t>(note), 127, 100,
                                     PhraseRuntime::kEventGhost, duration);
      Event output{};
      assert(Macros::transformEvent(source, nullptr, 0, 0,
          PhraseRuntime::kTicksPerBar, state, cMajor(), 7, output) ==
          Macros::ApplyResult::Ready);
      assert(output.note <= 127 && output.velocity == 127);
      assert(output.durationSubticks >= 1 && output.durationSubticks <= duration);
      assert(output.probability == 0);
      assert(output.startTick == source.startTick && output.flags == source.flags);
      if (!ProjectKey::inScale(source.note, 0, GroovePuterRhythm::kScaleMajor)) {
        assert(output.note == source.note);
      }
    }
  }
}

void testEventTransformRejectsInvalidDataWithoutChangingOutput() {
  Event source = makeEvent(24, 60, 80, 100, 0, 24);
  Event output = makeEvent(0, 48, 64, 100, 0, 96);
  const Event before = output;
  source.velocity = 0;
  assert(Macros::transformEvent(source, nullptr, 0, 0,
                                PhraseRuntime::kTicksPerBar,
                                Macros::State{}, cMajor(), 1, output) ==
         Macros::ApplyResult::InvalidSource);
  assert(sameEvent(output, before));
}

void testEventTransformKeepsValidAcrossBarPlaybackLifetimes() {
  Event source = makeEvent(360, 60, 90, 100, 0, 800);
  Event output{};
  Macros::State state{};
  assert(state.set(Macros::Control::Space, 100));
  assert(Macros::transformEvent(source, nullptr, 0, 0,
                                PhraseRuntime::kTicksPerBar, state,
                                cMajor(), 1, output) ==
         Macros::ApplyResult::Ready);
  assert(output.startTick == 360);
  assert(output.durationSubticks == 80);   // 800 at SPACE 100%: 10% of the gate
}

void testNegativeMacrosInvertEachEffect() {
  const Buffer source = samplePhrase();
  // Negative ENERGY: quieter, never below 1 (the realtime path uses the same helper).
  Macros::State energy{};
  assert(energy.set(Macros::Control::Energy, -100));
  Buffer quiet{};
  assert(Macros::apply(source, energy, cMajor(), 3, quiet) == Macros::ApplyResult::Ready);
  for (uint16_t i = 0; i < source.count; ++i) {
    assert(quiet.events[i].velocity <= source.events[i].velocity && quiet.events[i].velocity >= 1);
  }
  assert(Macros::detail::energyVelocity(100, -100) == 50);
  assert(Macros::detail::energyVelocity(1, -100) == 1);

  // Negative SPACE: gates grow (legato), capped at the bar.
  Macros::State space{};
  assert(space.set(Macros::Control::Space, -100));
  Event longGate = makeEvent(0, 60, 90, 100, 0, 800);
  Event out{};
  assert(Macros::transformEvent(longGate, nullptr, 0, 0, PhraseRuntime::kTicksPerBar,
                                space, cMajor(), 1, out) == Macros::ApplyResult::Ready);
  assert(out.durationSubticks == 1600);
  Event nearEnd = makeEvent(360, 60, 90, 100, 0, 200);   // 24 ticks to the bar line
  assert(Macros::transformEvent(nearEnd, nullptr, 0, 0, PhraseRuntime::kTicksPerBar,
                                space, cMajor(), 1, out) == Macros::ApplyResult::Ready);
  assert(out.durationSubticks == 24 * 16);   // 400 wanted, capped at the bar line

  // Negative TENSION: no accent on weak onsets; the velocity can only go down.
  Macros::State tension{};
  assert(tension.set(Macros::Control::Tension, -100));
  Event weak = makeEvent(96 + 24, 60, 100, 100, 0, 240);
  assert(Macros::transformEvent(weak, nullptr, 0, 0, PhraseRuntime::kTicksPerBar,
                                tension, cMajor(), 1, out) == Macros::ApplyResult::Ready);
  assert(out.velocity < 100 && out.velocity >= 88);

  // Range and packing keep the sign.
  Macros::State packed{};
  assert(packed.set(Macros::Control::Mutation, -37));
  assert(!packed.set(Macros::Control::Mutation, -101));
  const Macros::State back = Macros::unpackState(Macros::packState(packed));
  assert(back.mutation == -37 && back.valid());
}

}  // namespace

void* operator new(std::size_t size) {
  if (trackAllocations) ++trackedAllocations;
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
  if (trackAllocations) ++trackedAllocations;
  if (void* memory = std::malloc(size)) return memory;
  throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
  testNegativeMacrosInvertEachEffect();
  testRealtimeExtremesStayMidiSafe();
  testFullRangeProtectsMotifAndKeepsRealtimeProjectionBounded();
  testDefaultsAreExactNoOpAndDoNotMutateSource();
  testStateRangeIsFixedAndInvalidWriteIsRejected();
  testStatePackRoundTripsAsOneAtomicSnapshotWord();
  testTensionAccentsWeakOnsetsAndEmphasizesApproaches();
  testEnergyRaisesVelocityAndOptionalEventProbabilityWithinBounds();
  testSpaceCreatesOffbeatRestsAndShortensGatesBoundedly();
  testMutationIsSeededBoundedAndProtectsBarAnchors();
  testUnknownHarmonyUsesConservativeMutationFallback();
  testRejectedInputDoesNotPartiallyReplaceDestination();
  testApplyUsesNoHeapAllocations();
  testEventTransformMatchesBatchViewAtEachOnset();
  testEventTransformRejectsInvalidDataWithoutChangingOutput();
  testEventTransformKeepsValidAcrossBarPlaybackLifetimes();
}
