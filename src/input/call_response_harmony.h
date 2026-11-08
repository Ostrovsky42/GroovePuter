#pragma once
#ifndef GROOVEPUTER_CALL_RESPONSE_HARMONY_H
#define GROOVEPUTER_CALL_RESPONSE_HARMONY_H

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <initializer_list>

#include "call_response_planner.h"
#include "../generation/idiom/genre_idiom.h"

// CALL/RESPONSE phase 2: the ECHO_VARIANT answer follows the harmony of the
// bars it sounds in and the genre's ending vocabulary.
//
// Harmony: one chord (absolute pitch-class mask) per strong beat of the answer
// (answer steps 0, 4, ..., 28 = 8 beats over the two answer bars; phase 4:
// up to 16 beats, steps 0..60, for a 4-bar answer). The caller
// fills it from the snapshot at the step where that beat sounds (bar 2 of the
// answer = absoluteStep + 16 and later), falls back to the current harmony and
// leaves the beat empty (phase-1 behaviour) when neither exists.
//
// Genre: the existing idiom dictionary (GenreIdiom::ideaWeight, the Idea ->
// Ending mapping of ideaPlanFor and interactionShapeForIdea) chooses one
// answer strategy by weight with a deterministic seed. The generator itself
// is not called. The pitch of the ending obeys the phase-1 rule (tonic after
// an open CALL, fifth after a closed one) - or, since phase 6, the half/full
// cadence over the ending's chord (AnswerCadence); the genre chooses how the
// answer gets there: Tonic (plain resolve), RunUp, Late or Minimal.
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
    // Beats of the default 2-bar answer (the 1- and 2-bar lookahead span).
    static constexpr uint8_t kDefaultBeats =
        static_cast<uint8_t>(PhraseCapture::kWindowSteps / kStrongBeatSteps);
    static constexpr uint8_t kBeats =
        static_cast<uint8_t>(PhraseCapture::kMaxWindowSteps / kStrongBeatSteps);
    // Bits 0..11: absolute pitch classes (bit n = pc n); 0 = unknown.
    // Bits 12..15 (phase 8): chord root pc + 1; 0 = root unknown. The root
    // rides in the unused top nibble, so the struct (loop stack) does not grow.
    uint16_t chordPcs[kBeats]{};
    uint16_t aheadMask{0};        // beats from a lookahead snapshot
    uint16_t currentMask{0};      // beats from the current-harmony fallback

    void set(uint8_t beat, uint16_t pcs, ChordSource source, uint8_t rootPc = 0xFF) {
        if (beat >= kBeats) return;
        pcs = static_cast<uint16_t>(pcs & 0x0fffu);
        const uint16_t root = pcs != 0 && rootPc < 12 && (pcs & (1u << rootPc))
            ? static_cast<uint16_t>((rootPc + 1u) << 12) : 0u;
        chordPcs[beat] = static_cast<uint16_t>(pcs | root);
        const uint16_t bit = static_cast<uint16_t>(1u << beat);
        aheadMask = static_cast<uint16_t>(aheadMask & ~bit);
        currentMask = static_cast<uint16_t>(currentMask & ~bit);
        if (source == ChordSource::Ahead) aheadMask |= bit;
        if (source == ChordSource::Current) currentMask |= bit;
    }
    uint16_t chordAtStep(uint8_t answerStep) const {
        const uint8_t beat = static_cast<uint8_t>(answerStep / kStrongBeatSteps);
        return beat < kBeats ? static_cast<uint16_t>(chordPcs[beat] & 0x0fffu) : 0;
    }
    // Phase 8: root pc of the chord at that step; 0xFF = unknown.
    uint8_t rootAtStep(uint8_t answerStep) const {
        const uint8_t beat = static_cast<uint8_t>(answerStep / kStrongBeatSteps);
        if (beat >= kBeats || (chordPcs[beat] >> 12) == 0) return 0xFF;
        return static_cast<uint8_t>((chordPcs[beat] >> 12) - 1u);
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

// Phase 4: onsets of bars 3-4 fold onto bars 1-2 in the mask (& 31) so that
// the 1- and 2-bar seeds (and with them the genre ending) stay unchanged.
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

// ---- Ending: chord and place in the form (phase 6) ---------------------------
//
// ByCall is the phase-1 rule (tonic after a CALL that ends away from the
// tonic, fifth after one that ends on it). Half / Full are the form-aware
// endings of phase 6, chosen by the answer number: the 1st, 3rd, ... answer is
// a half cadence (Half), the 2nd, 4th, ... a full one (Full). They apply only
// when the chord under the ending is known and shares a tone with the key;
// otherwise the ending stays ByCall (nothing to clash with, phase-1 result).
enum class AnswerCadence : uint8_t { ByCall = 0, Half, Full };

// 0-based answer index -> cadence: index 0 (the first answer) is Half.
inline AnswerCadence answerCadenceFor(uint16_t answerIndex) {
    return (answerIndex & 1u) ? AnswerCadence::Full : AnswerCadence::Half;
}

// The ending note (unfolded; placeInRange folds by octaves, the pitch class
// stays). chord = absolute pitch classes under the ending, 0 = unknown.
//   Full: the tonic nearest the CALL's last note; when the chord has no tonic,
//         the chord tone in the key nearest to that tonic (tie goes down).
//   Half: the key's fifth (b5 etc. when the scale has none) when the chord has
//         it, else the nearest chord tone in the key other than the tonic, so
//         the half cadence never lands on the tonic over V (tonic only when it
//         is the chord's sole tone in the key).
//
// Phase 8: the genre colours the ending (ending colour table below) when the
// chord's root is known. The colour never overrides the form: the full
// cadence still lands on the tonic whenever the chord has it, and the half
// cadence never lands on the tonic. Colour tones are relative to the chord
// root and must be in the chord and in the key.
//   Plain    (Acid, Techno): root or fifth of the chord, no extensions. Half:
//            the key's fifth if the chord has it, else the chord's root/fifth
//            nearest to it; Full without the tonic: the chord's root/fifth
//            nearest to the tonic (over V in C: D, not the leading tone B).
//   Extended (House, LoFi, Dub = Reggae): the chord's 7th (b7/7) or 9th
//            preferred when the chord has one (Half: nearest to the key's
//            fifth; Full without the tonic: root/fifth/7th/9th nearest the
//            tonic); a plain triad ends as before.
//   Blue     (UK garage): the chord's b3 or b7 preferred when the chord has
//            one (Half: nearest to the key's fifth; Full without the tonic:
//            b3/b7 nearest the tonic); otherwise as before.
//   Default  (every other genre, no genre, unknown root): phase 6 unchanged.
enum class EndingColor : uint8_t { Default = 0, Plain, Extended, Blue };

// GenerativeMode index -> colour. Constant table (flash), 0 bytes DRAM.
inline EndingColor endingColorForGenre(uint8_t genre) {
    static constexpr EndingColor kColors[kGenreCount] = {
        EndingColor::Plain,     // 0 Acid
        EndingColor::Default,   // 1 Outrun
        EndingColor::Default,   // 2 Darksynth
        EndingColor::Default,   // 3 Electro
        EndingColor::Default,   // 4 Rave
        EndingColor::Extended,  // 5 Reggae (dub)
        EndingColor::Default,   // 6 TripHop
        EndingColor::Default,   // 7 Broken
        EndingColor::Default,   // 8 Chip
        EndingColor::Extended,  // 9 House
        EndingColor::Plain,     // 10 Techno
        EndingColor::Default,   // 11 HipHop
        EndingColor::Default,   // 12 FunkSoul
        EndingColor::Blue,      // 13 UK garage
        EndingColor::Default,   // 14 DnB
        EndingColor::Extended,  // 15 LoFi
    };
    return genre < kGenreCount ? kColors[genre] : EndingColor::Default;
}

namespace detail {

// Chord tones (absolute pcs) at the given intervals above the chord root.
inline uint16_t chordTonesAt(uint16_t chord, uint8_t chordRoot, std::initializer_list<uint8_t> intervals) {
    uint16_t out = 0;
    for (uint8_t interval : intervals) {
        const uint8_t pc = static_cast<uint8_t>((chordRoot + interval) % 12u);
        if (chord & (1u << pc)) out = static_cast<uint16_t>(out | (1u << pc));
    }
    return out;
}

}  // namespace detail

inline int cadenceEndingNote(const ResponseKey& key, int callLast, uint16_t chord,
                             AnswerCadence cadence, AnswerCadence* applied = nullptr,
                             uint8_t chordRoot = 0xFF, uint8_t genre = kNoGenre) {
    const uint8_t root = static_cast<uint8_t>(key.rootPitchClass % 12u);
    const uint16_t allowed = static_cast<uint16_t>(chord & 0x0fffu & absoluteKeyPcs(key));
    if (cadence == AnswerCadence::ByCall || allowed == 0) {
        if (applied) *applied = AnswerCadence::ByCall;
        return nearestOfPitchClass(callLast, root, detail::echoEndingRel(key, callLast));
    }
    if (applied) *applied = cadence;
    const EndingColor color = chordRoot < 12 && (chord & (1u << chordRoot))
        ? endingColorForGenre(genre) : EndingColor::Default;
    // Colour tones of this chord, in the key (empty for Default).
    uint16_t colour = 0;   // preferred colour tones
    uint16_t plain = 0;    // root / fifth of the chord
    if (color != EndingColor::Default) {
        plain = static_cast<uint16_t>(detail::chordTonesAt(chord, chordRoot, {0, 6, 7}) & allowed);
        if (color == EndingColor::Extended)
            colour = static_cast<uint16_t>(detail::chordTonesAt(chord, chordRoot, {10, 11, 2}) & allowed);
        else if (color == EndingColor::Blue)
            colour = static_cast<uint16_t>(detail::chordTonesAt(chord, chordRoot, {3, 10}) & allowed);
    }
    if (cadence == AnswerCadence::Full) {
        const int tonic = nearestOfPitchClass(callLast, root, 0);
        if (pcIn(allowed, tonic)) return tonic;
        const uint16_t set = color == EndingColor::Extended ? static_cast<uint16_t>(plain | colour)
                           : color == EndingColor::Blue ? (colour ? colour : plain)
                           : plain;
        return nearestInSet(tonic, set ? set : allowed);
    }
    const uint16_t noTonic = static_cast<uint16_t>(~(1u << root));
    const int fifth = nearestOfPitchClass(callLast, root, detail::echoEndingRel(key, root));
    if ((color == EndingColor::Extended || color == EndingColor::Blue) && (colour & noTonic))
        return nearestInSet(fifth, static_cast<uint16_t>(colour & noTonic));
    // Plain: the key's fifth only when it is not the chord's 7th/9th.
    const uint16_t fifthOk = color == EndingColor::Plain
        ? static_cast<uint16_t>(allowed & ~detail::chordTonesAt(chord, chordRoot, {2, 10, 11}))
        : allowed;
    if (pcIn(fifthOk, fifth)) return fifth;
    if (plain & noTonic) return nearestInSet(fifth, static_cast<uint16_t>(plain & noTonic));
    const uint16_t open = static_cast<uint16_t>(allowed & noTonic);
    return nearestInSet(fifth, open ? open : allowed);
}

// ---- Planner -----------------------------------------------------------------

struct ResponseHarmonyReport {
    uint8_t strongNotes{0};     // answer notes on a strong beat with a known chord
    uint8_t strongChordTones{0};
    uint8_t movedToChord{0};    // notes moved to a chord tone
    bool endingChordTone{false};
    bool endingHasChord{false};
    AnswerCadence cadence{AnswerCadence::ByCall};  // the ending rule actually applied
};

// Phase 2 ECHO_VARIANT. With an empty harmony and an inactive style the result
// equals planEchoVariant() exactly (any cadence: Half/Full need a chord).
inline bool planHarmonicEcho(const PhraseCapture::Phrase& phrase,
                             const MotifDescriptor& motif, const ResponseKey& key,
                             const ResponseHarmony& harmony, const ResponseStyle& style,
                             ResponsePlan& out, ResponseHarmonyReport* report = nullptr,
                             AnswerCadence cadence = AnswerCadence::ByCall) {
    (void)motif;  // phase 6: velocity comes per note from the phrase
    if (report) *report = ResponseHarmonyReport{};
    int notes[ResponsePlan::kCapacity]{};
    if (!detail::echoLine(phrase, key, out, notes)) return false;
    const int32_t usable = out.windowSteps;
    uint8_t last = out.count - 1u;
    const int callLast = notes[last];

    // Genre shape (rhythm only; the ending pitch rule is untouched).
    if (style.active() && style.ending == GenreIdiom::Ending::Minimal) {
        // ShortCall: the second answer bar (and bars 3-4 of a 4-bar answer)
        // keeps only its strong beats and the ending.
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

    // Ending: nearest to the CALL's last note; the phase-1 rule, or (phase 6)
    // the half/full cadence over the chord under the (possibly late) ending.
    AnswerCadence applied = AnswerCadence::ByCall;
    notes[last] = cadenceEndingNote(key, callLast, harmony.chordAtStep(out.notes[last].step),
                                    cadence, &applied, harmony.rootAtStep(out.notes[last].step),
                                    style.active() ? style.genre : kNoGenre);

    // RunUp: approach the ending by step from below (only off the strong beats,
    // which belong to the chord).
    if (style.active() && style.ending == GenreIdiom::Ending::RunUp && last > 0 &&
        (out.notes[last - 1].step % kStrongBeatSteps) != 0) {
        notes[last - 1] = scaleToneBelow(key, notes[last]);
    }

    detail::placeInRange(out, notes);

    if (report) {
        report->cadence = applied;
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

// " chord=A2C6 end=RESOLVE ct=3/4 endct=1 fin=H" appended to the [CR-A] line.
// fin: C = phase-1 rule (by the CALL), H = half cadence, F = full (phase 6).
inline int formatHarmonyLog(char* out, std::size_t capacity, const ResponseHarmony& harmony,
                            const ResponseStyle& style, const ResponseHarmonyReport& report) {
    if (!out || capacity == 0) return 0;
    return std::snprintf(out, capacity, " chord=A%uC%u end=%s ct=%u/%u endct=%u fin=%c",
                         static_cast<unsigned>(__builtin_popcount(harmony.aheadMask)),
                         static_cast<unsigned>(__builtin_popcount(harmony.currentMask)),
                         style.active() ? responseEndingName(style.ending) : "P1",
                         static_cast<unsigned>(report.strongChordTones),
                         static_cast<unsigned>(report.strongNotes),
                         report.endingHasChord ? (report.endingChordTone ? 1u : 0u) : 2u,
                         report.cadence == AnswerCadence::Half ? 'H'
                             : report.cadence == AnswerCadence::Full ? 'F' : 'C');
}

}  // namespace GroovePuterCallResponse

#endif  // GROOVEPUTER_CALL_RESPONSE_HARMONY_H
