#pragma once

#include <cstdint>

// Single-writer endpoint-health state for the sole USB MIDI dispatcher. TinyUSB
// can reject a short chord while its 16-packet FIFO drains, so the state becomes
// blocked only after rejects persist for a wall-clock interval.
enum class UsbEndpointHealthState : uint8_t {
    Ready = 0,
    Backpressured,
    Stalled,
};

struct UsbEndpointHealthSnapshot {
    UsbEndpointHealthState state{UsbEndpointHealthState::Ready};
    uint32_t writeAccepted{0};
    uint32_t writeRejected{0};
    uint32_t stalledTransitions{0};
    uint32_t recoveredTransitions{0};
    uint32_t currentBlockedMs{0};
    uint32_t maximumBlockedMs{0};
};

class UsbEndpointHealth {
public:
    explicit UsbEndpointHealth(uint32_t stallThresholdMs)
        : stallThresholdMs_(stallThresholdMs) {}

    void observeWrite(bool mounted, bool accepted, uint32_t nowMs) {
        if (!mounted) {
            resetBlockedWindow(nowMs);
            return;
        }

        if (accepted) {
            ++writeAccepted_;
            if (state_ == UsbEndpointHealthState::Stalled) {
                ++recoveredTransitions_;
            }
            resetBlockedWindow(nowMs);
            return;
        }

        ++writeRejected_;
        if (!blocked_) {
            blocked_ = true;
            blockedSinceMs_ = nowMs;
            state_ = UsbEndpointHealthState::Backpressured;
            return;
        }

        const uint32_t blockedMs = nowMs - blockedSinceMs_;
        if (blockedMs > maximumBlockedMs_) maximumBlockedMs_ = blockedMs;
        if (state_ != UsbEndpointHealthState::Stalled &&
            blockedMs >= stallThresholdMs_) {
            state_ = UsbEndpointHealthState::Stalled;
            ++stalledTransitions_;
        }
    }

    UsbEndpointHealthSnapshot snapshot(uint32_t nowMs) const {
        UsbEndpointHealthSnapshot snapshot{};
        snapshot.state = state_;
        snapshot.writeAccepted = writeAccepted_;
        snapshot.writeRejected = writeRejected_;
        snapshot.stalledTransitions = stalledTransitions_;
        snapshot.recoveredTransitions = recoveredTransitions_;
        snapshot.maximumBlockedMs = maximumBlockedMs_;
        snapshot.currentBlockedMs = blocked_ ? nowMs - blockedSinceMs_ : 0;
        return snapshot;
    }

private:
    void resetBlockedWindow(uint32_t nowMs) {
        if (blocked_) {
            const uint32_t blockedMs = nowMs - blockedSinceMs_;
            if (blockedMs > maximumBlockedMs_) maximumBlockedMs_ = blockedMs;
        }
        blocked_ = false;
        blockedSinceMs_ = nowMs;
        state_ = UsbEndpointHealthState::Ready;
    }

    uint32_t stallThresholdMs_{0};
    uint32_t blockedSinceMs_{0};
    uint32_t writeAccepted_{0};
    uint32_t writeRejected_{0};
    uint32_t stalledTransitions_{0};
    uint32_t recoveredTransitions_{0};
    uint32_t maximumBlockedMs_{0};
    UsbEndpointHealthState state_{UsbEndpointHealthState::Ready};
    bool blocked_{false};
};
