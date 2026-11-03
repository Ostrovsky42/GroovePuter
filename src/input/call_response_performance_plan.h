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

// Every variation starts from the captured original; no cumulative mutation.
// cadence (phase 6): Half/Full by the answer number (answerCadenceFor); LOOP
// EVOLVE 0 ignores it (exact repeat of the original pitches and velocities).
inline bool planPerformanceReply(const PhraseCapture::Phrase& original,
    const MotifDescriptor& motif, const ResponseKey& key,
    const ResponseHarmony& harmony, const ResponseStyle& style,
    const ReplyMemory& memory, bool loop, bool duet, uint8_t evolve,
    uint8_t delay, ResponsePlan& plan, ReplyStrategy& actual,
    ResponseHarmonyReport& report, AnswerCadence cadence = AnswerCadence::ByCall) {
    report = ResponseHarmonyReport{};
    actual = ReplyStrategy::Echo;
    if (loop && evolve == 0) {
        int pitches[ResponsePlan::kCapacity]{};
        // Reuse timing/tail rules but restore original pitches and velocities.
        if (!detail::echoLine(original, key, plan, pitches, delay)) return false;
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
    } else if (!loop || evolve >= 2) {
        actual = chooseReplyStrategy(original, motif, memory);
        // Phase 7: SHORT REPLY keeps the first half of the notes and leaves
        // the rest of the window silent - in LOOP that was a hole of half the
        // loop (a whole bar of 2) on every third pass at EVOLVE 2/3, and on
        // the first pass of a dense loop. LOOP varies with ECHO <-> DEVELOP.
        if (loop && actual == ReplyStrategy::ShortReply) actual = ReplyStrategy::Echo;
    }
    PhraseCapture::Phrase phrase{};
    if (!transformCall(original, actual, delay, phrase)) actual = ReplyStrategy::Echo;
    if (loop && evolve >= 3) {
        const int usable = phrase.windowSteps - clampAnswerDelay(delay);
        for (uint8_t i = 3; i < phrase.count; i += 2) {
            const int shifted = phrase.notes[i].onset + 1;
            if (shifted >= usable || (i + 1 < phrase.count && shifted >= phrase.notes[i + 1].onset)) continue;
            phrase.notes[i].onset = static_cast<uint8_t>(shifted);
        }
    }
    const ResponseStyle appliedStyle = performanceReplyStyle(style, loop, evolve);
    return planHarmonicEcho(phrase, motif, key, harmony, appliedStyle, plan, &report, delay, cadence);
}
} // namespace GroovePuterCallResponse
