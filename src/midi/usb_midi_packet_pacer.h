#pragma once

#include <cstdint>

namespace GroovePuterMidi {

class UsbMidiPacketPacer {
public:
    explicit constexpr UsbMidiPacketPacer(uint32_t spacingMicros)
        : spacingMicros_(spacingMicros) {}

    uint32_t waitMicros(uint32_t nowMicros) const {
        if (!armed_) return 0;
        const int32_t remaining = static_cast<int32_t>(
            nextAttemptMicros_ - nowMicros);
        return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
    }

    void recordAttempt(uint32_t nowMicros) {
        nextAttemptMicros_ = nowMicros + spacingMicros_;
        armed_ = true;
    }

    void reset() {
        armed_ = false;
        nextAttemptMicros_ = 0;
    }

private:
    uint32_t spacingMicros_{0};
    uint32_t nextAttemptMicros_{0};
    bool armed_{false};
};

}  // namespace GroovePuterMidi
