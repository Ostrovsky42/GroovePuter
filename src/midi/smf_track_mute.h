#pragma once
#ifndef GROOVEPUTER_SMF_TRACK_MUTE_H
#define GROOVEPUTER_SMF_TRACK_MUTE_H

#include <atomic>
#include <cstdint>

namespace GroovePuterMidi {

struct SmfTrackMuteSnapshot {
    uint16_t trackCount{0};
    uint16_t selectedTrack{0};
    uint64_t mutedMask{0};

    bool selectedMuted() const {
        return selectedTrack < 64u &&
               (mutedMask & (uint64_t{1} << selectedTrack)) != 0;
    }
};

class SmfTrackMuteState {
public:
    void reset(uint16_t trackCount) {
        if (trackCount > 64u) trackCount = 64u;
        trackCount_.store(trackCount, std::memory_order_release);
        selectedTrack_.store(0, std::memory_order_release);
        mutedMaskLow_.store(0, std::memory_order_release);
        mutedMaskHigh_.store(0, std::memory_order_release);
        pendingReleaseLow_.store(0, std::memory_order_release);
        pendingReleaseHigh_.store(0, std::memory_order_release);
    }

    SmfTrackMuteSnapshot snapshot() const {
        SmfTrackMuteSnapshot result{};
        result.trackCount = trackCount_.load(std::memory_order_acquire);
        result.selectedTrack = selectedTrack_.load(std::memory_order_acquire);
        const uint64_t low = mutedMaskLow_.load(std::memory_order_acquire);
        const uint64_t high = mutedMaskHigh_.load(std::memory_order_acquire);
        result.mutedMask = low | (high << 32u);
        if (result.trackCount == 0) {
            result.selectedTrack = 0;
        } else if (result.selectedTrack >= result.trackCount) {
            result.selectedTrack = static_cast<uint16_t>(result.trackCount - 1u);
        }
        return result;
    }

    bool selectTrack(uint16_t trackIndex) {
        const uint16_t count = trackCount_.load(std::memory_order_acquire);
        if (count == 0 || trackIndex >= count) return false;
        selectedTrack_.store(trackIndex, std::memory_order_release);
        return true;
    }

    void selectRelative(int delta) {
        const uint16_t count = trackCount_.load(std::memory_order_acquire);
        if (count == 0 || delta == 0) return;
        int selected = static_cast<int>(
            selectedTrack_.load(std::memory_order_acquire));
        selected = (selected + delta) % static_cast<int>(count);
        if (selected < 0) selected += count;
        selectedTrack_.store(static_cast<uint16_t>(selected),
                             std::memory_order_release);
    }

    bool toggleSelected() {
        const SmfTrackMuteSnapshot state = snapshot();
        if (state.trackCount == 0 || state.selectedTrack >= 64u) return false;

        const bool wasMuted = state.selectedMuted();
        if (state.selectedTrack < 32u) {
            const uint32_t bit = uint32_t{1} << state.selectedTrack;
            mutedMaskLow_.fetch_xor(bit, std::memory_order_acq_rel);
            if (!wasMuted) pendingReleaseLow_.fetch_or(bit, std::memory_order_acq_rel);
        } else {
            const uint32_t bit = uint32_t{1} << (state.selectedTrack - 32u);
            mutedMaskHigh_.fetch_xor(bit, std::memory_order_acq_rel);
            if (!wasMuted) pendingReleaseHigh_.fetch_or(bit, std::memory_order_acq_rel);
        }
        return true;
    }

    void clear() {
        mutedMaskLow_.store(0, std::memory_order_release);
        mutedMaskHigh_.store(0, std::memory_order_release);
        pendingReleaseLow_.store(0, std::memory_order_release);
        pendingReleaseHigh_.store(0, std::memory_order_release);
    }

    bool isMuted(uint16_t trackIndex) const {
        if (trackIndex >= 64u) return false;
        if (trackIndex < 32u) {
            return (mutedMaskLow_.load(std::memory_order_acquire) &
                    (uint32_t{1} << trackIndex)) != 0;
        }
        return (mutedMaskHigh_.load(std::memory_order_acquire) &
                (uint32_t{1} << (trackIndex - 32u))) != 0;
    }

    bool takePendingReleaseTrack(uint8_t& trackIndex) {
        uint32_t low = pendingReleaseLow_.load(std::memory_order_acquire);
        while (low != 0u) {
            const uint32_t bit = low & (~low + 1u);
            const uint32_t next = low & ~bit;
            if (pendingReleaseLow_.compare_exchange_weak(
                    low, next, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                trackIndex = static_cast<uint8_t>(trailingZeroes(bit));
                return true;
            }
        }

        uint32_t high = pendingReleaseHigh_.load(std::memory_order_acquire);
        while (high != 0u) {
            const uint32_t bit = high & (~high + 1u);
            const uint32_t next = high & ~bit;
            if (pendingReleaseHigh_.compare_exchange_weak(
                    high, next, std::memory_order_acq_rel,
                    std::memory_order_acquire)) {
                trackIndex = static_cast<uint8_t>(32u + trailingZeroes(bit));
                return true;
            }
        }
        return false;
    }

private:
    static uint8_t trailingZeroes(uint32_t value) {
        uint8_t count = 0;
        while ((value & 1u) == 0u) {
            value >>= 1u;
            ++count;
        }
        return count;
    }

    std::atomic<uint16_t> trackCount_{0};
    std::atomic<uint16_t> selectedTrack_{0};
    std::atomic<uint32_t> mutedMaskLow_{0};
    std::atomic<uint32_t> mutedMaskHigh_{0};
    std::atomic<uint32_t> pendingReleaseLow_{0};
    std::atomic<uint32_t> pendingReleaseHigh_{0};
};

inline SmfTrackMuteState& smfTrackMuteState() {
    static SmfTrackMuteState state;
    return state;
}

inline bool shouldEmitSmfTrackEvent(bool noteOn,
                                    uint16_t trackIndex,
                                    const SmfTrackMuteState& state =
                                        smfTrackMuteState()) {
    // NoteOff remains cleanup-critical even when a track is muted after its
    // NoteOn was already queued or dispatched.
    return !noteOn || !state.isMuted(trackIndex);
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_TRACK_MUTE_H
