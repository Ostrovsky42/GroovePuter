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
// Timing: the ANSWER window is the two bars right after the CALL window. The
// answer starts kAnswerDelaySteps after that boundary, so a person who goes on
// playing on the downbeat (up to half a step late still quantizes to onset 0)
// is heard before the first answer note sounds.
namespace GroovePuterCallResponse {

constexpr uint8_t kAnswerDelaySteps = 1;     // default (PARTNER DELAY)
constexpr uint8_t kAnswerMaxDelaySteps = 1;  // PARTNER DELAY offers 0 or 1

inline uint8_t clampAnswerDelay(uint8_t delaySteps) {
    return delaySteps > kAnswerMaxDelaySteps ? kAnswerMaxDelaySteps : delaySteps;
}
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
    uint8_t step{0};      // sixteenths from the answer start (window start + delay)
    uint8_t length{0};    // sixteenths, step + length <= windowSteps
    uint8_t note{0};
    uint8_t velocity{0};
};

struct ResponsePlan {
    static constexpr uint8_t kCapacity = PhraseCapture::kCapacity;
    uint8_t count{0};
    uint8_t delaySteps{kAnswerDelaySteps};
    uint8_t windowSteps{0};   // usable steps after the delay
    uint8_t droppedTail{0};   // CALL notes that do not fit after the delay
    ResponseNote notes[kCapacity]{};

    uint32_t onsetMask() const {
        uint32_t mask = 0;
        for (uint8_t i = 0; i < count; ++i) mask |= 1u << notes[i].step;
        return mask;
    }
};

namespace detail {

inline int32_t phraseWindowSteps(const PhraseCapture::Phrase& phrase) {
    const int32_t steps = phrase.windowSteps;
    return steps > 0 && steps <= PhraseCapture::kWindowSteps ? steps : PhraseCapture::kWindowSteps;
}

// Stage 1: the CALL rhythm after the delay, pitches snapped down to the key,
// one monophonic line. notes[] receives the (unshifted) pitches.
//
// Phase 3 fix ("one note, one answer"): a CALL note on the last step(s) that
// the delay pushes past the window is dropped (cut=), as in phase 1 - except
// when that would leave NO answer at all. That is exactly a one-note CALL on
// the last sixteenth, i.e. a downbeat played 0.3..1.5 steps early (the pickup
// rule only takes the last 30 % of the step). Phase 1 silently answered
// nothing there; now the note is answered on the last usable step, which is
// within one step of where a pickup would have put it.
inline bool echoLine(const PhraseCapture::Phrase& phrase, const ResponseKey& key,
                     ResponsePlan& out, int (&notes)[ResponsePlan::kCapacity],
                     uint8_t delaySteps = kAnswerDelaySteps) {
    out = ResponsePlan{};
    delaySteps = clampAnswerDelay(delaySteps);
    const int32_t usable = phraseWindowSteps(phrase) - delaySteps;
    out.delaySteps = delaySteps;
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
        notes[out.count] = snapDownToKey(key, source.note);
        ++out.count;
    }
    if (out.count == 0 && lastDropped >= 0 && usable > 0) {
        const CapturedNote& source = phrase.notes[lastDropped];
        out.notes[0].step = static_cast<uint8_t>(usable - 1);
        out.notes[0].length = 1;
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
inline void placeInRange(ResponsePlan& out, const int (&notes)[ResponsePlan::kCapacity],
                         uint8_t velocity) {
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
        out.notes[i].velocity = velocity;
    }
}

}  // namespace detail

// ECHO_VARIANT. phrase notes are sorted by onset with one note per onset (the
// capture contract). Returns false when nothing can be answered.
inline bool planEchoVariant(const PhraseCapture::Phrase& phrase,
                            const MotifDescriptor& motif,
                            const ResponseKey& key,
                            ResponsePlan& out,
                            uint8_t delaySteps = kAnswerDelaySteps) {
    int notes[ResponsePlan::kCapacity]{};
    if (!detail::echoLine(phrase, key, out, notes, delaySteps)) return false;
    const uint8_t last = out.count - 1u;
    const int callLast = notes[last];
    notes[last] = nearestOfPitchClass(callLast, key.rootPitchClass % 12u,
                                      detail::echoEndingRel(key, callLast));
    detail::placeInRange(out, notes, answerVelocityFor(motif.averageVelocity));
    return true;
}

// "[CR-A] bar=3 at=+1 n=4 notes=0:62:77:2,4:64:77:2,..." (step:note:vel:len,
// step from the answer start). bar is the 1-based first bar of the ANSWER window.
inline int formatResponseLog(char* out, std::size_t capacity, uint32_t bar1Based,
                             const ResponsePlan& plan) {
    if (!out || capacity == 0) return 0;
    int written = std::snprintf(out, capacity, "[CR-A] bar=%lu at=+%u n=%u cut=%u notes=",
                                static_cast<unsigned long>(bar1Based),
                                static_cast<unsigned>(plan.delaySteps),
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
