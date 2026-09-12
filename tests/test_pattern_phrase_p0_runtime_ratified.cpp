#include <cassert>
#include <cstdint>
#include <cstdio>

// Reuse the complete historical P0 characterization fixture and every
// unaffected characterization. Only the Song-boundary cleanup expectation is
// re-attested below: Pattern playback now has one common runtime owner, so a
// physical Song source transition closes both the internal gate and the
// PatternPlayer MIDI gate through the same hard barrier.
#define main legacyPatternPhraseP0Main
#include "test_pattern_phrase_p0_runtime.cpp"
#undef main

namespace {

void testSongBoundaryCleanupConvergesForSynth(int synth) {
  RuntimeFixture f;
  editPattern(f, synth, 0, {15, 60});
  editPattern(f, synth, 1, {4, 64});
  editPattern(f, 1 - synth, 0, {});
  editPattern(f, 1 - synth, 1, {});

  f.engine.setSongLength(2);
  f.engine.setSongPattern(0, songTrackForSynth(synth),
                          songPatternFromBank(0, 0));
  f.engine.setSongPattern(1, songTrackForSynth(synth),
                          songPatternFromBank(0, 1));
  f.engine.songMode_ = true;
  f.engine.sceneManager_.setSongMode(true);
  f.engine.songPlaybackSlot_ = f.engine.activeSongSlot();
  f.engine.sceneManager_.currentScene().feel.patternBars = 1;
  f.engine.sceneManager_.setSongPosition(0);
  f.engine.applySongPositionSelection();
  assert(f.engine.current303PatternIndex(synth) == 0);

  f.engine.playing = true;
  f.beginRender();
  processTick(f, 360);
  f.endRender();
  f.dispatchLikeProduction();

  assert(f.noteHeld(synth));
  assert(f.runtimeActive(synth));
  const uint32_t deadlineBefore = f.runtimeDeadline(synth);
  assert(deadlineBefore > 360u * PhraseRuntime::kSubticksPerTick);
  assert(f.midi.activeGateCount(
             MusicalEventSource::PatternPlayer,
             targetForSynth(synth), 0) == 1);

  f.engine.songBarIndex_ = 0;
  f.beginRender();
  processTick(f, 384);

  // The physical Song source transition is now one owner-aware hard barrier.
  // It must retire the internal runtime owner synchronously, before the queued
  // PatternPlayer NoteOff is dispatched to the external MIDI endpoint.
  assert(f.engine.currentSongPosition() == 1);
  assert(f.engine.current303PatternIndex(synth) == 1);
  assert(!f.noteHeld(synth));
  assert(!f.runtimeActive(synth));
  assert(f.runtimeDeadline(synth) == 0);
  assert(f.engine.patternMidiNotes_[synth] == -1);

  f.endRender();
  f.dispatchLikeProduction();

  assert(f.midi.activeGateCount(
             MusicalEventSource::PatternPlayer,
             targetForSynth(synth), 0) == 0);
  assert(countWire(f.usb.packets, WireType::NoteOn,
                   channelForSynth(synth)) == 1);
  assert(countWire(f.usb.packets, WireType::NoteOff,
                   channelForSynth(synth)) == 1);
  f.assertEndpointParity();
}

void testSongBoundaryCleanupConverges() {
  testSongBoundaryCleanupConvergesForSynth(0);
  testSongBoundaryCleanupConvergesForSynth(1);
  std::puts(
      "P0-C RATIFIED: Song row @384 closes internal and PatternPlayer gates");
}

}  // namespace

int main() {
  testGridStepsAreSchedulerNoOp();
  testExact384TickBoundary();
  testSongBoundaryCleanupConverges();
  testLegacyTieCrossingSymptom();
  testSwingPlusMicrotimingWrapsLateStep();
  testNegativeMicrotimingWrapsStepZero();
  std::puts("PATTERN/PHRASE P0 runtime owner-cleanup ratification: OK");
  return 0;
}
