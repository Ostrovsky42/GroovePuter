#pragma once

#include <cstddef>
#include <cstdint>

// Generated from SEQTRAK Pattern Atlas schema 2.6.0.
// Do not edit manually.

namespace AtlasGenerated {

enum EventFlags : uint8_t {
  kActive = 1u << 0,
  kAccent = 1u << 1,
  kSlide = 1u << 2,
  kSustain = 1u << 3,
};

struct Event {
  uint8_t target;
  uint8_t step;
  int8_t note;
  uint8_t velocity;
  int8_t timing;
  uint8_t probability;
  uint8_t flags;
};

struct Pattern {
  const char* atlasPatternId;
  const char* slotId;
  const char* slotFunction;
  const Event* events;
  uint16_t eventCount;
};

struct Recipe {
  uint8_t runtimeRecipeId;
  const char* atlasRecipeId;
  const char* displayName;
  uint16_t bpm;
  uint8_t swingPercent;
  const Pattern* patterns;
  uint8_t patternCount;
};

}  // namespace AtlasGenerated
