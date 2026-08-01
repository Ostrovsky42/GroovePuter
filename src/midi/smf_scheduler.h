#pragma once

#include <cstdint>

#include "smf_timing.h"

namespace GroovePuterMidi {

inline constexpr uint16_t kSmfOriginalTempoScalePermille = 1000;

inline constexpr uint64_t scaleSmfPlaybackMicros(
        uint64_t fileMicros,
        uint16_t tempoScalePermille) {
    return tempoScalePermille == 0
        ? 0
        : (fileMicros * kSmfOriginalTempoScalePermille +
           tempoScalePermille / 2u) / tempoScalePermille;
}

inline constexpr uint64_t scaleSmfFileMicros(
        uint64_t playbackMicros,
        uint16_t tempoScalePermille) {
    return (playbackMicros * tempoScalePermille +
            kSmfOriginalTempoScalePermille / 2u) /
           kSmfOriginalTempoScalePermille;
}

struct SmfScheduledPosition {
    uint32_t blockSequence{0};
    uint16_t frameOffset{0};
};

// Converts an SMF tick into the accepted audio-block/sample timeline. The
// player task may call this ahead of playback; MidiDispatchTask remains the sole
// owner of actual USB deadlines.
bool scheduleSmfTick(const SmfTimingMap& timing,
                     uint32_t originTick,
                     uint32_t originBlockSequence,
                     uint32_t eventTick,
                     uint32_t sampleRate,
                     uint16_t blockFrames,
                     SmfScheduledPosition& out,
                     uint16_t tempoScalePermille =
                         kSmfOriginalTempoScalePermille);

}  // namespace GroovePuterMidi
