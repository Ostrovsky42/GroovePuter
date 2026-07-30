#pragma once

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
    virtual void flush() = 0;
};
