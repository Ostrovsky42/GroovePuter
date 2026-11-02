#pragma once
#ifndef GROOVEPUTER_CALL_RESPONSE_HARMONY_H
#define GROOVEPUTER_CALL_RESPONSE_HARMONY_H

#include <cstddef>
#include <cstdint>
#include <cstdio>

#include "call_response_planner.h"
#include "../generation/idiom/genre_idiom.h"

// CALL/RESPONSE phase 2: the ECHO_VARIANT answer follows the harmony of the
// bars it sounds in and the genre's ending vocabulary.
//
// Harmony: one chord (absolute pitch-class mask) per strong beat of the answer
// (answer steps 0, 4, ..., 28 = 8 beats over the two answer bars). The caller
// fills it from the snapshot at the step where that beat sounds (bar 2 of the
// answer = absoluteStep + 16 and later), falls back to the current harmony and
// leaves the beat empty (phase-1 behaviour) when neither exists.
//
// Genre: the existing idiom dictionary (GenreIdiom::ideaWeight, the Idea ->
// Ending mapping of ideaPlanFor and interactionShapeForIdea) chooses one
// answer strategy by weight with a deterministic seed. The generator itself
// is not called. The pitch of the ending always obeys the phase-1 rule
// (tonic after an open CALL, fifth after a closed one); the genre chooses how
// the answer gets there: Tonic (plain resolve), RunUp, Late or Minimal.
namespace GroovePuterCallResponse {

constexpr uint8_t kNoGenre = 0xFF;
constexpr uint8_t kGenreCount = 16;    // GenerativeMode::Acid .. LoFi
constexpr uint8_t kStrongBeatSteps = 4;
constexpr uint8_t kLateEndingSteps = 2;

enum class ChordSource : uint8_t { None = 0, Ahead, Current };

// The answer chord for one beat: the lookahead snapshot, else the current
// harmony, else none (phase 1: key only).
inline uint16_t chooseAnswerChord(bool haveAhead, uint16_t aheadPcs,
                                  bool haveCurrent, uint16_t currentPcs,
                                  ChordSource* source = nullptr) {
    aheadPcs &= 0x0fffu;
    currentPcs &= 0x0fffu;
    if (haveAhead && aheadPcs != 0) {
        if (source) *source = ChordSource::Ahead;
        return aheadPcs;
    }
    if (haveCurrent && currentPcs != 0) {
        if (source) *source = ChordSource::Current;
        return currentPcs;
    }
    if (source) *source = ChordSource::None;
    return 0;
}

struct ResponseHarmony {
    static constexpr uint8_t kBeats =
        static_cast<uint8_t>(PhraseCapture::kWindowSteps / kStrongBeatSteps);
    uint16_t chordPcs[kBeats]{};  // absolute pitch classes (bit n = pc n); 0 = unknown
    uint8_t aheadMask{0};         // beats from a lookahead snapshot
    uint8_t currentMask{0};       // beats from the current-harmony fallback

    void set(uint8_t beat, uint16_t pcs, ChordSource source) {
        if (beat >= kBeats) return;
        chordPcs[beat] = static_cast<uint16_t>(pcs & 0x0fffu);
        const uint8_t bit = static_cast<uint8_t>(1u << beat);
        aheadMask = static_cast<uint8_t>(aheadMask & ~bit);
        currentMask = static_cast<uint8_t>(currentMask & ~bit);
        if (source == ChordSource::Ahead) aheadMask |= bit;
        if (source == ChordSource::Current) currentMask |= bit;
    }
    uint16_t chordAtStep(uint8_t answerStep) const {
        const uint8_t beat = static_cast<uint8_t>(answerStep / kStrongBeatSteps);
        return beat < kBeats ? chordPcs[beat] : 0;
    }
    bool any() const { return (aheadMask | currentMask) != 0; }
};

inline uint16_t absoluteKeyPcs(const ResponseKey& key) {
    uint16_t pcs = 0;
    for (uint8_t rel = 0; rel < 12; ++rel) {
        if (key.scaleMask & (1u << rel)) pcs |= 1u << ((key.rootPitchClass + rel) % 12u);
    }
    return pcs;
}

inline bool pcIn(uint16_t pcs, int note) {
    return (pcs & (1u << (((note % 12) + 12) % 12))) != 0;
}

// Nearest note whose pitch class is in pcs; a tie goes down. pcs must be non-zero.
inline int nearestInSet(int note, uint16_t pcs) {
    for (int d = 0; d < 12; ++d) {
        if (pcIn(pcs, note - d)) return note - d;
        if (pcIn(pcs, note + d)) return note + d;
    }
    return note;
}

// Next scale tone strictly below the note.
inline int scaleToneBelow(const ResponseKey& key, int note) {
    for (int candidate = note - 1; candidate > note - 13; --candidate) {
        if (candidate >= 0 && inKey(key, static_cast<uint8_t>(candidate))) return candidate;
    }
    return note;
}

// ---- Genre weights (existing idiom dictionary, no generator) -----------------

struct ResponseStyle {
    uint8_t genre{kNoGenre};
    GenreIdiom::Idea idea{GenreIdiom::Idea::Original};
    GenreIdiom::Ending ending{GenreIdiom::Ending::Tonic};
    GenreIdiom::InteractionShape shape{GenreIdiom::InteractionShape::Repeat};
    bool active() const { return genre < kGenreCount; }
};

constexpr uint8_t kEndingCount = 5;  // GenreIdiom::Ending::Tonic .. Minimal
constexpr uint8_t kResponseLiveliness = 1;  // NORMAL: the partner has no CALM/LIVELY

// Ending for an idea, as GenreIdiom::ideaPlanFor maps it. Tonic means "resolve
// by the phase-1 rule" (tonic or fifth, chosen by the CALL); the dictionary's
// Fifth (a secondary touch) is folded into it. ReducedMotif keeps a running 303
// line whole (Acid) instead of cutting it to one note.
inline GenreIdiom::Ending responseEndingForIdea(uint8_t genre, GenreIdiom::Idea idea) {
    switch (idea) {
        case GenreIdiom::Idea::DelayedAnswer: return GenreIdiom::Ending::Late;
        case GenreIdiom::Idea::Turnaround: return GenreIdiom::Ending::RunUp;
        case GenreIdiom::Idea::ReducedMotif:
            return genre == 0 ? GenreIdiom::Ending::Tonic : GenreIdiom::Ending::Minimal;
        default: return GenreIdiom::Ending::Tonic;
    }
}

// Weight per GenreIdiom::Ending value (index = enum value). All zero for an
// unknown genre (phase-1 behaviour).
inline void responseEndingWeights(uint8_t genre, uint8_t (&weights)[kEndingCount]) {
    for (uint8_t& w : weights) w = 0;
    if (genre >= kGenreCount) return;
    for (uint8_t i = 0; i < static_cast<uint8_t>(GenreIdiom::Idea::Count); ++i) {
        const auto idea = static_cast<GenreIdiom::Idea>(i);
        const uint8_t ending = static_cast<uint8_t>(responseEndingForIdea(genre, idea));
        weights[ending] = static_cast<uint8_t>(
            weights[ending] + GenreIdiom::detail::ideaWeight(genre, kResponseLiveliness, idea));
    }
}

inline uint32_t responseSeed(const PhraseCapture::Phrase& phrase) {
    uint32_t mask = 0;
    uint32_t pitches = 0;
    for (uint8_t i = 0; i < phrase.count; ++i) {
        mask |= 1u << (phrase.notes[i].onset & 31u);
        pitches = pitches * 31u + phrase.notes[i].note;
    }
    return GenreIdiom::mix(mask * 0x9E3779B1u + pitches * 0x85EBCA77u +
                           static_cast<uint32_t>(phrase.firstBar()));
}

// One idea by weight; deterministic for (genre, seed).
inline ResponseStyle pickResponseStyle(uint8_t genre, uint32_t seed) {
    ResponseStyle style{};
    if (genre >= kGenreCount) return style;
    style.genre = genre;
    uint32_t total = 0;
    const uint8_t ideas = static_cast<uint8_t>(GenreIdiom::Idea::Count);
    for (uint8_t i = 0; i < ideas; ++i) {
        total += GenreIdiom::detail::ideaWeight(genre, kResponseLiveliness,
                                               static_cast<GenreIdiom::Idea>(i));
    }
    uint32_t pick = GenreIdiom::mix(seed + 0x27D4EB2Fu) % (total ? total : 1u);
    uint8_t chosen = 0;
    for (; chosen + 1 < ideas; ++chosen) {
        const uint8_t w = GenreIdiom::detail::ideaWeight(genre, kResponseLiveliness,
                                                         static_cast<GenreIdiom::Idea>(chosen));
        if (pick < w) break;
        pick -= w;
    }
    style.idea = static_cast<GenreIdiom::Idea>(chosen);
    style.ending = responseEndingForIdea(genre, style.idea);
    style.shape = GenreIdiom::interactionShapeForIdea(style.idea);
    return style;
}

inline const char* responseEndingName(GenreIdiom::Ending ending) {
    switch (ending) {
        case GenreIdiom::Ending::Tonic: return "RESOLVE";
        case GenreIdiom::Ending::Fifth: return "FIFTH";
        case GenreIdiom::Ending::RunUp: return "RUNUP";
        case GenreIdiom::Ending::Late: return "LATE";
        case GenreIdiom::Ending::Minimal: return "MINIMAL";
    }
    return "?";
}

// ---- Planner -----------------------------------------------------------------

struct ResponseHarmonyReport {
    uint8_t strongNotes{0};     // answer notes on a strong beat with a known chord
    uint8_t strongChordTones{0};
    uint8_t movedToChord{0};    // notes moved to a chord tone
    bool endingChordTone{false};
    bool endingHasChord{false};
};

// Phase 2 ECHO_VARIANT. With an empty harmony and an inactive style the result
// equals planEchoVariant() exactly.
inline bool planHarmonicEcho(const PhraseCapture::Phrase& phrase,
                             const MotifDescriptor& motif, const ResponseKey& key,
                             const ResponseHarmony& harmony, const ResponseStyle& style,
                             ResponsePlan& out, ResponseHarmonyReport* report = nullptr,
                             uint8_t delaySteps = kAnswerDelaySteps) {
    if (report) *report = ResponseHarmonyReport{};
    int notes[ResponsePlan::kCapacity]{};
    if (!detail::echoLine(phrase, key, out, notes, delaySteps)) return false;
    const int32_t usable = out.windowSteps;
    uint8_t last = out.count - 1u;
    const int callLast = notes[last];
    const uint8_t endingRel = detail::echoEndingRel(key, callLast);

    // Genre shape (rhythm only; the ending pitch rule is untouched).
    if (style.active() && style.ending == GenreIdiom::Ending::Minimal) {
        // ShortCall: the second answer bar keeps only its strong beats and the ending.
        uint8_t kept = 0;
        for (uint8_t i = 0; i < out.count; ++i) {
            const bool drop = i != last && out.notes[i].step >= PhraseCapture::kStepsPerBar &&
                              (out.notes[i].step % kStrongBeatSteps) != 0;
            if (drop) continue;
            out.notes[kept] = out.notes[i];
            notes[kept] = notes[i];
            ++kept;
        }
        out.count = kept;
        last = out.count - 1u;
    } else if (style.active() && style.ending == GenreIdiom::Ending::Late) {
        // DelayedResponse: a short silence, then the ending late.
        ResponseNote& end = out.notes[last];
        const int32_t lateStep = end.step + kLateEndingSteps;
        if (lateStep < usable) {
            int32_t length = end.length;
            if (lateStep + length > usable) length = usable - lateStep;
            if (length < 1) length = 1;
            end.step = static_cast<uint8_t>(lateStep);
            end.length = static_cast<uint8_t>(length);
        }
    }

    // Strong beats on chord tones (in the key, so the line never leaves it).
    const uint16_t keyPcs = absoluteKeyPcs(key);
    for (uint8_t i = 0; i < last; ++i) {
        if ((out.notes[i].step % kStrongBeatSteps) != 0) continue;
        const uint16_t allowed = static_cast<uint16_t>(harmony.chordAtStep(out.notes[i].step) & keyPcs);
        if (allowed == 0 || pcIn(allowed, notes[i])) continue;
        notes[i] = nearestInSet(notes[i], allowed);
        if (report) ++report->movedToChord;
    }

    // Ending: the phase-1 rule, nearest to the CALL's last note.
    notes[last] = nearestOfPitchClass(callLast, key.rootPitchClass % 12u, endingRel);

    // RunUp: approach the ending by step from below (only off the strong beats,
    // which belong to the chord).
    if (style.active() && style.ending == GenreIdiom::Ending::RunUp && last > 0 &&
        (out.notes[last - 1].step % kStrongBeatSteps) != 0) {
        notes[last - 1] = scaleToneBelow(key, notes[last]);
    }

    detail::placeInRange(out, notes, answerVelocityFor(motif.averageVelocity));

    if (report) {
        for (uint8_t i = 0; i < out.count; ++i) {
            const uint16_t chord = harmony.chordAtStep(out.notes[i].step);
            if (i == last) {
                report->endingHasChord = chord != 0;
                report->endingChordTone = chord != 0 && pcIn(chord, out.notes[i].note);
            }
            if ((out.notes[i].step % kStrongBeatSteps) != 0 || chord == 0) continue;
            ++report->strongNotes;
            if (pcIn(chord, out.notes[i].note)) ++report->strongChordTones;
        }
    }
    return true;
}

// " chord=A2C6 end=RESOLVE ct=3/4 endct=1" appended to the [CR-A] line.
inline int formatHarmonyLog(char* out, std::size_t capacity, const ResponseHarmony& harmony,
                            const ResponseStyle& style, const ResponseHarmonyReport& report) {
    if (!out || capacity == 0) return 0;
    return std::snprintf(out, capacity, " chord=A%uC%u end=%s ct=%u/%u endct=%u",
                         static_cast<unsigned>(__builtin_popcount(harmony.aheadMask)),
                         static_cast<unsigned>(__builtin_popcount(harmony.currentMask)),
                         style.active() ? responseEndingName(style.ending) : "P1",
                         static_cast<unsigned>(report.strongChordTones),
                         static_cast<unsigned>(report.strongNotes),
                         report.endingHasChord ? (report.endingChordTone ? 1u : 0u) : 2u);
}

}  // namespace GroovePuterCallResponse

#endif  // GROOVEPUTER_CALL_RESPONSE_HARMONY_H
