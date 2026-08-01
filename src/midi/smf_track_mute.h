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
        if (state.selectedTrack < 32u) {
            mutedMaskLow_.fetch_xor(uint32_t{1} << state.selectedTrack,
                                    std::memory_order_acq_rel);
        } else {
            mutedMaskHigh_.fetch_xor(
                uint32_t{1} << (state.selectedTrack - 32u),
                std::memory_order_acq_rel);
        }
        return true;
    }

    void clear() {
        mutedMaskLow_.store(0, std::memory_order_release);
        mutedMaskHigh_.store(0, std::memory_order_release);
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

private:
    std::atomic<uint16_t> trackCount_{0};
    std::atomic<uint16_t> selectedTrack_{0};
    std::atomic<uint32_t> mutedMaskLow_{0};
    std::atomic<uint32_t> mutedMaskHigh_{0};
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
