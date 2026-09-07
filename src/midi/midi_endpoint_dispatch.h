#pragma once
#ifndef GROOVEPUTER_MIDI_ENDPOINT_DISPATCH_H
#define GROOVEPUTER_MIDI_ENDPOINT_DISPATCH_H

#include <cstddef>
#include <cstdint>

#include "midi_transport.h"

namespace GroovePuterMidi {

enum class MidiEndpoint : uint8_t { Usb = 0, Uart = 1 };
enum class MidiEndpointEventType : uint8_t { NoteOn, NoteOff };

struct MidiEndpointEvent {
    uint32_t sequence{0};
    MidiEndpointEventType type{MidiEndpointEventType::NoteOff};
    uint8_t channel{0};
    uint8_t note{0};
    uint8_t velocity{0};
};

struct MidiEndpointDelivery {
    uint8_t acceptedMask{0};

    bool accepted(MidiEndpoint endpoint) const {
        return (acceptedMask & bit(endpoint)) != 0;
    }

    static constexpr uint8_t bit(MidiEndpoint endpoint) {
        return static_cast<uint8_t>(1u << static_cast<uint8_t>(endpoint));
    }
};

// Dispatches an event once per enabled endpoint. A retry with the same unique
// sequence visits only endpoints that have not already accepted that event.
// This preserves a valid UART attack while USB backpressure is retried.
class MidiEndpointDispatcher {
public:
    MidiEndpointDispatcher(IMidiTransport& usb, IMidiTransport& uart)
        : transports_{&usb, &uart} {}

    void setEnabled(MidiEndpoint endpoint, bool enabled) {
        const uint8_t mask = MidiEndpointDelivery::bit(endpoint);
        if (enabled) enabledMask_ |= mask;
        else enabledMask_ &= static_cast<uint8_t>(~mask);
    }

    MidiEndpointDelivery deliver(const MidiEndpointEvent& event) {
        DeliverySlot& slot = slotFor(event.sequence);
        uint8_t active = enabledMask_;
        for (uint8_t raw = 0; raw < 2; ++raw) {
            const MidiEndpoint endpoint = static_cast<MidiEndpoint>(raw);
            const uint8_t mask = MidiEndpointDelivery::bit(endpoint);
            if ((active & mask) == 0 || (slot.acceptedMask & mask) != 0) continue;
            IMidiTransport& transport = *transports_[raw];
            if (!transport.mounted()) continue;
            if (send(transport, event)) slot.acceptedMask |= mask;
        }
        return MidiEndpointDelivery{slot.acceptedMask};
    }

private:
    static constexpr std::size_t kDeliverySlots = 8;

    struct DeliverySlot {
        uint32_t sequence{0};
        uint8_t acceptedMask{0};
        bool occupied{false};
    };

    static bool send(IMidiTransport& transport, const MidiEndpointEvent& event) {
        if (event.type == MidiEndpointEventType::NoteOn) {
            return transport.sendNoteOn(event.channel, event.note, event.velocity);
        }
        return transport.sendNoteOff(event.channel, event.note, event.velocity);
    }

    DeliverySlot& slotFor(uint32_t sequence) {
        for (DeliverySlot& slot : slots_) {
            if (slot.occupied && slot.sequence == sequence) return slot;
        }
        DeliverySlot& slot = slots_[nextSlot_];
        nextSlot_ = (nextSlot_ + 1u) % kDeliverySlots;
        slot = DeliverySlot{sequence, 0, true};
        return slot;
    }

    IMidiTransport* transports_[2];
    DeliverySlot slots_[kDeliverySlots]{};
    uint8_t enabledMask_{0};
    std::size_t nextSlot_{0};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_ENDPOINT_DISPATCH_H
