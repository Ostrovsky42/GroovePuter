// M4 / FS1C: ratify the engine's per-voice pending-material primitive.
//
// This test deliberately does NOT claim that 0.9.11 contains an end-to-end
// NEXT request -> prepare/load -> stage producer. It proves only the layer that
// actually exists in the release root: two independent pending buffers,
// fail-closed staging, and boundary activation into the matching voice.

#include <cassert>
#include <cstdint>
#include <cstdio>

#define private public
#include "src/dsp/miniacid_engine.h"
#undef private

#include "src/audio/pattern_paging.h"
#include "src/input/musical_event_queue.h"

SerialMock Serial;
SDMock SD;

namespace {

using GroovePuterMaterial::MaterialKind;

int g_failures = 0;

void expect(bool condition, const char* message) {
  if (condition) return;
  std::fprintf(stderr, "M4 FAIL: %s\n", message);
  ++g_failures;
}

float readEnginePhase(void* context) {
  return static_cast<MiniAcid*>(context)->transportPhaseSteps();
}

PhraseRuntime::RuntimeSynthEventBuffer melodyWithNote(uint8_t note) {
  PhraseRuntime::RuntimeSynthEventBuffer melody{};
  melody.lengthTicks = PhraseRuntime::kTicksPerBar;
  melody.count = 1;
  melody.events[0].startTick = 0;
  melody.events[0].durationSubticks = 12 * PhraseRuntime::kSubticksPerTick;
  melody.events[0].note = note;
  melody.events[0].velocity = 100;
  melody.events[0].probability = 100;
  return melody;
}

struct Fixture {
  MiniAcid engine{44100.0f, nullptr};
  MusicalEventQueue queue{};

  Fixture() {
    engine.setBpm(120.0f);
    assert(engine.rebuildPatternRuntimeEventBank());
    engine.setPatternEventQueue(&queue);
    queue.setPhaseReader(readEnginePhase, &engine);
    engine.playing = true;
    engine.tickPhaseAccum_ = 0;
  }
};

}  // namespace

int main() {
  // 1. The buffers are allocated once, on the heap. Both voices must own a
  //    different address and repeated use must not reallocate either slot.
  {
    Fixture fixture;
    expect(fixture.engine.pendingMaterialReady(),
           "pending material buffers were not available after init");
    const void* first = fixture.engine.pendingMaterialAddress(0);
    const void* second = fixture.engine.pendingMaterialAddress(1);
    expect(first != nullptr && second != nullptr,
           "a pending buffer was not allocated");
    expect(first != second, "both voices share one pending buffer");

    const auto churnMelody = melodyWithNote(60);
    for (int i = 0; i < 20; ++i) {
      (void)fixture.engine.stagePendingMaterial(0, 7, MaterialKind::Melody,
                                                &churnMelody);
      fixture.engine.activatePendingMaterial();
    }
    expect(fixture.engine.pendingMaterialAddress(0) == first,
           "the pending buffer moved during use, so it is being reallocated");
    expect(fixture.engine.pendingMaterialAddress(1) == second,
           "the idle voice pending buffer moved during other-voice churn");
  }

  // 2. Staging prepares; it does not activate. Until the boundary, active
  //    material must remain exactly what it was.
  {
    Fixture fixture;
    const auto melody = melodyWithNote(60);
    expect(fixture.engine.stagePendingMaterial(0, 7, MaterialKind::Melody,
                                               &melody),
           "staging a prepared melody was refused");
    expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Pattern,
           "staging changed what is playing before the boundary");
    expect(fixture.engine.hasPendingMaterial(0),
           "a staged request was not remembered");
  }

  // 3. Activation moves slot, kind and payload together into the same voice.
  {
    Fixture fixture;
    const auto melody = melodyWithNote(64);
    (void)fixture.engine.stagePendingMaterial(0, 7, MaterialKind::Melody,
                                              &melody);
    fixture.engine.activatePendingMaterial();
    expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Melody,
           "activation did not change the material kind");
    expect(fixture.engine.activeMaterial(0).slot == 7,
           "activation did not carry the slot");
    expect(fixture.engine.currentPhraseBuffer(0).events[0].note == 64,
           "activation did not carry the melody itself");
    expect(!fixture.engine.hasPendingMaterial(0),
           "the request survived its own activation");
  }

  // 4. Two voices queued before the same boundary both arrive on it. Making
  //    the second wait an extra bar would be an allocator limitation, not a
  //    musical rule.
  {
    Fixture fixture;
    const auto a = melodyWithNote(60);
    const auto b = melodyWithNote(67);
    (void)fixture.engine.stagePendingMaterial(0, 3, MaterialKind::Melody, &a);
    (void)fixture.engine.stagePendingMaterial(1, 4, MaterialKind::Melody, &b);
    fixture.engine.activatePendingMaterial();
    expect(fixture.engine.activeMaterial(0).slot == 3 &&
               fixture.engine.activeMaterial(1).slot == 4,
           "two voices queued together did not both arrive");
    expect(fixture.engine.currentPhraseBuffer(0).events[0].note == 60 &&
               fixture.engine.currentPhraseBuffer(1).events[0].note == 67,
           "the two voices received each other's melodies");
  }

  // 5. A Pattern request needs no melody, and must not destroy the retained
  //    Working melody buffer while changing the active kind.
  {
    Fixture fixture;
    const auto melody = melodyWithNote(60);
    (void)fixture.engine.stagePendingMaterial(0, 1, MaterialKind::Melody,
                                              &melody);
    fixture.engine.activatePendingMaterial();
    expect(fixture.engine.stagePendingMaterial(0, 2, MaterialKind::Pattern,
                                               nullptr),
           "a Pattern request without a melody was refused");
    fixture.engine.activatePendingMaterial();
    expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Pattern,
           "returning to Pattern did not take effect");
    expect(fixture.engine.currentPhraseBuffer(0).events[0].note == 60,
           "a Pattern request destroyed the melody material");
  }

  // 6. A Melody request with nothing prepared is refused outright. With no
  //    older request queued, ACTIVE remains unchanged and nothing becomes
  //    pending.
  {
    Fixture fixture;
    const auto melody = melodyWithNote(60);
    (void)fixture.engine.stagePendingMaterial(0, 5, MaterialKind::Melody,
                                              &melody);
    fixture.engine.activatePendingMaterial();

    expect(!fixture.engine.stagePendingMaterial(0, 9, MaterialKind::Melody,
                                                nullptr),
           "a Melody request with no prepared material was accepted");
    expect(!fixture.engine.hasPendingMaterial(0),
           "a refused request was still queued");
    fixture.engine.activatePendingMaterial();
    expect(fixture.engine.activeMaterial(0).slot == 5,
           "a refused request still moved what is playing");
  }

  // 7. Activating with nothing queued is a no-op.
  {
    Fixture fixture;
    const auto melody = melodyWithNote(71);
    (void)fixture.engine.stagePendingMaterial(1, 6, MaterialKind::Melody,
                                              &melody);
    fixture.engine.activatePendingMaterial();
    const auto before = fixture.engine.activeMaterial(1);
    for (int i = 0; i < 5; ++i) fixture.engine.activatePendingMaterial();
    expect(fixture.engine.activeMaterial(1).slot == before.slot &&
               fixture.engine.activeMaterial(1).kind == before.kind,
           "an empty boundary changed what is playing");
  }

  // 8. FS1C adversarial ownership: staging B cannot change queued A; replacing
  //    A cannot change B; a failed replacement of A preserves both the last
  //    valid A request and B. At the boundary each voice receives its own
  //    latest valid musical decision.
  {
    Fixture fixture;
    const auto a1 = melodyWithNote(60);
    const auto a2 = melodyWithNote(62);
    const auto b1 = melodyWithNote(67);
    auto invalidA = melodyWithNote(72);
    invalidA.count = PhraseRuntime::kMaxSynthEvents + 1;

    expect(fixture.engine.stagePendingMaterial(0, 3, MaterialKind::Melody, &a1),
           "initial A staging failed");
    expect(fixture.engine.stagePendingMaterial(1, 4, MaterialKind::Melody, &b1),
           "B staging failed after A was already queued");
    expect(fixture.engine.hasPendingMaterial(0) &&
               fixture.engine.hasPendingMaterial(1),
           "staging the second voice cancelled the first");
    expect(fixture.engine.pendingMaterial_[0].melody->events[0].note == 60,
           "B staging overwrote A payload");
    expect(fixture.engine.pendingMaterial_[1].melody->events[0].note == 67,
           "B pending payload is not its own value");

    expect(fixture.engine.stagePendingMaterial(0, 5, MaterialKind::Melody, &a2),
           "restaging A failed");
    expect(fixture.engine.pendingMaterial_[0].slot == 5 &&
               fixture.engine.pendingMaterial_[0].melody->events[0].note == 62,
           "restaging A did not replace only A");
    expect(fixture.engine.pendingMaterial_[1].slot == 4 &&
               fixture.engine.pendingMaterial_[1].melody->events[0].note == 67,
           "restaging A changed B");

    expect(!fixture.engine.stagePendingMaterial(0, 9, MaterialKind::Melody,
                                                nullptr),
           "null A restage was accepted");
    expect(!fixture.engine.stagePendingMaterial(0, 9, MaterialKind::Melody,
                                                &invalidA),
           "invalid A restage was accepted");
    expect(fixture.engine.hasPendingMaterial(0) &&
               fixture.engine.pendingMaterial_[0].slot == 5 &&
               fixture.engine.pendingMaterial_[0].melody->events[0].note == 62,
           "failed A restage destroyed the last valid A decision");
    expect(fixture.engine.hasPendingMaterial(1) &&
               fixture.engine.pendingMaterial_[1].slot == 4 &&
               fixture.engine.pendingMaterial_[1].melody->events[0].note == 67,
           "failed A restage changed B");

    fixture.engine.activatePendingMaterial();
    expect(fixture.engine.activeMaterial(0).slot == 5 &&
               fixture.engine.currentPhraseBuffer(0).events[0].note == 62,
           "A did not receive its latest valid pending material");
    expect(fixture.engine.activeMaterial(1).slot == 4 &&
               fixture.engine.currentPhraseBuffer(1).events[0].note == 67,
           "B did not survive A replacement/failure path");
    expect(!fixture.engine.hasPendingMaterial(0) &&
               !fixture.engine.hasPendingMaterial(1),
           "activation did not clear the two requests independently");
  }

  // 9. Allocation-failure fault injection: if one voice has no pending buffer,
  //    staging that voice fails closed and never borrows or aliases the other
  //    voice's storage. The healthy voice remains independently activatable.
  {
    Fixture fixture;
    auto* savedA = fixture.engine.pendingMaterial_[0].melody;
    auto* savedB = fixture.engine.pendingMaterial_[1].melody;
    const auto a = melodyWithNote(60);
    const auto b = melodyWithNote(67);

    fixture.engine.pendingMaterial_[0].melody = nullptr;
    expect(!fixture.engine.pendingMaterialReady(),
           "readiness stayed true with A allocation missing");
    expect(!fixture.engine.stagePendingMaterial(0, 3, MaterialKind::Melody, &a),
           "A staging succeeded without A storage");
    expect(fixture.engine.stagePendingMaterial(1, 4, MaterialKind::Melody, &b),
           "missing A storage blocked healthy B staging");
    expect(fixture.engine.pendingMaterialAddress(0) == nullptr &&
               fixture.engine.pendingMaterialAddress(1) == savedB,
           "missing A aliased or replaced B storage");
    fixture.engine.activatePendingMaterial();
    expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Pattern,
           "failed A staging changed A active material");
    expect(fixture.engine.activeMaterial(1).kind == MaterialKind::Melody &&
               fixture.engine.currentPhraseBuffer(1).events[0].note == 67,
           "healthy B did not activate while A storage was missing");
    fixture.engine.pendingMaterial_[0].melody = savedA;
  }

  {
    Fixture fixture;
    auto* savedA = fixture.engine.pendingMaterial_[0].melody;
    auto* savedB = fixture.engine.pendingMaterial_[1].melody;
    const auto a = melodyWithNote(60);
    const auto b = melodyWithNote(67);

    fixture.engine.pendingMaterial_[1].melody = nullptr;
    expect(!fixture.engine.pendingMaterialReady(),
           "readiness stayed true with B allocation missing");
    expect(fixture.engine.stagePendingMaterial(0, 3, MaterialKind::Melody, &a),
           "missing B storage blocked healthy A staging");
    expect(!fixture.engine.stagePendingMaterial(1, 4, MaterialKind::Melody, &b),
           "B staging succeeded without B storage");
    expect(fixture.engine.pendingMaterialAddress(0) == savedA &&
               fixture.engine.pendingMaterialAddress(1) == nullptr,
           "missing B aliased or replaced A storage");
    fixture.engine.activatePendingMaterial();
    expect(fixture.engine.activeMaterial(0).kind == MaterialKind::Melody &&
               fixture.engine.currentPhraseBuffer(0).events[0].note == 60,
           "healthy A did not activate while B storage was missing");
    expect(fixture.engine.activeMaterial(1).kind == MaterialKind::Pattern,
           "failed B staging changed B active material");
    fixture.engine.pendingMaterial_[1].melody = savedB;
  }

  if (g_failures == 0) {
    std::printf("M4 / FS1C pending ownership: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M4 / FS1C pending ownership: %d failure(s)\n",
               g_failures);
  return 1;
}
