#pragma once

#include "call_response_phrase_capture.h"
#include "call_response_motif.h"

namespace GroovePuterCallResponse {

enum class ReplyStrategy : uint8_t { Echo, ShortReply, Develop };

struct ReplyMemory {
    uint32_t lastSignature{0};
    uint8_t repeatCount{0};
    ReplyStrategy lastStrategy{ReplyStrategy::Echo};
    bool valid{false};
};

inline uint32_t callSignature(const PhraseCapture::Phrase& phrase, const MotifDescriptor& motif) {
    uint32_t hash = 2166136261u;
    const auto add = [&](uint8_t value) { hash = (hash ^ value) * 16777619u; };
    add(phrase.windowSteps);
    add(phrase.count);
    for (uint8_t i = 0; i < phrase.count && i < PhraseCapture::kCapacity; ++i) {
        add(phrase.notes[i].onset);
        add(static_cast<uint8_t>(static_cast<int>(phrase.notes[i].note) - phrase.notes[0].note));
    }
    (void)motif;
    return hash;
}

inline ReplyStrategy chooseReplyStrategy(const PhraseCapture::Phrase& phrase,
                                         const MotifDescriptor& motif,
                                         const ReplyMemory& memory) {
    if (phrase.count < 3) return ReplyStrategy::Echo;
    if (memory.valid && memory.lastSignature == callSignature(phrase, motif)) {
        if (memory.lastStrategy == ReplyStrategy::Echo) return ReplyStrategy::Develop;
        if (memory.lastStrategy == ReplyStrategy::Develop) return ReplyStrategy::ShortReply;
        return ReplyStrategy::Echo;
    }
    return phrase.count * 16u >= phrase.windowSteps * 8u
        ? ReplyStrategy::ShortReply : ReplyStrategy::Develop;
}

inline void rememberReply(ReplyMemory& memory, uint32_t signature, ReplyStrategy strategy) {
    const bool repeat = memory.valid && memory.lastSignature == signature;
    memory.repeatCount = repeat ? (memory.repeatCount == 255 ? 255 : memory.repeatCount + 1) : 0;
    memory.lastSignature = signature;
    memory.lastStrategy = strategy;
    memory.valid = true;
}

// Transform a copy of the CALL; preserve the original when a change cannot fit.
inline bool transformCall(const PhraseCapture::Phrase& input, ReplyStrategy strategy,
                          uint8_t delaySteps, PhraseCapture::Phrase& output) {
    output = input;
    if (strategy == ReplyStrategy::Echo || input.count == 0 ||
        input.count > PhraseCapture::kCapacity || input.windowSteps == 0 ||
        input.windowSteps > PhraseCapture::kWindowSteps || delaySteps >= input.windowSteps)
        return false;
    const int usable = input.windowSteps - delaySteps;
    bool changed = false;
    if (strategy == ReplyStrategy::ShortReply && input.count >= 2) {
        output.count = (input.count + 1u) / 2u;
        changed = output.count != input.count;
        auto& last = output.notes[output.count - 1];
        const int maxLength = usable - 1 - last.onset;
        if (maxLength < 1) { output = input; return false; }
        if (last.length > maxLength) last.length = static_cast<uint8_t>(maxLength);
    } else if (strategy == ReplyStrategy::Develop && input.count >= 3) {
        for (uint8_t i = 2; i < output.count; i += 2) {
            const int shifted = output.notes[i].onset + 1;
            if (shifted >= usable || (i + 1 < output.count && shifted >= output.notes[i + 1].onset)) continue;
            output.notes[i].onset = static_cast<uint8_t>(shifted);
            changed = true;
        }
        for (uint8_t i = 0; i < output.count; ++i) {
            const int end = i + 1 < output.count ? output.notes[i + 1].onset : usable;
            const int maxLength = end - output.notes[i].onset;
            if (maxLength < 1) { output = input; return false; }
            if (output.notes[i].length > maxLength) output.notes[i].length = static_cast<uint8_t>(maxLength);
        }
    }
    if (!changed) output = input;
    return changed;
}

}  // namespace GroovePuterCallResponse
