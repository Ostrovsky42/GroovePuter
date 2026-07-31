#include "smf_scheduler.h"

#include <limits>

namespace GroovePuterMidi {

bool scheduleSmfTick(const SmfTimingMap& timing,
                     uint32_t originTick,
                     uint32_t originBlockSequence,
                     uint32_t eventTick,
                     uint32_t sampleRate,
                     uint16_t blockFrames,
                     SmfScheduledPosition& out) {
    if (!timing.valid() || sampleRate == 0 || blockFrames == 0 ||
        eventTick < originTick) {
        return false;
    }

    const uint64_t originMicros = timing.tickToMicros(originTick);
    const uint64_t eventMicros = timing.tickToMicros(eventTick);
    if (eventMicros < originMicros) return false;

    const uint64_t deltaMicros = eventMicros - originMicros;
    // Convert once to a sample position so fractional block durations cannot
    // accumulate rounding error over long files.
    const uint64_t totalFrames =
        (deltaMicros * static_cast<uint64_t>(sampleRate) + 500000ull) /
        1000000ull;
    const uint64_t blockOffset = totalFrames / blockFrames;
    if (blockOffset > std::numeric_limits<uint32_t>::max()) return false;

    out.blockSequence = originBlockSequence + static_cast<uint32_t>(blockOffset);
    out.frameOffset = static_cast<uint16_t>(totalFrames % blockFrames);
    return true;
}

}  // namespace GroovePuterMidi
