#pragma once

#include <cstddef>
#include <cstdint>

#include "smf_document.h"

namespace GroovePuterMidi {

constexpr std::size_t kSmfMidiChannelCount = 16;

struct SmfChannelInfo {
    uint32_t noteCount{0};
    uint32_t velocitySum{0};
    uint8_t minNote{127};
    uint8_t maxNote{0};
    uint8_t firstProgram{0};
    uint8_t maxPolyphony{0};
    bool used{false};
    bool likelyDrums{false};
    bool hasProgramChange{false};

    bool hasNotes() const { return noteCount > 0; }

    uint8_t averageVelocity() const {
        if (noteCount == 0) return 0;
        const uint32_t average = velocitySum / noteCount;
        return static_cast<uint8_t>(average > 127u ? 127u : average);
    }
};

struct SmfChannelInspectorSnapshot {
    uint16_t format{0};
    uint16_t division{0};
    uint16_t trackCount{0};
    uint16_t usedChannelMask{0};
    SmfChannelInfo channels[kSmfMidiChannelCount]{};

    uint8_t usedChannelCount() const {
        uint8_t count = 0;
        for (std::size_t channel = 0; channel < kSmfMidiChannelCount; ++channel) {
            if ((usedChannelMask & (1u << channel)) != 0) ++count;
        }
        return count;
    }
};

class SmfChannelInspectorBuilder {
public:
    void reset(uint16_t format, uint16_t division, uint16_t trackCount) {
        snapshot_ = SmfChannelInspectorSnapshot{};
        snapshot_.format = format;
        snapshot_.division = division;
        snapshot_.trackCount = trackCount;
        for (std::size_t channel = 0; channel < kSmfMidiChannelCount; ++channel) {
            activeNotes_[channel] = 0;
        }
    }

    void observe(const SmfEvent& event) {
        if (event.channel >= kSmfMidiChannelCount) return;
        const std::size_t channel = event.channel;
        SmfChannelInfo& info = snapshot_.channels[channel];

        if (event.kind == SmfEventKind::ProgramChange) {
            markUsed(channel, info);
            if (!info.hasProgramChange) info.firstProgram = event.data1;
            info.hasProgramChange = true;
            return;
        }

        if (event.kind == SmfEventKind::NoteOn) {
            markUsed(channel, info);
            ++info.noteCount;
            info.velocitySum += event.data2;
            if (event.data1 < info.minNote) info.minNote = event.data1;
            if (event.data1 > info.maxNote) info.maxNote = event.data1;
            if (activeNotes_[channel] < 0xFFFFu) ++activeNotes_[channel];
            const uint16_t bounded = activeNotes_[channel] > 255u
                ? 255u
                : activeNotes_[channel];
            if (bounded > info.maxPolyphony) {
                info.maxPolyphony = static_cast<uint8_t>(bounded);
            }
            return;
        }

        if (event.kind == SmfEventKind::NoteOff && activeNotes_[channel] > 0) {
            --activeNotes_[channel];
        }
    }

    const SmfChannelInspectorSnapshot& snapshot() const { return snapshot_; }

private:
    void markUsed(std::size_t channel, SmfChannelInfo& info) {
        info.used = true;
        info.likelyDrums = channel == 9;
        snapshot_.usedChannelMask |= static_cast<uint16_t>(1u << channel);
    }

    SmfChannelInspectorSnapshot snapshot_{};
    uint16_t activeNotes_[kSmfMidiChannelCount]{};
};

}  // namespace GroovePuterMidi
