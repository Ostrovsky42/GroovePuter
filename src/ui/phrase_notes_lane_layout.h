#pragma once

#include <algorithm>
#include <array>
#include <cstdint>

#include "phrase_notes_projection.h"

namespace PhraseNotesLaneLayout {

constexpr uint8_t kOverflowLane = 0xFFu;

struct Layout {
  std::array<uint8_t, PhraseRuntime::kMaxSynthEvents> laneByEvent{};
  uint8_t laneCount = 0;
  uint16_t overflowCount = 0;
};

namespace detail {

inline bool eventLess(const PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                      uint16_t lhsIndex,
                      uint16_t rhsIndex) {
  const auto& lhs = phrase.events[lhsIndex];
  const auto& rhs = phrase.events[rhsIndex];
  if (lhs.startTick != rhs.startTick) return lhs.startTick < rhs.startTick;
  if (lhs.durationSubticks != rhs.durationSubticks) {
    return lhs.durationSubticks < rhs.durationSubticks;
  }
  if (lhs.note != rhs.note) return lhs.note < rhs.note;
  if (lhs.velocity != rhs.velocity) return lhs.velocity < rhs.velocity;
  if (lhs.probability != rhs.probability) return lhs.probability < rhs.probability;
  if (lhs.flags != rhs.flags) return lhs.flags < rhs.flags;
  if (lhs.fx != rhs.fx) return lhs.fx < rhs.fx;
  if (lhs.fxParam != rhs.fxParam) return lhs.fxParam < rhs.fxParam;
  return lhsIndex < rhsIndex;
}

}  // namespace detail

inline Layout build(const PhraseRuntime::RuntimeSynthEventBuffer& phrase,
                    uint32_t windowStartSubtick,
                    uint32_t windowSubticks,
                    uint8_t maxLanes) {
  Layout out{};
  out.laneByEvent.fill(kOverflowLane);
  if (!PhraseNotesProjection::validate(phrase) || windowSubticks == 0) return out;

  const uint32_t windowEndSubtick = windowStartSubtick + windowSubticks;
  std::array<uint16_t, PhraseRuntime::kMaxSynthEvents> visibleIndices{};
  uint16_t visibleCount = 0;

  for (uint16_t i = 0; i < phrase.count; ++i) {
    PhraseNotesProjection::NoteSpan span{};
    if (!PhraseNotesProjection::project(phrase, i, span)) continue;
    if (span.endSubtick <= windowStartSubtick || span.startSubtick >= windowEndSubtick) {
      continue;
    }
    visibleIndices[visibleCount++] = i;
  }

  std::sort(visibleIndices.begin(), visibleIndices.begin() + visibleCount,
            [&phrase](uint16_t lhs, uint16_t rhs) {
              return detail::eventLess(phrase, lhs, rhs);
            });

  std::array<uint32_t, PhraseRuntime::kMaxSynthEvents> laneEnd{};
  for (uint16_t position = 0; position < visibleCount; ++position) {
    const uint16_t eventIndex = visibleIndices[position];
    PhraseNotesProjection::NoteSpan span{};
    if (!PhraseNotesProjection::project(phrase, eventIndex, span)) continue;

    const uint32_t clippedStart = std::max(span.startSubtick, windowStartSubtick);
    const uint32_t clippedEnd = std::min(span.endSubtick, windowEndSubtick);

    uint8_t lane = kOverflowLane;
    for (uint8_t candidate = 0; candidate < out.laneCount; ++candidate) {
      if (laneEnd[candidate] <= clippedStart) {
        lane = candidate;
        break;
      }
    }

    if (lane == kOverflowLane && out.laneCount < maxLanes) {
      lane = out.laneCount++;
    }

    if (lane == kOverflowLane) {
      ++out.overflowCount;
      continue;
    }

    out.laneByEvent[eventIndex] = lane;
    laneEnd[lane] = clippedEnd;
  }

  return out;
}

}  // namespace PhraseNotesLaneLayout
