#pragma once

// P3 audible PATTERN/PHRASE A/B — DIAGNOSTIC IMAGE ONLY.
//
// The shipped firmware never reaches the PHRASE sequenced source: nothing in
// production calls setSequencedSource(), so sequencedSource_[] stays Pattern and
// currentPhrase_[] stays empty. Measuring runtime memory on the product image
// would therefore prove only that the buffers are resident, not that the
// phrase-relative playback path executes.
//
// This header makes that path reachable for measurement. The whole body is
// compiled out unless P3_AUDIBLE_AB is defined, which only the
// runtime-instrumented source copy does. The product ELF must contain none of
// these symbols.

#if defined(P3_AUDIBLE_AB)

// No includes: this header is injected into the sketch translation unit after
// the engine instance is declared, so MiniAcid, PhraseRuntime and the Arduino
// runtime are already visible. Including them again would pull
// runtime_synth_events.h in by a second path, which defeats its #pragma once
// under the Arduino build's copied source tree.

namespace P3AudibleAB {

// Audible A/B material. Two bars, deliberately sparse: the point is to hear the
// difference, not to stress the scan. The dense 128-event version used for the
// memory characterization is in this file's history.
constexpr uint8_t kBars = 2;
constexpr uint16_t kSpanTicks = kBars * PhraseRuntime::kTicksPerBar;  // 768

inline void fillPhrase(PhraseRuntime::RuntimeSynthEventBuffer& phrase) {
  phrase.count = 0;
  phrase.lengthTicks = kSpanTicks;

  auto push = [&phrase](uint16_t startTick, uint8_t note, uint16_t durationTicks) {
    if (phrase.count >= PhraseRuntime::kMaxSynthEvents) return;
    PhraseRuntime::RuntimeSynthEvent& event = phrase.events[phrase.count++];
    event.startTick = startTick;
    event.durationSubticks =
        static_cast<uint16_t>(durationTicks * PhraseRuntime::kSubticksPerTick);
    event.note = note;
    event.velocity = 100;
    event.probability = 100;
    event.flags = 0;
    event.fx = 0;
    event.fxParam = 0;
  };

  // Bar 1: three short notes, then one that starts at 360 and runs 96 ticks, so
  // it is still sounding when the bar boundary passes at 384 and releases at 456.
  push(0,   40, 20);
  push(96,  45, 20);
  push(192, 47, 20);
  push(360, 52, 96);

  // Bar 2 is deliberately not bar 1. Under bar-local addressing every bar is
  // identical, so hearing these different pitches is the phrase-relative proof.
  push(480, 36, 20);
  push(576, 38, 20);
  push(672, 43, 40);
}

enum class Stage : uint8_t {
  Waiting,
  PlayingPattern,
  PlayingPhrase,
};

struct State {
  Stage stage = Stage::Waiting;
  uint32_t stageStartedMs = 0;
};

inline State& state() {
  static State s;
  return s;
}

// Eight bars at 120 BPM is about sixteen seconds, long enough to hear the
// structure of each side before it flips.
constexpr uint32_t kSideMs = 16000;
constexpr uint32_t kWarmupMs = 8000;

inline void begin(MiniAcid& engine) {
  fillPhrase(engine.currentPhraseBuffer(0));
  engine.setPhraseLength(0, kBars);
  engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
  State& s = state();
  s.stage = Stage::Waiting;
  s.stageStartedMs = millis();
}

// Every source flip goes through the engine's own source-transition barrier, so
// a note sounding on one side cannot be left held when the other takes over.
inline const char* poll(MiniAcid& engine) {
  State& s = state();
  const uint32_t now = millis();
  const uint32_t elapsed = now - s.stageStartedMs;

  switch (s.stage) {
    case Stage::Waiting:
      if (elapsed < kWarmupMs) return nullptr;
      engine.start();
      s.stage = Stage::PlayingPattern;
      s.stageStartedMs = now;
      Serial.println("[P3-AB] PATTERN  one bar, repeating");
      return "p3-ab-pattern";

    case Stage::PlayingPattern:
      if (elapsed < kSideMs) return nullptr;
      engine.barrierPatternRuntimeSourceTransition();
      engine.setSequencedSource(0, MiniAcid::SequencedSource::Phrase);
      s.stage = Stage::PlayingPhrase;
      s.stageStartedMs = now;
      Serial.println("[P3-AB] PHRASE   two bars, note 360 rings across 384");
      return "p3-ab-phrase";

    case Stage::PlayingPhrase:
    default:
      if (elapsed < kSideMs) return nullptr;
      engine.barrierPatternRuntimeSourceTransition();
      engine.setSequencedSource(0, MiniAcid::SequencedSource::Pattern);
      s.stage = Stage::PlayingPattern;
      s.stageStartedMs = now;
      Serial.println("[P3-AB] PATTERN  one bar, repeating");
      return "p3-ab-pattern";
  }
}

}  // namespace P3AudibleAB

#endif  // P3_AUDIBLE_AB
