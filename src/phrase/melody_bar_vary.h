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

// Variant 0 is the original bar; D cycles 1..5 and then returns to 0:
//   1 ANSWER    - Cadential tail answer (+2 scale steps on final onset, leading approach)
//   2 SEQUENCE  - Diatonic sequence (second half of the bar shifted +1 scale degree)
//   3 SYNCOPATE - Off-beat 16th anticipation on weak beats (downbeat stays anchored, no wrap)
//   4 CONTOUR   - Register-bounded contour inversion + neighbour motion on repeated notes
//   5 OCTAVE    - Acid octave jump (+12/-12) + accent/slide on alternating onsets
enum Kind : uint8_t {
  Original = 0,
  Answer = 1,
  Sequence = 2,
  Syncopate = 3,
  Contour = 4,
  Octave = 5,
};
constexpr uint8_t kVariants = 6;

inline const char* name(uint8_t kind) {
  switch (kind) {
    case Answer: return "ANSWER";
    case Sequence: return "SEQUENCE";
    case Syncopate: return "SYNCOPATE";
    case Contour: return "CONTOUR";
    case Octave: return "OCTAVE";
    default: return "ORIGINAL";
  }
}

constexpr uint16_t kSixteenthTicks = PhraseRuntime::kTicksPerBar / 16u;
constexpr uint16_t kEighthTicks = PhraseRuntime::kTicksPerBar / 8u;
constexpr uint16_t kBeatTicks = PhraseRuntime::kTicksPerBar / 4u;
constexpr uint16_t kHalfBarTicks = PhraseRuntime::kTicksPerBar / 2u;

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
  while (cur != to && d < 128 && d > -128) {
    const int next = ProjectKey::step(cur, dir, root, scale);
    if (next < 0 || (dir > 0 ? next > to : next < to)) break;
    cur = next;
    d += dir;
  }
  return d;
}

// Collect unique sorted startTicks inside [lo, hi).
inline uint16_t collectBarOnsets(const Buffer& buf, uint32_t lo, uint32_t hi,
                                 uint16_t (&onsets)[PhraseRuntime::kMaxSynthEvents]) {
  uint16_t count = 0;
  for (uint16_t i = 0; i < buf.count; ++i) {
    const uint16_t t = buf.events[i].startTick;
    if (t < lo || t >= hi) continue;
    bool exists = false;
    for (uint16_t k = 0; k < count; ++k) {
      if (onsets[k] == t) {
        exists = true;
        break;
      }
    }
    if (!exists) onsets[count++] = t;
  }
  std::sort(onsets, onsets + count);
  return count;
}

inline void sortBarSlotsInPlace(Buffer& out, uint32_t lo, uint32_t hi) {
  uint16_t slots[PhraseRuntime::kMaxSynthEvents];
  Event tmp[PhraseRuntime::kMaxSynthEvents];
  uint16_t n = 0;
  for (uint16_t i = 0; i < out.count; ++i) {
    if (out.events[i].startTick >= lo && out.events[i].startTick < hi) {
      slots[n] = i;
      tmp[n] = out.events[i];
      ++n;
    }
  }
  std::stable_sort(tmp, tmp + n, [](const Event& a, const Event& b) {
    return a.startTick < b.startTick;
  });
  for (uint16_t k = 0; k < n; ++k) {
    out.events[slots[k]] = tmp[k];
  }
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

  uint16_t onsets[PhraseRuntime::kMaxSynthEvents];
  const uint16_t onsetCount = detail::collectBarOnsets(out, lo, hi, onsets);
  if (onsetCount == 0) return out;

  if (kind == Answer) {
    // Question-and-answer: keep the opening intact and resolve the tail of the
    // bar two scale degrees up (or down at the ceiling). When a penultimate
    // onset in the second half would collide with the resolved final pitch,
    // step it one degree to create a smooth cadential approach.
    const uint16_t lastTick = onsets[onsetCount - 1];
    int lastNewNote = -1;
    for (uint16_t i = 0; i < out.count; ++i) {
      if (!inBar(out.events[i]) || out.events[i].startTick != lastTick) continue;
      const int n = out.events[i].note;
      int moved = detail::walk(n, 2, root, scale);
      if (moved == n) moved = detail::walk(n, -2, root, scale);
      out.events[i].note = static_cast<uint8_t>(moved);
      lastNewNote = moved;
    }
    if (onsetCount >= 3 && lastNewNote >= 0) {
      const uint16_t prevTick = onsets[onsetCount - 2];
      if (prevTick >= lo + kHalfBarTicks) {
        for (uint16_t i = 0; i < out.count; ++i) {
          if (!inBar(out.events[i]) || out.events[i].startTick != prevTick) continue;
          if (out.events[i].note == static_cast<uint8_t>(lastNewNote)) {
            const int stepped = detail::walk(lastNewNote, -1, root, scale);
            out.events[i].note = static_cast<uint8_t>(stepped);
          }
        }
      }
    }
  } else if (kind == Sequence) {
    // Diatonic sequence: keep the first half of the bar as the statement and
    // transpose the second half up by one scale degree (or down when high).
    // If all notes sit in the first half, transpose the second half of the
    // onset list so Sequence always produces a distinct musical answer.
    uint16_t splitTick = static_cast<uint16_t>(lo + kHalfBarTicks);
    bool hasSecondHalf = false;
    int sumPitch = 0;
    int pitchCount = 0;
    for (uint16_t i = 0; i < out.count; ++i) {
      if (!inBar(out.events[i])) continue;
      if (out.events[i].startTick >= splitTick) hasSecondHalf = true;
      sumPitch += out.events[i].note;
      ++pitchCount;
    }
    if (!hasSecondHalf) {
      splitTick = onsets[onsetCount / 2];
    }
    const int avgPitch = pitchCount > 0 ? (sumPitch / pitchCount) : 60;
    const int stepDir = avgPitch > 78 ? -1 : 1;
    // If the bar has only 1 onset, shift by -1 degree so Sequence differs from Answer (+2).
    const int seqDegrees = (onsetCount == 1) ? -1 : stepDir;
    for (uint16_t i = 0; i < out.count; ++i) {
      if (!inBar(out.events[i])) continue;
      if (onsetCount > 1 && out.events[i].startTick < splitTick) continue;
      const int n = out.events[i].note;
      int moved = detail::walk(n, seqDegrees, root, scale);
      if (moved == n) moved = detail::walk(n, -seqDegrees, root, scale);
      out.events[i].note = static_cast<uint8_t>(moved);
    }
  } else if (kind == Syncopate) {
    // Musical syncopation: Beat 1 (rel == 0) stays anchored on the downbeat;
    // weak beats (Beat 2 at 96, Beat 4 at 288, or off-downbeat eighths) shift
    // by one 16th into an anticipation (or delay if the preceding 16th is
    // occupied), without wrapping around the bar boundary.
    auto hasOnsetAt = [&](uint16_t tick) {
      for (uint16_t k = 0; k < onsetCount; ++k) {
        if (onsets[k] == tick) return true;
      }
      return false;
    };

    bool movedAny = false;
    uint16_t newOnsetTick[PhraseRuntime::kMaxSynthEvents];
    for (uint16_t k = 0; k < onsetCount; ++k) newOnsetTick[k] = onsets[k];

    for (uint16_t k = 0; k < onsetCount; ++k) {
      const uint16_t oldTick = onsets[k];
      const uint16_t rel = static_cast<uint16_t>(oldTick - lo);
      if (rel == 0 && onsetCount > 1) continue;  // Anchor the downbeat.

      // Prefer weak beats (Beat 2 and Beat 4), or Beat 3 when only 2 onsets exist,
      // or any even eighth when no quarter weak beats exist.
      const bool isWeakBeat =
          (rel == kBeatTicks) || (rel == 3u * kBeatTicks) ||
          (onsetCount <= 2 && rel == kHalfBarTicks) ||
          (rel > 0 && (rel % kEighthTicks) == 0 && rel != kHalfBarTicks);
      if (!isWeakBeat && onsetCount > 2) continue;

      uint16_t targetTick = oldTick;
      const uint16_t prevEndMin =
          (k > 0) ? static_cast<uint16_t>(newOnsetTick[k - 1] + kSixteenthTicks)
                  : static_cast<uint16_t>(lo);
      if (oldTick >= lo + kSixteenthTicks &&
          oldTick - kSixteenthTicks >= prevEndMin &&
          !hasOnsetAt(static_cast<uint16_t>(oldTick - kSixteenthTicks))) {
        targetTick = static_cast<uint16_t>(oldTick - kSixteenthTicks);
      } else if (static_cast<uint32_t>(oldTick) + kSixteenthTicks < hi &&
                 (k + 1 >= onsetCount ||
                  oldTick + kSixteenthTicks < onsets[k + 1]) &&
                 !hasOnsetAt(static_cast<uint16_t>(oldTick + kSixteenthTicks))) {
        targetTick = static_cast<uint16_t>(oldTick + kSixteenthTicks);
      }
      if (targetTick != oldTick) {
        newOnsetTick[k] = targetTick;
        movedAny = true;
      }
    }

    // Fallback: if none of the onsets were on even eighths (e.g. already all
    // off-beats), nudge the last movable onset by one 16th inside the bar.
    if (!movedAny) {
      for (int k = static_cast<int>(onsetCount) - 1; k >= 0; --k) {
        const uint16_t oldTick = onsets[k];
        if (static_cast<uint32_t>(oldTick) + kSixteenthTicks < hi &&
            (k + 1 >= static_cast<int>(onsetCount) ||
             oldTick + kSixteenthTicks < onsets[k + 1])) {
          newOnsetTick[k] = static_cast<uint16_t>(oldTick + kSixteenthTicks);
          movedAny = true;
          break;
        }
        const uint16_t prevFloor =
            (k > 0) ? static_cast<uint16_t>(onsets[k - 1] + kSixteenthTicks)
                    : static_cast<uint16_t>(lo);
        if (oldTick >= prevFloor + kSixteenthTicks) {
          newOnsetTick[k] = static_cast<uint16_t>(oldTick - kSixteenthTicks);
          movedAny = true;
          break;
        }
      }
    }

    // Apply onset shifts and trim any preceding note that would now overlap
    // an anticipated note.
    for (uint16_t i = 0; i < out.count; ++i) {
      if (!inBar(out.events[i])) continue;
      for (uint16_t k = 0; k < onsetCount; ++k) {
        if (out.events[i].startTick == onsets[k]) {
          out.events[i].startTick = newOnsetTick[k];
          const uint32_t maxRoom =
              (hi - out.events[i].startTick) * PhraseRuntime::kSubticksPerTick;
          if (out.events[i].durationSubticks > maxRoom) {
            out.events[i].durationSubticks = static_cast<uint16_t>(maxRoom);
          }
          break;
        }
      }
    }
    for (uint16_t i = 0; i < out.count; ++i) {
      if (!inBar(out.events[i])) continue;
      for (uint16_t k = 1; k < onsetCount; ++k) {
        if (newOnsetTick[k] < onsets[k] &&
            out.events[i].startTick < newOnsetTick[k]) {
          const uint32_t endSub =
              static_cast<uint32_t>(out.events[i].startTick) *
                  PhraseRuntime::kSubticksPerTick +
              out.events[i].durationSubticks;
          const uint32_t nextStartSub =
              static_cast<uint32_t>(newOnsetTick[k]) *
              PhraseRuntime::kSubticksPerTick;
          if (endSub > nextStartSub) {
            out.events[i].durationSubticks = static_cast<uint16_t>(
                nextStartSub -
                static_cast<uint32_t>(out.events[i].startTick) *
                    PhraseRuntime::kSubticksPerTick);
          }
        }
      }
    }
    detail::sortBarSlotsInPlace(out, lo, hi);
  } else if (kind == Contour) {
    // Register-bounded contour inversion around the bar's first note +
    // upper/lower neighbour motion when notes repeat on the pivot pitch.
    int first = -1;
    for (uint16_t i = 0; i < out.count; ++i) {
      if (inBar(out.events[i]) &&
          (first < 0 || out.events[i].startTick < out.events[first].startTick)) {
        first = i;
      }
    }
    if (first >= 0) {
      const int pivot = out.events[first].note;
      int stepIndex = 0;
      for (uint16_t k = 0; k < onsetCount; ++k) {
        const uint16_t tick = onsets[k];
        for (uint16_t i = 0; i < out.count; ++i) {
          if (!inBar(out.events[i]) || out.events[i].startTick != tick) continue;
          if (i == static_cast<uint16_t>(first) && onsetCount > 1) continue;
          ++stepIndex;
          const int n = out.events[i].note;
          const int d = detail::degreesBetween(pivot, n, root, scale);
          int target = n;
          if (d == 0) {
            // Repeated pivot note: weave upper and lower diatonic neighbours.
            const int dir = (stepIndex & 1) ? 1 : -1;
            target = detail::walk(pivot, dir, root, scale);
            if (target == pivot) target = detail::walk(pivot, -dir, root, scale);
          } else {
            target = detail::walk(pivot, -d, root, scale);
            // Keep the inverted contour within a musical ±9 semitone corridor
            // around the pivot so basslines and leads never fly into extreme octaves.
            while (target - pivot > 9 && target >= 12) target -= 12;
            while (pivot - target > 9 && target <= 115) target += 12;
          }
          out.events[i].note = static_cast<uint8_t>(std::clamp(target, 0, 127));
        }
      }
    }
  } else if (kind == Octave) {
    // Acid octave / accent / slide pulse: keep the opening onset grounded and
    // lift alternating onsets by an octave (+12, or -12 above C7) with accent,
    // adding slide on the approach note.
    for (uint16_t k = 0; k < onsetCount; ++k) {
      const bool jump = (onsetCount == 1) ? true : ((k % 2u) == 1u);
      const uint16_t tick = onsets[k];
      // Find highest note at this onset so chords only octave-shift their top voice.
      int topIdx = -1;
      for (uint16_t i = 0; i < out.count; ++i) {
        if (!inBar(out.events[i]) || out.events[i].startTick != tick) continue;
        if (topIdx < 0 || out.events[i].note >= out.events[topIdx].note) {
          topIdx = i;
        }
      }
      if (topIdx < 0) continue;
      if (jump) {
        const int n = out.events[topIdx].note;
        const int shifted = (n + 12 <= 96) ? (n + 12) : std::max(0, n - 12);
        out.events[topIdx].note = static_cast<uint8_t>(shifted);
        out.events[topIdx].flags |= PhraseRuntime::kEventAccent;
      } else if (k + 1 < onsetCount) {
        out.events[topIdx].flags |= PhraseRuntime::kEventSlide;
      }
    }
  }
  return out;
}

}  // namespace MelodyBarVary

#endif
