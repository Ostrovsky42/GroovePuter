#pragma once
#ifndef GROOVEPUTER_CALL_RESPONSE_PLANNER_H
#define GROOVEPUTER_CALL_RESPONSE_PLANNER_H

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "call_response_motif.h"
#include "call_response_phrase_capture.h"

// CALL/RESPONSE phase 1: the answer from a template (pure functions, no state).
//
// Transform ECHO_VARIANT: the answer repeats the CALL rhythm (same onset mask,
// relative to the answer start) and the CALL pitches snapped to the key, and
// changes the ending like an antecedent/consequent pair: a CALL that ends away
// from the tonic is answered with the tonic, a CALL that ends on the tonic is
// answered with the fifth (an open ending). The answer is one monophonic line.
//
// Timing: the ANSWER window is the window right after the CALL window, and
// the answer starts exactly on its downbeat (phase 9: the PARTNER DELAY
// safety sixteenth is gone, every reply is on the grid).
namespace GroovePuterCallResponse {

constexpr uint8_t kAnswerMinNote = 24;   // internal 303 range, also the MIDI answer range
constexpr uint8_t kAnswerMaxNote = 71;

// A key as a pitch-class mask (bit 0 = root). The root is always a member.
struct ResponseKey {
    uint8_t rootPitchClass{0};
    uint16_t scaleMask{0x0fffu};  // relative to the root
};

inline ResponseKey responseKeyFromIntervals(uint8_t rootPitchClass,
                                            const int8_t* intervals,
                                            uint8_t count) {
    ResponseKey key{static_cast<uint8_t>(rootPitchClass % 12u), 1u};
    for (uint8_t i = 0; intervals && i < count; ++i) {
        const int interval = ((intervals[i] % 12) + 12) % 12;
        key.scaleMask = static_cast<uint16_t>(key.scaleMask | (1u << interval));
    }
    return key;
}

// absolutePitchClasses: bit n = pitch class n (as PerformanceContext::scalePitchClasses).
inline ResponseKey responseKeyFromPitchClasses(uint8_t rootMidi, uint16_t absolutePitchClasses) {
    const uint8_t root = static_cast<uint8_t>(rootMidi % 12u);
    ResponseKey key{root, 1u};
    for (uint8_t pc = 0; pc < 12; ++pc) {
        if ((absolutePitchClasses & (1u << pc)) == 0) continue;
        key.scaleMask = static_cast<uint16_t>(key.scaleMask | (1u << ((pc + 12u - root) % 12u)));
    }
    return key;
}

// Phase 8: the answer key from a harmony context. The tonic is the key's tonic
// (HarmonySnapshot::tonal.tonicPc via PerformanceContext::tonicPc), not the
// root of the chord sounding when the CALL closes: a CALL closed over iv (Fm
// in C minor) used to get the key "F + the C-minor pitch classes", so the full
// cadence and the fifth were counted from F. chordRootMidi (the old source) is
// only the fallback when the context has no tonic (tonicPc >= 12).
inline ResponseKey responseKeyFromHarmony(uint8_t tonicPc, uint8_t chordRootMidi,
                                          uint16_t absolutePitchClasses) {
    return responseKeyFromPitchClasses(tonicPc < 12 ? tonicPc : chordRootMidi, absolutePitchClasses);
}

inline bool inKey(const ResponseKey& key, uint8_t note) {
    const uint8_t rel = static_cast<uint8_t>((note + 12u - key.rootPitchClass % 12u) % 12u);
    return (key.scaleMask & (1u << rel)) != 0;
}

// Scale tone at or below the note (the same rule as the phase-0 contour).
inline uint8_t snapDownToKey(const ResponseKey& key, uint8_t note) {
    for (int candidate = note; candidate >= 0 && candidate > note - 12; --candidate) {
        if (inKey(key, static_cast<uint8_t>(candidate))) return static_cast<uint8_t>(candidate);
    }
    return note;
}

// Nearest note of the given relative pitch class to reference; a tie goes down
// (a falling resolution sounds more final).
inline int nearestOfPitchClass(int reference, uint8_t rootPitchClass, uint8_t relativePc) {
    const int pc = (rootPitchClass + relativePc) % 12;
    const int base = reference - (((reference - pc) % 12) + 12) % 12;  // at or below
    const int up = base + 12;
    return (reference - base) <= (up - reference) ? base : up;
}

inline int foldIntoAnswerRange(int note) {
    while (note < kAnswerMinNote) note += 12;
    while (note > kAnswerMaxNote) note -= 12;
    return note;
}

struct ResponseNote {
    uint8_t step{0};      // sixteenths from the answer start (the window downbeat)
    uint8_t length{0};    // sixteenths, step + length <= windowSteps (<= 64)
    uint8_t note{0};
    uint8_t velocity{0};
};

struct ResponsePlan {
    static constexpr uint8_t kCapacity = PhraseCapture::kMaxCapacity;  // 32 (4 bars)
    uint8_t count{0};
    uint8_t windowSteps{0};   // usable steps: the whole answer window
    uint8_t droppedTail{0};   // CALL notes outside the window (guard; 0 for a captured phrase)
    ResponseNote notes[kCapacity]{};

    uint64_t onsetMask() const {
        uint64_t mask = 0;
        for (uint8_t i = 0; i < count; ++i) mask |= uint64_t{1} << (notes[i].step & 63u);
        return mask;
    }
};

// Phase 9: PARTNER LENGTH. Every answer note is played at a fraction of its
// planned length, rounded up to a whole step (never below one step). Onsets
// and pitches never change, and a shorter note cannot overlap the next one,
// so the line stays monophonic. FULL (100 %) is the phase-8 answer exactly.
enum class AnswerLength : uint8_t { Full = 0, ThreeQuarters, Half, Quarter };
constexpr uint8_t kAnswerLengthCount = 4;

constexpr uint8_t answerLengthPercent(AnswerLength length) {
    return length == AnswerLength::ThreeQuarters ? 75
         : length == AnswerLength::Half ? 50
         : length == AnswerLength::Quarter ? 25 : 100;
}

// ceil(steps * percent / 100), at least one step (percent <= 100).
constexpr uint8_t scaledAnswerNoteLength(uint8_t steps, uint8_t percent) {
    return steps * percent < 100 ? 1 : static_cast<uint8_t>((steps * percent + 99) / 100);
}

namespace detail {

inline int32_t phraseWindowSteps(const PhraseCapture::Phrase& phrase) {
    const int32_t steps = phrase.windowSteps;
    return steps > 0 && steps <= PhraseCapture::kMaxWindowSteps ? steps : PhraseCapture::kWindowSteps;
}

// Stage 1: the CALL rhythm from the answer downbeat, pitches snapped down to
// the key, one monophonic line. notes[] receives the (unshifted) pitches.
//
// A note outside the window (only possible for a phrase that breaks the
// capture contract) is dropped (cut=); if that would leave no answer at all,
// the last such note is answered on the last step (the phase-3 "one note,
// one answer" guard). Phase 9: with no delay a captured phrase always fits.
inline bool echoLine(const PhraseCapture::Phrase& phrase, const ResponseKey& key,
                     ResponsePlan& out, int (&notes)[ResponsePlan::kCapacity]) {
    out = ResponsePlan{};
    const int32_t usable = phraseWindowSteps(phrase);
    out.windowSteps = static_cast<uint8_t>(usable);
    if (phrase.count == 0) return false;

    int lastDropped = -1;
    for (uint8_t i = 0; i < phrase.count && i < ResponsePlan::kCapacity; ++i) {
        const CapturedNote& source = phrase.notes[i];
        if (source.onset >= usable) {
            ++out.droppedTail;
            lastDropped = i;
            continue;
        }
        ResponseNote& target = out.notes[out.count];
        target.step = source.onset;
        target.length = source.length;
        target.velocity = source.velocity;  // raw; placeInRange() applies the answer level
        notes[out.count] = snapDownToKey(key, source.note);
        ++out.count;
    }
    if (out.count == 0 && lastDropped >= 0 && usable > 0) {
        const CapturedNote& source = phrase.notes[lastDropped];
        out.notes[0].step = static_cast<uint8_t>(usable - 1);
        out.notes[0].length = 1;
        out.notes[0].velocity = source.velocity;
        notes[0] = snapDownToKey(key, source.note);
        out.count = 1;
        --out.droppedTail;
    }
    if (out.count == 0) return false;

    // One monophonic line: a note ends no later than the next onset and the window.
    for (uint8_t i = 0; i < out.count; ++i) {
        int32_t end = out.notes[i].step + out.notes[i].length;
        if (i + 1 < out.count && end > out.notes[i + 1].step) end = out.notes[i + 1].step;
        if (end > usable) end = usable;
        int32_t length = end - out.notes[i].step;
        if (length < 1) length = 1;
        out.notes[i].length = static_cast<uint8_t>(length);
    }
    return true;
}

// Stage 2 (the phase-1 Ending rule): relative pitch class of the answer's last
// note. Tonic after a non-tonic CALL ending, fifth after a tonic ending (the
// nearest scale tone to the fifth when the scale has none, e.g. Locrian -> b5).
inline uint8_t echoEndingRel(const ResponseKey& key, int callLast) {
    const uint8_t lastRel = static_cast<uint8_t>((callLast + 12 - key.rootPitchClass % 12) % 12);
    uint8_t endingRel = 0;
    if (lastRel == 0) {
        endingRel = 7;
        if ((key.scaleMask & (1u << 7)) == 0) {
            for (uint8_t delta = 1; delta < 6; ++delta) {
                if (key.scaleMask & (1u << (7 - delta))) { endingRel = static_cast<uint8_t>(7 - delta); break; }
                if (key.scaleMask & (1u << (7 + delta))) { endingRel = static_cast<uint8_t>(7 + delta); break; }
            }
        }
    }
    return endingRel;
}

// Stage 3: register and velocity. Keep the CALL register; move the whole line
// by octaves into the answer range when possible, then fold any note still
// outside it.
//
// Phase 6: every answer note keeps the velocity of the CALL note it comes
// from (echoLine() copied it), scaled by the answer level
// (answerVelocityFor, kAnswerVelocityPercent). The scale is a ratio, so each
// note keeps its place relative to the CALL mean - an accent stays an accent -
// and the answer mean is the level times the CALL mean. Always 1..127.
inline void placeInRange(ResponsePlan& out, const int (&notes)[ResponsePlan::kCapacity]) {
    int low = 127, high = 0;
    for (uint8_t i = 0; i < out.count; ++i) {
        if (notes[i] < low) low = notes[i];
        if (notes[i] > high) high = notes[i];
    }
    int shift = 0;
    while (high + shift > kAnswerMaxNote && low + shift - 12 >= kAnswerMinNote) shift -= 12;
    while (low + shift < kAnswerMinNote && high + shift + 12 <= kAnswerMaxNote) shift += 12;
    for (uint8_t i = 0; i < out.count; ++i) {
        out.notes[i].note = static_cast<uint8_t>(foldIntoAnswerRange(notes[i] + shift));
        out.notes[i].velocity = answerVelocityFor(out.notes[i].velocity);
    }
}

}  // namespace detail

// ECHO_VARIANT. phrase notes are sorted by onset with one note per onset (the
// capture contract). Returns false when nothing can be answered.
inline bool planEchoVariant(const PhraseCapture::Phrase& phrase,
                            const MotifDescriptor& motif,
                            const ResponseKey& key,
                            ResponsePlan& out) {
    int notes[ResponsePlan::kCapacity]{};
    if (!detail::echoLine(phrase, key, out, notes)) return false;
    const uint8_t last = out.count - 1u;
    const int callLast = notes[last];
    notes[last] = nearestOfPitchClass(callLast, key.rootPitchClass % 12u,
                                      detail::echoEndingRel(key, callLast));
    detail::placeInRange(out, notes);
    (void)motif;  // phase 6: velocity comes per note from the phrase, not the mean
    return true;
}

inline void applyAnswerLength(ResponsePlan& plan, AnswerLength length) {
    const uint8_t percent = answerLengthPercent(length);
    for (uint8_t i = 0; i < plan.count && i < ResponsePlan::kCapacity; ++i)
        plan.notes[i].length = scaledAnswerNoteLength(plan.notes[i].length, percent);
}

// "[CR-A] bar=3 n=4 cut=0 notes=0:62:77:2,4:64:77:2,..." (step:note:vel:len,
// step from the answer downbeat, len after PARTNER LENGTH). bar is the 1-based
// first bar of the ANSWER window.
inline int formatResponseLog(char* out, std::size_t capacity, uint32_t bar1Based,
                             const ResponsePlan& plan) {
    if (!out || capacity == 0) return 0;
    int written = std::snprintf(out, capacity, "[CR-A] bar=%lu n=%u cut=%u notes=",
                                static_cast<unsigned long>(bar1Based),
                                static_cast<unsigned>(plan.count),
                                static_cast<unsigned>(plan.droppedTail));
    if (written < 0) return written;
    std::size_t used = static_cast<std::size_t>(written);
    for (uint8_t i = 0; i < plan.count && used < capacity; ++i) {
        const ResponseNote& note = plan.notes[i];
        const int n = std::snprintf(out + used, capacity - used, i == 0 ? "%u:%u:%u:%u" : ",%u:%u:%u:%u",
                                    static_cast<unsigned>(note.step), static_cast<unsigned>(note.note),
                                    static_cast<unsigned>(note.velocity),
                                    static_cast<unsigned>(note.length));
        if (n < 0) break;
        used += static_cast<std::size_t>(n);
    }
    return static_cast<int>(used);
}

}  // namespace GroovePuterCallResponse

#endif  // GROOVEPUTER_CALL_RESPONSE_PLANNER_H
