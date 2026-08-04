#pragma once
#ifndef GROOVEPUTER_SMF_TRACK_INSPECTOR_H
#define GROOVEPUTER_SMF_TRACK_INSPECTOR_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "smf_document.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfTrackInspectorMaxTracks = 64;
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
    SmfTrackInfoSnapshot tracks[kSmfTrackInspectorMaxTracks]{};

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
static_assert(sizeof(SmfTrackInspectorSnapshot) <= 1284,
              "SMF track metadata must remain bounded for Cardputer ADV DRAM");

class SmfTrackInspectorState {
public:
    void reset(uint16_t trackCount) {
        if (trackCount > kSmfTrackInspectorMaxTracks) {
            trackCount = static_cast<uint16_t>(kSmfTrackInspectorMaxTracks);
        }
        trackCount_.store(0, std::memory_order_release);
        for (std::size_t track = 0; track < kSmfTrackInspectorMaxTracks; ++track) {
            AtomicTrack& item = tracks_[track];
            for (std::size_t word = 0; word < kNameWordCount; ++word) {
                item.nameWords[word].store(0, std::memory_order_relaxed);
            }
            item.channelMask.store(0, std::memory_order_relaxed);
            item.firstProgram.store(0, std::memory_order_relaxed);
            item.flags.store(0, std::memory_order_relaxed);
        }
        trackCount_.store(trackCount, std::memory_order_release);
    }

    void setName(uint16_t trackIndex, const char* name) {
        const uint16_t count = trackCount_.load(std::memory_order_acquire);
        if (trackIndex >= count || !name || name[0] == '\0') return;

        char sanitized[kSmfTrackNameBytes]{};
        std::size_t length = 0;
        while (length + 1u < kSmfTrackNameBytes && name[length] != '\0') {
            const unsigned char value = static_cast<unsigned char>(name[length]);
            sanitized[length] = value >= 32u && value <= 126u
                ? static_cast<char>(value)
                : ' ';
            ++length;
        }
        while (length > 0u && sanitized[length - 1u] == ' ') --length;
        sanitized[length] = '\0';
        if (length == 0u) return;

        AtomicTrack& item = tracks_[trackIndex];
        for (std::size_t word = 0; word < kNameWordCount; ++word) {
            uint32_t packed = 0;
            const std::size_t offset = word * sizeof(uint32_t);
            for (std::size_t byte = 0; byte < sizeof(uint32_t); ++byte) {
                const std::size_t index = offset + byte;
                if (index >= kSmfTrackNameBytes) break;
                packed |= static_cast<uint32_t>(
                    static_cast<uint8_t>(sanitized[index])) << (byte * 8u);
            }
            item.nameWords[word].store(packed, std::memory_order_relaxed);
        }
        item.flags.fetch_or(SmfTrackInfoSnapshot::kHasName,
                            std::memory_order_release);
    }

    void observe(uint16_t trackIndex, const SmfEvent& event) {
        const uint16_t count = trackCount_.load(std::memory_order_acquire);
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
        result.trackCount = trackCount_.load(std::memory_order_acquire);
        if (result.trackCount > kSmfTrackInspectorMaxTracks) {
            result.trackCount = static_cast<uint16_t>(kSmfTrackInspectorMaxTracks);
        }

        for (uint16_t track = 0; track < result.trackCount; ++track) {
            const AtomicTrack& item = tracks_[track];
            SmfTrackInfoSnapshot& output = result.tracks[track];
            output.flags = item.flags.load(std::memory_order_acquire);
            output.channelMask = item.channelMask.load(std::memory_order_relaxed);
            output.firstProgram = item.firstProgram.load(std::memory_order_relaxed);
            if ((output.flags & SmfTrackInfoSnapshot::kHasName) != 0u) {
                for (std::size_t word = 0; word < kNameWordCount; ++word) {
                    const uint32_t packed =
                        item.nameWords[word].load(std::memory_order_relaxed);
                    const std::size_t offset = word * sizeof(uint32_t);
                    for (std::size_t byte = 0; byte < sizeof(uint32_t); ++byte) {
                        const std::size_t index = offset + byte;
                        if (index >= kSmfTrackNameBytes) break;
                        output.name[index] = static_cast<char>(
                            (packed >> (byte * 8u)) & 0xFFu);
                    }
                }
                output.name[kSmfTrackNameBytes - 1u] = '\0';
            }
        }
        return result;
    }

private:
    static constexpr std::size_t kNameWordCount =
        kSmfTrackNameBytes / sizeof(uint32_t);

    struct AtomicTrack {
        std::atomic<uint32_t> nameWords[kNameWordCount]{};
        std::atomic<uint16_t> channelMask{0};
        std::atomic<uint8_t> firstProgram{0};
        std::atomic<uint8_t> flags{0};
    };

    std::atomic<uint16_t> trackCount_{0};
    AtomicTrack tracks_[kSmfTrackInspectorMaxTracks]{};
};

inline SmfTrackInspectorState& smfTrackInspectorState() {
    static SmfTrackInspectorState state;
    return state;
}

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_SMF_TRACK_INSPECTOR_H
