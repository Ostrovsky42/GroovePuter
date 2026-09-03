#pragma once

#include <cstdint>
#include <type_traits>

#include "../../scenes.h"

namespace PhraseRuntime {

constexpr uint16_t kTicksPerBar = 384;
constexpr uint16_t kSubticksPerTick = 16;
constexpr uint8_t kMaxPhraseBars = 8;
constexpr uint16_t kMaxSynthEvents =
    static_cast<uint16_t>(SynthPattern::kSteps * kMaxPhraseBars);

enum RuntimeSynthEventFlag : uint8_t {
  kEventAccent = 1u << 0u,
  kEventSlide = 1u << 1u,
  kEventGhost = 1u << 2u,
};

struct RuntimeSynthEvent {
  uint16_t startTick = 0;
  uint16_t durationSubticks = 0;
  uint8_t note = 0;
  uint8_t velocity = 100;
  uint8_t probability = 100;
  uint8_t flags = 0;
  uint8_t fx = 0;
  uint8_t fxParam = 0;
};

struct RuntimeSynthEventBuffer {
  RuntimeSynthEvent events[kMaxSynthEvents]{};
  uint16_t count = 0;
  uint16_t lengthTicks = kTicksPerBar;
};

struct PatternProjectionSettings {
  uint8_t synthIndex = 0;
  uint8_t swingPercent = 50;
  bool swingEnabled = false;
  float gateLengthRatio = 0.5f;
};

enum class PatternProjectionStatus : uint8_t {
  Ready = 0,
  InvalidSynthIndex,
};

PatternProjectionStatus projectPatternToRuntimeEvents(
    const SynthPattern& pattern,
    const PatternProjectionSettings& settings,
    RuntimeSynthEventBuffer& destination);

static_assert(kMaxSynthEvents == 128,
              "P1 future Phrase capacity must remain 16 onsets x 8 bars");
static_assert(sizeof(RuntimeSynthEvent) == 10,
              "P1 runtime synth event ABI changed");
static_assert(sizeof(RuntimeSynthEventBuffer) == 1284,
              "P1 runtime synth event buffer ABI changed");
static_assert(std::is_trivially_copyable<RuntimeSynthEvent>::value,
              "runtime synth event must remain fixed-capacity");
static_assert(std::is_trivially_copyable<RuntimeSynthEventBuffer>::value,
              "runtime synth event buffer must remain fixed-capacity");

}  // namespace PhraseRuntime
