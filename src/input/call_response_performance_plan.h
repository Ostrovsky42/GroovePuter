#pragma once
#include "call_response_development.h"
#include "call_response_harmony.h"

namespace GroovePuterCallResponse {
// LOOP keeps the loop's rhythm: the genre endings that move or drop notes
// (LATE: the last note 2 steps later - a one-note loop lost its downbeat;
// MINIMAL: off-beat notes of bar 2+ dropped) become a plain TONIC ending.
// Phase 7: for every EVOLVE (EVOLVE 2 used to keep them). evolve stays in the
// signature for the callers; EVOLVE 0 never reaches the genre shaping.
inline ResponseStyle performanceReplyStyle(ResponseStyle style, bool loop, uint8_t evolve) {
    (void)evolve;
    if (loop &&
        (style.ending == GenreIdiom::Ending::Late || style.ending == GenreIdiom::Ending::Minimal))
        style.ending = GenreIdiom::Ending::Tonic;
    return style;
}

// ---- Phase 8: LOOP passes ----------------------------------------------------
//
// Before: the loop repeated an A A' pair - EVOLVE 2/3 alternated DEVELOP and
// ECHO with period 2 and the cadence alternated Half/Full with period 2, in
// phase, so DEVELOP always ended open and ECHO always closed: one 2-pass
// pattern forever.
//
// Now (EVOLVE >= 1; EVOLVE 0 stays an exact repeat):
//  - form over 4 passes (loopCycle % 4): 0 the loop, 1 the loop again with the
//    other cadence, 2 the motif one scale degree higher (transposed before the
//    harmony is applied, so its strong beats still land on the chord of their
//    beat), 3 back to the loop with the full cadence - a period a a' b a'.
//    The cadence itself stays the phase-6 parity (H F H F), so pass 0/2 are
//    half cadences and 1/3 full ones;
//  - EVOLVE 2/3: the rhythm strategy follows loopCycle % 3 (ECHO, DEVELOP,
//    ECHO) instead of the reply memory, so DEVELOP meets both cadences
//    (strategy x cadence period 6; with the form, 12 passes);
//  - EVOLVE 3: on passes 1 and 2 a seeded variation (seed = CALL signature +
//    loopCycle): weak-beat notes may move one scale step, weak-beat
//    velocities move by up to +-8. The first note, the strong beats and the
//    ending are never touched.
inline bool loopPassTransposed(uint16_t loopCycle) { return (loopCycle & 3u) == 2u; }
inline bool loopPassVaried(uint16_t loopCycle) {
    return (loopCycle & 3u) == 1u || (loopCycle & 3u) == 2u;
}
inline ReplyStrategy loopStrategyFor(uint16_t loopCycle) {
    return loopCycle % 3u == 1u ? ReplyStrategy::Develop : ReplyStrategy::Echo;
}

namespace detail {

// Next scale tone strictly above the note (the note itself when none in an octave).
inline int scaleToneAbove(const ResponseKey& key, int note) {
    for (int candidate = note + 1; candidate < note + 13 && candidate <= 127; ++candidate)
        if (inKey(key, static_cast<uint8_t>(candidate))) return candidate;
    return note;
}

// The motif one scale degree higher (each note snapped to the key, then the
// next scale tone up). Rhythm and velocity untouched.
inline void transposeUpOneDegree(PhraseCapture::Phrase& phrase, const ResponseKey& key) {
    for (uint8_t i = 0; i < phrase.count && i < PhraseCapture::kMaxCapacity; ++i) {
        const int up = scaleToneAbove(key, snapDownToKey(key, phrase.notes[i].note));
        if (up >= 0 && up <= 127) phrase.notes[i].note = static_cast<uint8_t>(up);
    }
}

// EVOLVE 3 weak-beat variation (see above). Deterministic for (seed, plan).
inline void varyWeakBeats(ResponsePlan& plan, const ResponseKey& key, uint32_t seed) {
    if (plan.count < 3) return;
    for (uint8_t i = 1; i + 1 < plan.count; ++i) {
        ResponseNote& note = plan.notes[i];
        if ((note.step % kStrongBeatSteps) == 0) continue;
        const uint32_t r = GenreIdiom::mix(seed + i * 0x9E3779B1u);
        if ((r & 3u) == 0u) {  // one note in four: a scale step up or down
            const bool up = (r & 4u) != 0;
            int moved = up ? scaleToneAbove(key, note.note) : scaleToneBelow(key, note.note);
            if (moved < kAnswerMinNote || moved > kAnswerMaxNote)
                moved = up ? scaleToneBelow(key, note.note) : scaleToneAbove(key, note.note);
            if (moved >= kAnswerMinNote && moved <= kAnswerMaxNote)
                note.note = static_cast<uint8_t>(moved);
        }
        int velocity = static_cast<int>(note.velocity) + static_cast<int>((r >> 8) % 17u) - 8;
        if (velocity < 1) velocity = 1;
        if (velocity > 127) velocity = 127;
        note.velocity = static_cast<uint8_t>(velocity);
    }
}

}  // namespace detail

// Every variation starts from the captured original; no cumulative mutation.
// cadence (phase 6): Half/Full by the answer number (answerCadenceFor); LOOP
// EVOLVE 0 ignores it (exact repeat of the original pitches and velocities).
inline bool planPerformanceReply(const PhraseCapture::Phrase& original,
    const MotifDescriptor& motif, const ResponseKey& key,
    const ResponseHarmony& harmony, const ResponseStyle& style,
    const ReplyMemory& memory, bool loop, bool duet, uint8_t evolve,
    ResponsePlan& plan, ReplyStrategy& actual,
    ResponseHarmonyReport& report, AnswerCadence cadence = AnswerCadence::ByCall,
    uint16_t loopCycle = 0) {
    report = ResponseHarmonyReport{};
    actual = ReplyStrategy::Echo;
    if (loop && evolve == 0) {
        int pitches[ResponsePlan::kCapacity]{};
        // Reuse timing/tail rules but restore original pitches and velocities.
        if (!detail::echoLine(original, key, plan, pitches)) return false;
        for (uint8_t i = 0; i < plan.count; ++i) {
            uint8_t source = 0;
            while (source + 1 < original.count && original.notes[source].onset < plan.notes[i].step) ++source;
            plan.notes[i].note = static_cast<uint8_t>(foldIntoAnswerRange(original.notes[source].note));
            plan.notes[i].velocity = original.notes[source].velocity ? original.notes[source].velocity : 1;
        }
        return true;
    }
    if (duet) {
        if (original.count * 16u >= original.windowSteps * 8u) actual = ReplyStrategy::ShortReply;
    } else if (!loop) {
        actual = chooseReplyStrategy(original, motif, memory);
    } else if (evolve >= 2) {
        // Phase 7: no SHORT REPLY in LOOP (it left half the loop silent).
        // Phase 8: ECHO / DEVELOP by the pass (loopCycle % 3), not by the
        // reply memory, so the strategy and the cadence (% 2) drift apart.
        actual = loopStrategyFor(loopCycle);
    }
    PhraseCapture::Phrase phrase{};
    if (!transformCall(original, actual, phrase)) actual = ReplyStrategy::Echo;
    if (loop && evolve >= 3) {
        const int usable = phrase.windowSteps;
        for (uint8_t i = 3; i < phrase.count; i += 2) {
            const int shifted = phrase.notes[i].onset + 1;
            if (shifted >= usable || (i + 1 < phrase.count && shifted >= phrase.notes[i + 1].onset)) continue;
            phrase.notes[i].onset = static_cast<uint8_t>(shifted);
        }
    }
    // Phase 8: pass 2 of the 4-pass form - the motif a scale degree higher,
    // before the harmony, so planHarmonicEcho puts its strong beats on the
    // chords and gives it the cadence of its pass.
    if (loop && loopPassTransposed(loopCycle)) detail::transposeUpOneDegree(phrase, key);
    const ResponseStyle appliedStyle = performanceReplyStyle(style, loop, evolve);
    if (!planHarmonicEcho(phrase, motif, key, harmony, appliedStyle, plan, &report, cadence))
        return false;
    if (loop && evolve >= 3 && loopPassVaried(loopCycle))
        detail::varyWeakBeats(plan, key, callSignature(original, motif) + loopCycle * 0x85EBCA77u);
    return true;
}
} // namespace GroovePuterCallResponse
