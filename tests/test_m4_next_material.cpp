// M4: asking for the next material, and getting it on a musical boundary.
//
// The gesture stays fast -- press Q, the request is taken immediately -- while
// the work it implies does not happen where it would be heard. Preparation is
// control-side and may read a file; activation is a value copy at a bar line,
// with no allocation and no I/O.
//
// The census measured 12-18 ms to load a melody against a 2000 ms bar at
// 120 BPM, so there is room to prepare properly rather than racing. What that
// margin buys is the rule below: nothing half-prepared ever becomes audible.

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
  // 1. The buffers are allocated once, on the heap, because 2 x 1284 does not
  //    fit the static budget. If that allocation fails, NEXT is unavailable --
  //    and says so, rather than half working.
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
  }

  // 2. Staging prepares; it does not activate. Between the press and the bar
  //    line the voice must still be playing exactly what it was.
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

  // 3. Activation is what makes it audible, and it moves the whole thing:
  //    slot, kind and material together.
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
  //    the second wait an extra bar would be a limitation invented by an
  //    allocator, not by music.
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

  // 5. A Pattern request needs no melody, and must not carry one.
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

  // 6. A Melody request with nothing prepared is refused outright. This is the
  //    failed-load case, and the rule is that ACTIVE does not move and the
  //    request is dropped -- never a partial activation.
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

  // 7. Activating with nothing queued is a no-op, because the boundary comes
  //    round on every bar whether or not anyone pressed anything.
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

  if (g_failures == 0) {
    std::printf("M4 next material: PASS\n");
    return 0;
  }
  std::fprintf(stderr, "M4 next material: %d failure(s)\n", g_failures);
  return 1;
}
