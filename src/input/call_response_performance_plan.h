#pragma once
#include "call_response_development.h"
#include "call_response_harmony.h"

namespace GroovePuterCallResponse {
inline ResponseStyle performanceReplyStyle(ResponseStyle style, bool loop, uint8_t evolve) {
    if (loop && (evolve == 1 || evolve >= 3) &&
        (style.ending == GenreIdiom::Ending::Late || style.ending == GenreIdiom::Ending::Minimal))
        style.ending = GenreIdiom::Ending::Tonic;
    return style;
}

// Every variation starts from the captured original; no cumulative mutation.
inline bool planPerformanceReply(const PhraseCapture::Phrase& original,
    const MotifDescriptor& motif, const ResponseKey& key,
    const ResponseHarmony& harmony, const ResponseStyle& style,
    const ReplyMemory& memory, bool loop, bool duet, uint8_t evolve,
    uint8_t delay, ResponsePlan& plan, ReplyStrategy& actual,
    ResponseHarmonyReport& report) {
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
    return planHarmonicEcho(phrase, motif, key, harmony, appliedStyle, plan, &report, delay);
}
} // namespace GroovePuterCallResponse
