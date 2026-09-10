#pragma once

// P3 DRAM characterization scenario — DIAGNOSTIC IMAGE ONLY.
//
// The shipped firmware never reaches the PHRASE sequenced source: nothing in
// production calls setSequencedSource(), so sequencedSource_[] stays Pattern and
// currentPhrase_[] stays empty. Measuring runtime memory on the product image
// would therefore prove only that the buffers are resident, not that the
// phrase-relative playback path executes.
//
// This header makes that path reachable for measurement. The whole body is
// compiled out unless P3_DRAM_CHARACTERIZATION is defined, which only the
// runtime-instrumented source copy does. The product ELF must contain none of
// these symbols.

#if defined(P3_DRAM_CHARACTERIZATION)

// No includes: this header is injected into the sketch translation unit after
// the engine instance is declared, so MiniAcid, PhraseRuntime and the Arduino
// runtime are already visible. Including them again would pull
// runtime_synth_events.h in by a second path, which defeats its #pragma once
// under the Arduino build's copied source tree.

namespace P3DramCharacterization {

// Worst case for the phrase scan: the buffer filled to its bound.
constexpr uint8_t kBars = PhraseRuntime::kMaxPhraseBars;              // 8
constexpr uint16_t kSpanTicks = kBars * PhraseRuntime::kTicksPerBar;  // 3072

// The note that must survive a bar boundary: starts at 360, runs 96 ticks, so
// it releases at 456 with the boundary at 384 in between. Its neighbourhood is
// left empty so nothing preempts it before its own deadline.
constexpr uint16_t kCrossBarStartTick = 360;
constexpr uint16_t kCrossBarDurationSubticks =
    static_cast<uint16_t>(96 * PhraseRuntime::kSubticksPerTick);  // 1536

// A deliberate preemption: two onsets close enough that the second arrives
// while the first is still sounding. RuntimeSynthPlaybackState is monophonic,
// so this exercises the Release-then-Start pair in one action batch.
constexpr uint16_t kOverlapFirstTick = 1200;
constexpr uint16_t kOverlapSecondTick = 1208;

inline void fillPhrase(PhraseRuntime::RuntimeSynthEventBuffer& phrase) {
  phrase.count = 0;
  phrase.lengthTicks = kSpanTicks;

  auto push = [&phrase](uint16_t startTick, uint8_t note, uint16_t durationSubticks) {
    if (phrase.count >= PhraseRuntime::kMaxSynthEvents) return;
    PhraseRuntime::RuntimeSynthEvent& event = phrase.events[phrase.count++];
    event.startTick = startTick;
    event.durationSubticks = durationSubticks;
    event.note = note;
    event.velocity = 100;
    // probability 100 and no ghost flag keep triggerSynthStep_ on its
    // zero-RNG-draw path, so the scenario is deterministic run to run.
    event.probability = 100;
    event.flags = 0;
    event.fx = 0;
    event.fxParam = 0;
  };

  push(kCrossBarStartTick, 48, kCrossBarDurationSubticks);
  push(kOverlapFirstTick, 50, static_cast<uint16_t>(64 * PhraseRuntime::kSubticksPerTick));
  push(kOverlapSecondTick, 55, static_cast<uint16_t>(8 * PhraseRuntime::kSubticksPerTick));

  // Fill the rest on the step grid, skipping the ticks reserved above so the
  // cross-bar and overlap cases are not disturbed.
  for (uint16_t tick = 0; tick < kSpanTicks; tick += 24) {
    if (tick == kCrossBarStartTick || tick == 384 || tick == 456) continue;
    if (tick == 1200 || tick == 1224) continue;
    push(tick, static_cast<uint8_t>(36 + (tick / 24) % 24),
         static_cast<uint16_t>(8 * PhraseRuntime::kSubticksPerTick));
  }
}

enum class Stage : uint8_t {
  Idle,
  Loaded,
  PlaybackStart,
  CrossBar,
  Peak,
  Stopping,
  Restarting,
  Done,
};

struct State {
  Stage stage = Stage::Idle;
  uint32_t stageStartedMs = 0;
  uint8_t cycle = 0;
};

inline State& state() {
  static State s;
  return s;
}

// One phrase span at 120 BPM is 3072 ticks at 192 ticks/s, i.e. ~16 s.
constexpr uint32_t kSpanMs = 16000;
constexpr uint8_t kCycles = 3;

inline void begin(MiniAcid& engine) {
  fillPhrase(engine.currentPhraseBuffer(0));
  engine.setPhraseLength(0, kBars);
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
  State& s = state();
  s.stage = Stage::Loaded;
  s.stageStartedMs = millis();
  s.cycle = 0;
}

// Returns a telemetry phase name when a new stage is entered, otherwise
// nullptr. The caller logs it, so this header stays independent of whichever
// telemetry function the instrumented build injects.
inline const char* poll(MiniAcid& engine) {
  State& s = state();
  const uint32_t now = millis();
  const uint32_t elapsed = now - s.stageStartedMs;

  switch (s.stage) {
    case Stage::Loaded:
      engine.start();
      s.stage = Stage::PlaybackStart;
      s.stageStartedMs = now;
      return "p3-phrase-loaded";

    case Stage::PlaybackStart:
      if (elapsed < 1000) return nullptr;
      s.stage = Stage::CrossBar;
      s.stageStartedMs = now;
      return "p3-playback-start";

    case Stage::CrossBar:
      // Past the first phrase span, so the cross-bar note has started at 360,
      // survived the boundary at 384 and released at 456 at least once.
      if (elapsed < kSpanMs) return nullptr;
      s.stage = Stage::Peak;
      s.stageStartedMs = now;
      return "p3-cross-bar";

    case Stage::Peak:
      if (elapsed < kSpanMs) return nullptr;
      engine.stop();
      s.stage = Stage::Stopping;
      s.stageStartedMs = now;
      return "p3-peak";

    case Stage::Stopping:
      if (elapsed < 2000) return nullptr;
      s.stage = (s.cycle + 1 >= kCycles) ? Stage::Done : Stage::Restarting;
      s.stageStartedMs = now;
      return (s.cycle == 0)   ? "p3-stop-1"
             : (s.cycle == 1) ? "p3-stop-2"
                              : "p3-stop-3";

    case Stage::Restarting:
      engine.start();
      ++s.cycle;
      s.stage = Stage::Peak;
      s.stageStartedMs = now;
      return (s.cycle == 1)   ? "p3-restart-1"
             : (s.cycle == 2) ? "p3-restart-2"
                              : "p3-restart-3";

    case Stage::Idle:
    case Stage::Done:
    default:
      return nullptr;
  }
}

}  // namespace P3DramCharacterization

#endif  // P3_DRAM_CHARACTERIZATION
