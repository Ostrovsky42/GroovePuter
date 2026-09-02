#pragma once
#ifndef GROOVEPUTER_MIDI_TRANSPORT_H
#define GROOVEPUTER_MIDI_TRANSPORT_H

#include <cstdint>

// How much the transport can actually know about the far end.
//
// This is not cosmetic. USB enumerates: the device knows whether a host is
// attached, so `linked()` is a fact. A DIN wire is write-only and offers no
// feedback whatsoever - an unplugged cable, a powered-off synth and a healthy
// link all look like a successful UART write. A UI that renders "DIN READY"
// would be asserting something the firmware cannot observe.
enum class MidiTransportLink : uint8_t {
    // `linked()` reflects an observed connection.
    Enumerated = 0,
    // `linked()` only reflects that the port was opened. Report this endpoint
    // as enabled or sending, never as ready or connected.
    Unverifiable = 1,
};

// Hardware-independent boundary for MIDI output. Implementations must be
// non-blocking on the musical-event path.
class IMidiTransport {
public:
    virtual ~IMidiTransport() = default;

    virtual bool begin() = 0;

    // True when traffic can be handed to this transport. For Enumerated links
    // this means a host is attached; for Unverifiable links it means the port
    // is open and nothing more. Use linkKind() before presenting this to a
    // user as connection state.
    virtual bool mounted() const = 0;

    virtual MidiTransportLink linkKind() const {
        return MidiTransportLink::Enumerated;
    }

    virtual bool sendNoteOn(uint8_t zeroBasedChannel,
                            uint8_t note,
                            uint8_t velocity) = 0;
    virtual bool sendNoteOff(uint8_t zeroBasedChannel,
                             uint8_t note,
                             uint8_t velocity) = 0;

    // Optional channel-mode surface used only for terminal recovery. CC123
    // lets the SMF player clear a remote synth after backpressure made the
    // exact NoteOff ownership state unrecoverable.
    virtual bool sendControlChange(uint8_t zeroBasedChannel,
                                   uint8_t controller,
                                   uint8_t value) {
        (void)zeroBasedChannel;
        (void)controller;
        (void)value;
        return false;
    }

    // Strict System Real-Time surface: callers cannot use this interface as an
    // arbitrary System Common/SysEx escape hatch. Default false implementations
    // preserve non-transport test doubles while hardware transports opt in.
    virtual bool sendTimingClock() { return false; }
    virtual bool sendStart() { return false; }
    virtual bool sendContinue() { return false; }
    virtual bool sendStop() { return false; }

    // MIDI Song Position Pointer uses sixteenth-note units (MIDI beats) and a
    // 14-bit payload encoded on the wire as F2 LSB MSB. It is deliberately
    // optional: device profiles must enable it only when the complete dispatch
    // path and receiver behavior are known to be safe.
    virtual bool sendSongPositionPointer(uint16_t midiBeats) {
        (void)midiBeats;
        return false;
    }

    virtual void flush() = 0;
};

#endif  // GROOVEPUTER_MIDI_TRANSPORT_H
