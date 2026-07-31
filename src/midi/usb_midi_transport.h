#pragma once
#ifndef GROOVEPUTER_USB_MIDI_TRANSPORT_H
#define GROOVEPUTER_USB_MIDI_TRANSPORT_H

#include <cstdint>

// Hardware-independent boundary for class-compliant USB-MIDI device output.
// Implementations must be non-blocking on the musical-event path.
class IUsbMidiTransport {
public:
    virtual ~IUsbMidiTransport() = default;

    virtual bool begin() = 0;
    virtual bool mounted() const = 0;

    virtual bool sendNoteOn(uint8_t zeroBasedChannel,
                            uint8_t note,
                            uint8_t velocity) = 0;
    virtual bool sendNoteOff(uint8_t zeroBasedChannel,
                             uint8_t note,
                             uint8_t velocity) = 0;

    // Strict System Real-Time surface: callers cannot use this interface as an
    // arbitrary System Common/SysEx escape hatch. Default false implementations
    // preserve non-transport test doubles while hardware transports opt in.
    virtual bool sendTimingClock() { return false; }
    virtual bool sendStart() { return false; }
    virtual bool sendStop() { return false; }

    virtual void flush() = 0;
};

#endif  // GROOVEPUTER_USB_MIDI_TRANSPORT_H
