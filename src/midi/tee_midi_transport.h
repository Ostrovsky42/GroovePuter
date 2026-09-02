#pragma once
#ifndef GROOVEPUTER_TEE_MIDI_TRANSPORT_H
#define GROOVEPUTER_TEE_MIDI_TRANSPORT_H

#include <cstdint>

#include "midi_transport.h"

namespace GroovePuterMidi {

// Sends one MIDI stream to two wires while keeping a single musical owner.
//
// The alternative - a second output object bound to the DIN transport - would
// create two owners of the same note. That is not merely duplicated state: the
// SMF path retries a failed send up to 24 times, so a USB backpressure event
// would put repeated NoteOn messages on a DIN wire that already accepted the
// first one, and the two endpoints would drift apart in exactly the situation
// where recovery matters most.
//
// Teeing below the owner keeps note ownership, cleanup and retry policy
// identical to the single-endpoint path. Both wires receive byte-identical
// traffic.
//
// What this deliberately does NOT provide: independent routing or per-endpoint
// device profiles. Both wires carry the same channels and the same notes. Those
// require per-endpoint ownership and are a separate step.
class TeeMidiTransport final : public IMidiTransport {
public:
    struct Diagnostics {
        uint32_t secondaryRejected{0};
        uint32_t secondaryOnlyDelivered{0};
    };

    TeeMidiTransport(IMidiTransport& primary, IMidiTransport& secondary)
        : primary_(primary), secondary_(secondary) {}

    void setSecondaryEnabled(bool enabled) { secondaryEnabled_ = enabled; }
    bool secondaryEnabled() const { return secondaryEnabled_; }

    const Diagnostics& diagnostics() const { return diagnostics_; }

    bool begin() override {
        const bool primaryBegun = primary_.begin();
        const bool secondaryBegun = secondary_.begin();
        return primaryBegun || secondaryBegun;
    }

    // True when at least one wire can take traffic, so the DIN endpoint keeps
    // playing with no USB host attached.
    bool mounted() const override {
        return primary_.mounted() || (secondaryEnabled_ && secondary_.mounted());
    }

    // The tee is only as verifiable as its least verifiable active wire.
    MidiTransportLink linkKind() const override {
        if (secondaryEnabled_ && secondary_.mounted() &&
            secondary_.linkKind() == MidiTransportLink::Unverifiable) {
            return MidiTransportLink::Unverifiable;
        }
        return primary_.linkKind();
    }

    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        return dispatch(
            [&](IMidiTransport& t) { return t.sendNoteOn(channel, note, velocity); });
    }

    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        return dispatch(
            [&](IMidiTransport& t) { return t.sendNoteOff(channel, note, velocity); });
    }

    bool sendControlChange(uint8_t channel,
                           uint8_t controller,
                           uint8_t value) override {
        return dispatch([&](IMidiTransport& t) {
            return t.sendControlChange(channel, controller, value);
        });
    }

    bool sendTimingClock() override {
        return dispatch([](IMidiTransport& t) { return t.sendTimingClock(); });
    }
    bool sendStart() override {
        return dispatch([](IMidiTransport& t) { return t.sendStart(); });
    }
    bool sendContinue() override {
        return dispatch([](IMidiTransport& t) { return t.sendContinue(); });
    }
    bool sendStop() override {
        return dispatch([](IMidiTransport& t) { return t.sendStop(); });
    }
    bool sendSongPositionPointer(uint16_t midiBeats) override {
        return dispatch([&](IMidiTransport& t) {
            return t.sendSongPositionPointer(midiBeats);
        });
    }

    void flush() override {
        primary_.flush();
        if (secondaryEnabled_) secondary_.flush();
    }

private:
    // The authoritative answer is the USB wire whenever a host is attached,
    // because the caller's retry and cleanup policy was written against USB
    // backpressure. A secondary rejection is counted, never escalated: the DIN
    // queue has its own NoteOff reserve, and reporting failure here would make
    // the SMF path retry a note USB already delivered.
    template <typename SendFn>
    bool dispatch(SendFn&& send) {
        const bool primaryMounted = primary_.mounted();
        const bool primaryResult = primaryMounted ? send(primary_) : false;

        bool secondaryResult = false;
        if (secondaryEnabled_ && secondary_.mounted()) {
            secondaryResult = send(secondary_);
            if (!secondaryResult) ++diagnostics_.secondaryRejected;
        }

        if (primaryMounted) return primaryResult;
        if (secondaryResult) ++diagnostics_.secondaryOnlyDelivered;
        return secondaryResult;
    }

    IMidiTransport& primary_;
    IMidiTransport& secondary_;
    Diagnostics diagnostics_{};
    bool secondaryEnabled_{false};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_TEE_MIDI_TRANSPORT_H
