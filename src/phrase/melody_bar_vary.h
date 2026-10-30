#pragma once
#ifndef GROOVEPUTER_SRC_PHRASE_MELODY_BAR_VARY_H
#define GROOVEPUTER_SRC_PHRASE_MELODY_BAR_VARY_H

#include <algorithm>
#include <cstdint>

#include "runtime_synth_events.h"
#include "../ui/project_key.h"

// D on the MELODY page: develop one bar in place. Pure: the result is the
// input with only the events whose start lies inside the bar changed.
namespace MelodyBarVary {

using Buffer = PhraseRuntime::RuntimeSynthEventBuffer;
using Event = PhraseRuntime::RuntimeSynthEvent;

// Variant 0 is the original bar; D cycles 1..3 and then returns to 0.
enum Kind : uint8_t { Original = 0, Answer = 1, Push = 2, Mirror = 3 };
constexpr uint8_t kVariants = 4;

inline const char* name(uint8_t kind) {
  switch (kind) {
    case Answer: return "ANSWER";
    case Push: return "PUSH";
    case Mirror: return "MIRROR";
    default: return "ORIGINAL";
  }
}

constexpr uint16_t kSixteenthTicks = PhraseRuntime::kTicksPerBar / 16u;

namespace detail {
// `degrees` scale steps from `note` (negative = down); clamps to the last
// reachable key note and never leaves 0..127.
inline int walk(int note, int degrees, uint8_t root,
                ProjectKey::ScaleTypeValue scale) {
  int cur = note;
  const int dir = degrees >= 0 ? 1 : -1;
  for (int i = 0; i < (degrees < 0 ? -degrees : degrees); ++i) {
    const int next = ProjectKey::step(cur, dir, root, scale);
    if (next < 0) break;
    cur = next;
  }
  return cur;
}
// Scale steps from `from` to `to` (signed).
inline int degreesBetween(int from, int to, uint8_t root,
                          ProjectKey::ScaleTypeValue scale) {
  int d = 0;
  int cur = from;
  const int dir = to >= from ? 1 : -1;
  while (cur != to && d < 128) {
    const int next = ProjectKey::step(cur, dir, root, scale);
    if (next < 0 || (dir > 0 ? next > to : next < to)) break;
    cur = next;
    d += dir;
  }
  return d;
}
}  // namespace detail

inline bool barHasEvents(const Buffer& in, uint8_t bar) {
  const uint32_t lo = static_cast<uint32_t>(bar) * PhraseRuntime::kTicksPerBar;
  const uint32_t hi = lo + PhraseRuntime::kTicksPerBar;
  for (uint16_t i = 0; i < in.count; ++i) {
    if (in.events[i].startTick >= lo && in.events[i].startTick < hi) return true;
  }
  return false;
}

inline Buffer varyBar(const Buffer& in, uint8_t bar, uint8_t kind,
                      uint8_t root, ProjectKey::ScaleTypeValue scale) {
  Buffer out = in;
  if (kind == Original || kind >= kVariants) return out;
  const uint32_t lo = static_cast<uint32_t>(bar) * PhraseRuntime::kTicksPerBar;
  const uint32_t hi = lo + PhraseRuntime::kTicksPerBar;
  if (hi > in.lengthTicks) return out;
  const auto inBar = [&](const Event& e) {
    return e.startTick >= lo && e.startTick < hi;
  };

  if (kind == Answer) {
    // The last note of the bar moves two scale degrees (up; down at the top).
    int last = -1;
    for (uint16_t i = 0; i < out.count; ++i) {
      if (inBar(out.events[i]) &&
          (last < 0 || out.events[i].startTick >= out.events[last].startTick)) {
        last = i;
      }
    }
    if (last >= 0) {
      const int n = out.events[last].note;
      int moved = detail::walk(n, 2, root, scale);
      if (moved == n) moved = detail::walk(n, -2, root, scale);
      out.events[last].note = static_cast<uint8_t>(moved);
    }
  } else if (kind == Push) {
    // Everything one 16th later, wrapping inside the bar. A note on beat 1
    // stays put (the downbeat is the anchor); the rest wrap over beats 1+.
    bool anchor = false;
    for (uint16_t i = 0; i < out.count; ++i) {
      if (out.events[i].startTick == lo) anchor = true;
    }
    const uint32_t wrapTo = anchor ? kSixteenthTicks : 0u;
    uint16_t slots[PhraseRuntime::kMaxSynthEvents];
    uint16_t n = 0;
    for (uint16_t i = 0; i < out.count; ++i) {
      if (!inBar(out.events[i])) continue;
      slots[n++] = i;
      Event& e = out.events[i];
      if (anchor && e.startTick == lo) continue;
      uint32_t rel = (e.startTick - lo) + kSixteenthTicks;
      if (rel >= PhraseRuntime::kTicksPerBar) {
        rel = rel - PhraseRuntime::kTicksPerBar + wrapTo;
      }
      e.startTick = static_cast<uint16_t>(lo + rel);
      const uint32_t room = (hi - e.startTick) * PhraseRuntime::kSubticksPerTick;
      if (e.durationSubticks > room) e.durationSubticks = static_cast<uint16_t>(room);
    }
    // Keep the bar's slots in time order (stable, same slots as before).
    Event tmp[PhraseRuntime::kMaxSynthEvents];
    for (uint16_t k = 0; k < n; ++k) tmp[k] = out.events[slots[k]];
    std::stable_sort(tmp, tmp + n, [](const Event& a, const Event& b) {
      return a.startTick < b.startTick;
    });
    for (uint16_t k = 0; k < n; ++k) out.events[slots[k]] = tmp[k];
  } else {
    // Invert every pitch around the bar's first note, in the key.
    int first = -1;
    for (uint16_t i = 0; i < out.count; ++i) {
      if (inBar(out.events[i]) &&
          (first < 0 || out.events[i].startTick < out.events[first].startTick)) {
        first = i;
      }
    }
    if (first >= 0) {
      const int pivot = out.events[first].note;
      for (uint16_t i = 0; i < out.count; ++i) {
        if (!inBar(out.events[i]) || i == static_cast<uint16_t>(first)) continue;
        const int n = out.events[i].note;
        const int d = detail::degreesBetween(pivot, n, root, scale);
        out.events[i].note =
            static_cast<uint8_t>(detail::walk(pivot, -d, root, scale));
      }
    }
  }
  return out;
}

}  // namespace MelodyBarVary

#endif
