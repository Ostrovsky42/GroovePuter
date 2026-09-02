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
    // A mounted endpoint is not necessarily a working one. A USB host that
    // enumerates the device but never drains the IN endpoint - a computer with
    // no open MIDI port, or SEQTRAK against a composite build - fills the TX
    // FIFO after about sixteen packets and then rejects everything, while
    // still reporting mounted.
    //
    // Treating "mounted" as "authoritative" let that dead wire veto the other
    // one: every send was reported failed, the owner rolled the note back, and
    // the DIN endpoint went silent because of the state of a completely
    // different cable.
    //
    // Normal backpressure clears within the caller's retry window, so the
    // threshold sits far above a burst and far below a stall.
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

    // True when at least one wire can take traffic, so the DIN endpoint keeps
    // playing with no USB host attached.
    bool mounted() const override {
        return primary_.mounted() || (secondaryEnabled_ && secondary_.mounted());
    }

    // True while the primary is enumerated but has stopped accepting traffic.
    bool primaryStalled() const {
        return diagnostics_.primaryConsecutiveRejects >= kPrimaryStallRejects;
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
        const bool wasStalled = primaryStalled();

        // Keep offering traffic to a stalled primary: that is how it comes
        // back when the host finally opens the port.
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

        // The primary keeps its authority only while it is actually working.
        // Once demoted, a dead wire can no longer silence a live one.
        const bool primaryAuthoritative =
            primaryMounted && !(wasStalled || primaryStalled());

        // The result this call returns is what the owner records. If an
        // authoritative primary refused the message, the owner will treat it as
        // never sent - so the secondary must not send it either. Otherwise a
        // single transient USB backpressure event leaves a note sounding on the
        // DIN synth that no owner knows about, and on the SMF path its retries
        // stack duplicate NoteOn messages on that wire.
        //
        // This is what makes "both wires receive identical traffic" true rather
        // than aspirational.
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
