#pragma once
#ifndef GROOVEPUTER_MIDI_LATEST_PITCH_BEND_H
#define GROOVEPUTER_MIDI_LATEST_PITCH_BEND_H

#include <cstddef>
#include <cstdint>

#include "midi_input_event.h"
#include "midi_realtime_word.h"

namespace GroovePuterMidi {

struct LatestPitchBend {
    InputSource source{InputSource::Usb};
    uint32_t generation{0};
    uint8_t channel{0};
    uint8_t cable{0};
    uint16_t value14{8192};
};

// Bounded SPSC coalescing store for continuous pitch bend. Queue pressure may
// collapse intermediate values, but the consumer always observes the latest
// value per source/session/channel, including the terminal center value.
class MidiLatestPitchBendStore {
public:
    // This overflow store only receives external MIDI. Cardputer/QWERTY
    // gestures do not pass through the MIDI ingress queues.
    static constexpr std::size_t kSourceCount = 2;
    static constexpr std::size_t kChannelCount = 16;
    static constexpr std::size_t kSlotCount = kSourceCount * kChannelCount;

    void publish(InputSource source, uint32_t generation,
                 uint8_t channel, uint16_t value14, uint8_t cable = 0) {
        const std::size_t sourceValue = static_cast<std::size_t>(source);
        if (sourceValue < static_cast<std::size_t>(InputSource::Usb) ||
            sourceValue > static_cast<std::size_t>(InputSource::Uart) ||
            channel >= kChannelCount) return;
        const std::size_t slot =
            (sourceValue - static_cast<std::size_t>(InputSource::Usb)) * kChannelCount + channel;
        // The store is SPSC. Only its producer writes revisions_, so the
        // published even revision is also the next write's starting value.
        uint32_t next = revisions_[slot].loadRelaxed() + 2u;
        if (next == 0u) next = 2u;
        revisions_[slot].storeRelease(next - 1u);  // odd while publishing
        generations_[slot].storeRelaxed(generation);
        values_[slot].storeRelaxed(
            (value14 > 16383u ? 16383u : value14) |
            (static_cast<uint32_t>(cable & 0x0fu) << 14u));
        revisions_[slot].storeRelease(next);
    }

    bool takeNext(LatestPitchBend& out) {
        for (std::size_t checked = 0; checked < kSlotCount; ++checked) {
            const std::size_t slot = (readCursor_ + checked) % kSlotCount;
            const uint32_t before = revisions_[slot].loadAcquire();
            if (before == 0u || (before & 1u) != 0u ||
                before == consumedRevision_[slot]) continue;
            const uint32_t generation = generations_[slot].loadRelaxed();
            const uint32_t value = values_[slot].loadRelaxed();
            const uint32_t after = revisions_[slot].loadAcquire();
            if (before != after || (after & 1u) != 0u) continue;
            consumedRevision_[slot] = after;
            readCursor_ = (slot + 1u) % kSlotCount;
            out.source = static_cast<InputSource>(
                static_cast<std::size_t>(InputSource::Usb) + slot / kChannelCount);
            out.channel = static_cast<uint8_t>(slot % kChannelCount);
            out.generation = generation;
            out.value14 = static_cast<uint16_t>(value & 0x3fffu);
            out.cable = static_cast<uint8_t>((value >> 14u) & 0x0fu);
            return true;
        }
        return false;
    }

private:
    MidiRealtimeWord revisions_[kSlotCount]{};
    MidiRealtimeWord generations_[kSlotCount]{};
    MidiRealtimeWord values_[kSlotCount]{};
    uint32_t consumedRevision_[kSlotCount]{};  // consumer-owned
    std::size_t readCursor_{0};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_LATEST_PITCH_BEND_H
