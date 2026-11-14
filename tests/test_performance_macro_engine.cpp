#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <new>

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

void testTensionAccentsOnlyWeakStepwiseApproaches() {
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
  assert(output.events[4].velocity == source.events[4].velocity);
  assert(output.count == source.count);
}

void testEnergyRaisesVelocityAndOptionalEventProbabilityWithinBounds() {
  const Buffer source = samplePhrase();
  Macros::State state{};
  assert(state.set(Macros::Control::Energy, 100));
  Buffer output{};
  assert(Macros::apply(source, state, cMajor(), 11u, output) ==
         Macros::ApplyResult::Ready);
  assert(output.events[0].velocity == 120);
  assert(output.events[1].velocity >= source.events[1].velocity);
  assert(output.events[1].probability > source.events[1].probability);
  assert(output.events[1].probability <= 100);
  assert(output.events[0].startTick == source.events[0].startTick);
  assert(output.count == source.count);
  assert(RuntimePhraseEdit::validate(output));
}

void testSpaceReducesOnlyGhostChanceAndShortensGatesBoundedly() {
  const Buffer source = samplePhrase();
  Macros::State state{};
  assert(state.set(Macros::Control::Space, 100));
  Buffer output{};
  assert(Macros::apply(source, state, cMajor(), 17u, output) ==
         Macros::ApplyResult::Ready);
  assert(output.events[1].probability == 0);
  assert(output.events[2].probability == 0);
  assert(output.events[0].probability == source.events[0].probability);
  assert(output.events[0].durationSubticks == source.events[0].durationSubticks / 2);
  assert(output.events[0].durationSubticks > 0);
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
  testDefaultsAreExactNoOpAndDoNotMutateSource();
  testStateRangeIsFixedAndInvalidWriteIsRejected();
  testTensionAccentsOnlyWeakStepwiseApproaches();
  testEnergyRaisesVelocityAndOptionalEventProbabilityWithinBounds();
  testSpaceReducesOnlyGhostChanceAndShortensGatesBoundedly();
  testMutationIsSeededBoundedAndProtectsBarAnchors();
  testUnknownHarmonyUsesConservativeMutationFallback();
  testRejectedInputDoesNotPartiallyReplaceDestination();
  testApplyUsesNoHeapAllocations();
}
