#pragma once
#ifndef GROOVEPUTER_SMF_TRACK_INSPECTOR_H
#define GROOVEPUTER_SMF_TRACK_INSPECTOR_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "smf_document.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfTrackInspectorMaxTracks = 32;
constexpr std::size_t kSmfTrackNameBytes = 16;

struct SmfTrackInfoSnapshot {
    char name[kSmfTrackNameBytes]{};
    uint16_t channelMask{0};
    uint8_t firstProgram{0};
    uint8_t flags{0};

    static constexpr uint8_t kAudible = 1u << 0;
    static constexpr uint8_t kHasProgram = 1u << 1;
    static constexpr uint8_t kLikelyDrums = 1u << 2;
    static constexpr uint8_t kHasName = 1u << 3;

    bool audible() const { return (flags & kAudible) != 0; }
    bool hasProgramChange() const { return (flags & kHasProgram) != 0; }
    bool likelyDrums() const { return (flags & kLikelyDrums) != 0; }
    bool hasName() const { return (flags & kHasName) != 0 && name[0] != '\0'; }

    bool usesMultipleChannels() const {
        return channelMask != 0u && (channelMask & (channelMask - 1u)) != 0u;
    }

    int primaryChannel() const {
        if (channelMask == 0u || usesMultipleChannels()) return -1;
        for (int channel = 0; channel < 16; ++channel) {
            if ((channelMask & (uint16_t{1} << channel)) != 0u) return channel;
        }
        return -1;
    }
};

struct SmfTrackInspectorSnapshot {
    uint16_t trackCount{0};
    uint16_t declaredTrackCount{0};
    SmfTrackInfoSnapshot tracks[kSmfTrackInspectorMaxTracks]{};

    bool tracksTruncated() const { return declaredTrackCount > trackCount; }

    uint16_t audibleTrackCount() const {
        uint16_t count = 0;
        const uint16_t bounded = trackCount > kSmfTrackInspectorMaxTracks
            ? static_cast<uint16_t>(kSmfTrackInspectorMaxTracks)
            : trackCount;
        for (uint16_t track = 0; track < bounded; ++track) {
            if (tracks[track].audible()) ++count;
        }
        return count;
    }
};

static_assert(sizeof(SmfTrackInfoSnapshot) == 20,
              "SMF track rows must remain compact and fixed-size");
static_assert(sizeof(SmfTrackInspectorSnapshot) <= 644,
              "SMF track snapshots must remain bounded on the UI stack");

class SmfTrackInspectorState {
public:
    void reset(uint16_t trackCount, uint16_t declaredTrackCount = 0) {
        if (trackCount > kSmfTrackInspectorMaxTracks) {
            trackCount = static_cast<uint16_t>(kSmfTrackInspectorMaxTracks);
        }
        if (declaredTrackCount < trackCount) declaredTrackCount = trackCount;

        state_.store(0u, std::memory_order_release);
        declaredTrackCount_.store(declaredTrackCount, std::memory_order_relaxed);
        for (std::size_t track = 0; track < kSmfTrackInspectorMaxTracks; ++track) {
            AtomicTrack& item = tracks_[track];
            item.channelMask.store(0u, std::memory_order_relaxed);
            item.firstProgram.store(0u, std::memory_order_relaxed);
            item.flags.store(0u, std::memory_order_relaxed);
        }
        state_.store(trackCount, std::memory_order_release);
    }

    void freeze() {
        state_.fetch_or(kFrozenBit, std::memory_order_acq_rel);
    }

    bool frozen() const {
        return (state_.load(std::memory_order_acquire) & kFrozenBit) != 0u;
    }

    void setName(uint16_t trackIndex, const char* name) {
        // Arbitrary SMF TrackName strings used to reserve fixed DRAM for every
        // physical track. Stage 1A/1B needs physical identity, channel and
        // program; the UI derives a bounded GM-family label from firstProgram.
        // Keep this hook so the stream parser stays unchanged without retaining
        // per-file strings in global storage.
        (void)trackIndex;
        (void)name;
    }

    void observe(uint16_t trackIndex, const SmfEvent& event) {
        const uint16_t state = state_.load(std::memory_order_acquire);
        if ((state & kFrozenBit) != 0u) return;
        const uint16_t count = state & kCountMask;
        if (trackIndex >= count || event.channel >= 16u) return;

        AtomicTrack& item = tracks_[trackIndex];
        if (event.kind == SmfEventKind::NoteOn) {
            item.channelMask.fetch_or(
                static_cast<uint16_t>(uint16_t{1} << event.channel),
                std::memory_order_relaxed);
            uint8_t flags = SmfTrackInfoSnapshot::kAudible;
            if (event.channel == 9u) flags |= SmfTrackInfoSnapshot::kLikelyDrums;
            item.flags.fetch_or(flags, std::memory_order_release);
            return;
        }

        if (event.kind == SmfEventKind::ProgramChange) {
            item.channelMask.fetch_or(
                static_cast<uint16_t>(uint16_t{1} << event.channel),
                std::memory_order_relaxed);
            const uint8_t flags = item.flags.load(std::memory_order_acquire);
            if ((flags & SmfTrackInfoSnapshot::kHasProgram) == 0u) {
                item.firstProgram.store(event.data1, std::memory_order_relaxed);
                item.flags.fetch_or(SmfTrackInfoSnapshot::kHasProgram,
                                    std::memory_order_release);
            }
        }
    }

    SmfTrackInspectorSnapshot snapshot() const {
        SmfTrackInspectorSnapshot result{};
        result.trackCount = state_.load(std::memory_order_acquire) & kCountMask;
        if (result.trackCount > kSmfTrackInspectorMaxTracks) {
            result.trackCount = static_cast<uint16_t>(kSmfTrackInspectorMaxTracks);
        }
        result.declaredTrackCount =
            declaredTrackCount_.load(std::memory_order_relaxed);
        if (result.declaredTrackCount < result.trackCount) {
            result.declaredTrackCount = result.trackCount;
        }

        for (uint16_t track = 0; track < result.trackCount; ++track) {
            const AtomicTrack& item = tracks_[track];
            SmfTrackInfoSnapshot& output = result.tracks[track];
            output.flags = item.flags.load(std::memory_order_acquire);
            output.channelMask = item.channelMask.load(std::memory_order_relaxed);
            output.firstProgram = item.firstProgram.load(std::memory_order_relaxed);

            const char* label = nullptr;
            if (output.likelyDrums()) {
                label = "Drums";
            } else if (output.hasProgramChange()) {
                label = programLabel(output.firstProgram);
            }
            if (label) {
                copyLabel(output.name, label);
                output.flags |= SmfTrackInfoSnapshot::kHasName;
            }
        }
        return result;
    }

private:
    static constexpr uint16_t kFrozenBit = uint16_t{1} << 15u;
    static constexpr uint16_t kCountMask = kFrozenBit - 1u;

    struct AtomicTrack {
        std::atomic<uint16_t> channelMask{0};
        std::atomic<uint8_t> firstProgram{0};
        std::atomic<uint8_t> flags{0};
    };

    static void copyLabel(char* output, const char* input) {
        std::size_t index = 0;
        while (index + 1u < kSmfTrackNameBytes && input[index] != '\0') {
            output[index] = input[index];
            ++index;
        }
        output[index] = '\0';
    }

    static const char* programLabel(uint8_t program) {
        switch (program) {
            case 0: return "Grand Piano";
            case 4: return "E.Piano";
            case 24: return "Nylon Guitar";
            case 25: return "Steel Guitar";
            case 32: return "Acoustic Bass";
            case 33: return "Finger Bass";
            case 34: return "Pick Bass";
            case 38: return "Synth Bass";
            case 40: return "Violin";
            case 42: return "Cello";
            case 48: return "Strings";
            case 52: return "Choir";
            case 56: return "Trumpet";
            case 61: return "Brass";
            case 64: return "Soprano Sax";
            case 65: return "Alto Sax";
            case 66: return "Tenor Sax";
            case 73: return "Flute";
            case 80: return "Square Lead";
            case 81: return "Saw Lead";
            case 87: return "Bass Lead";
            case 88: return "New Age Pad";
            case 89: return "Warm Pad";
            case 90: return "Poly Pad";
            default: break;
        }

        if (program < 8u) return "Piano";
        if (program < 16u) return "Chromatic";
        if (program < 24u) return "Organ";
        if (program < 32u) return "Guitar";
        if (program < 40u) return "Bass";
        if (program < 48u) return "Strings";
        if (program < 56u) return "Ensemble";
        if (program < 64u) return "Brass";
        if (program < 72u) return "Reed";
        if (program < 80u) return "Pipe";
        if (program < 88u) return "Lead";
        if (program < 96u) return "Pad";
        if (program < 104u) return "Synth FX";
        if (program < 112u) return "Ethnic";
        if (program < 120u) return "Percussive";
        return "Sound FX";
    }

    std::atomic<uint16_t> state_{0};
    std::atomic<uint16_t> declaredTrackCount_{0};
    AtomicTrack tracks_[kSmfTrackInspectorMaxTracks]{};
};

static_assert(sizeof(SmfTrackInspectorState) <= 136,
              "SMF track metadata must remain bounded in fixed DRAM");

inline SmfTrackInspectorState& smfTrackInspectorState() {
    static SmfTrackInspectorState state;
    return state;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_TRACK_INSPECTOR_H
