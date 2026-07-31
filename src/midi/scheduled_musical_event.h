#pragma once

#include <cstdint>

#include "../input/musical_event.h"

// Audio-originated MIDI events keep their source sample position as a
// block sequence plus an offset inside that block. generation invalidates
// events from an obsolete target lifecycle. publicationSequence preserves
// deterministic order when multiple events share one sample position.
struct ScheduledMusicalEvent {
    MusicalEvent event{};
    uint32_t blockSequence{0};
    uint16_t frameOffset{0};
    uint32_t generation{0};
    uint32_t publicationSequence{0};
};

// Monotonic 32-bit sequence comparison with wrap-around support. The caller
// must not compare values separated by 2^31 or more increments.
inline constexpr bool midiSequenceBefore(uint32_t lhs, uint32_t rhs) {
    return static_cast<int32_t>(lhs - rhs) < 0;
}

inline constexpr bool scheduledMusicalEventBefore(
        const ScheduledMusicalEvent& lhs,
        const ScheduledMusicalEvent& rhs) {
    if (lhs.blockSequence != rhs.blockSequence) {
        return midiSequenceBefore(lhs.blockSequence, rhs.blockSequence);
    }
    if (lhs.frameOffset != rhs.frameOffset) {
        return lhs.frameOffset < rhs.frameOffset;
    }
    if (lhs.publicationSequence != rhs.publicationSequence) {
        return midiSequenceBefore(lhs.publicationSequence,
                                  rhs.publicationSequence);
    }
    return false;
}

inline constexpr bool scheduledMusicalEventFrameIsValid(
        const ScheduledMusicalEvent& scheduled,
        uint16_t blockFrames) {
    return blockFrames > 0 && scheduled.frameOffset < blockFrames;
}

inline constexpr bool scheduledMusicalEventGenerationIsCurrent(
        const ScheduledMusicalEvent& scheduled,
        uint32_t currentGeneration) {
    return scheduled.generation == currentGeneration;
}
