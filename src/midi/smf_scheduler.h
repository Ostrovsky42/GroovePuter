#pragma once

#include <cstdint>

#include "smf_timing.h"

namespace GroovePuterMidi {

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
                     SmfScheduledPosition& out);

}  // namespace GroovePuterMidi
