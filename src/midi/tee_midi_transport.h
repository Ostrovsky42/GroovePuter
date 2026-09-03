#pragma once
#ifndef GROOVEPUTER_TEE_MIDI_TRANSPORT_H
#define GROOVEPUTER_TEE_MIDI_TRANSPORT_H

#include <cstdint>

#include "midi_transport.h"

namespace GroovePuterMidi {

// Sends one owner-controlled MIDI stream to two physical wires.
// The secondary is never allowed to accept a message that an authoritative
// primary rejected, because the musical owner records such a message as unsent.
class TeeMidiTransport final : public IMidiTransport {
public:
    static constexpr uint32_t kPrimaryStallRejects = 64;

    struct Diagnostics {
        uint32_t secondaryRejected{0};
        uint32_t secondaryOnlyDelivered{0};
        uint32_t secondarySkipped{0};
        uint32_t primaryConsecutiveRejects{0};
        uint32_t primaryStallDemotions{0};
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

    bool mounted() const override {
        return primary_.mounted() || (secondaryEnabled_ && secondary_.mounted());
    }

    bool primaryStalled() const {
        return diagnostics_.primaryConsecutiveRejects >= kPrimaryStallRejects;
    }

    MidiTransportLink linkKind() const override {
        if (secondaryEnabled_ && secondary_.mounted() &&
            secondary_.linkKind() == MidiTransportLink::Unverifiable) {
            return MidiTransportLink::Unverifiable;
        }
        return primary_.linkKind();
    }

    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        return dispatch([&](IMidiTransport& t) {
            return t.sendNoteOn(channel, note, velocity);
        });
    }

    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        return dispatch([&](IMidiTransport& t) {
            return t.sendNoteOff(channel, note, velocity);
        });
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
    template <typename SendFn>
    bool dispatch(SendFn&& send) {
        const bool primaryMounted = primary_.mounted();
        const bool wasStalled = primaryStalled();

        // A stalled primary is still probed on every call so it can recover as
        // soon as the host starts draining the endpoint again.
        const bool primaryResult = primaryMounted ? send(primary_) : false;
        if (primaryMounted) {
            if (primaryResult) {
                diagnostics_.primaryConsecutiveRejects = 0;
            } else if (diagnostics_.primaryConsecutiveRejects <
                       kPrimaryStallRejects) {
                ++diagnostics_.primaryConsecutiveRejects;
                if (primaryStalled()) ++diagnostics_.primaryStallDemotions;
            }
        } else {
            diagnostics_.primaryConsecutiveRejects = 0;
        }

        const bool primaryAuthoritative =
            primaryMounted && !(wasStalled || primaryStalled());

        // Ownership invariant (B1): if the authoritative primary rejected this
        // exact message, the secondary must not send it. Otherwise the owner
        // records "unsent" while a DIN synth is already sounding the note.
        const bool skipSecondary = primaryAuthoritative && !primaryResult;

        bool secondaryResult = false;
        if (skipSecondary) {
            ++diagnostics_.secondarySkipped;
        } else if (secondaryEnabled_ && secondary_.mounted()) {
            secondaryResult = send(secondary_);
            if (!secondaryResult) ++diagnostics_.secondaryRejected;
        }

        if (primaryAuthoritative) return primaryResult;
        if (secondaryResult) ++diagnostics_.secondaryOnlyDelivered;
        return primaryResult || secondaryResult;
    }

    IMidiTransport& primary_;
    IMidiTransport& secondary_;
    Diagnostics diagnostics_{};
    bool secondaryEnabled_{false};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_TEE_MIDI_TRANSPORT_H
