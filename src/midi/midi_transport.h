#pragma once
#ifndef GROOVEPUTER_MIDI_TRANSPORT_H
#define GROOVEPUTER_MIDI_TRANSPORT_H

#include <cstdint>

enum class MidiTransportLink : uint8_t {
    Enumerated = 0,
    Unverifiable = 1,
};

class IMidiTransport {
public:
    virtual ~IMidiTransport() = default;
    virtual bool begin() = 0;
    virtual bool mounted() const = 0;
    virtual MidiTransportLink linkKind() const { return MidiTransportLink::Enumerated; }
    virtual bool sendNoteOn(uint8_t zeroBasedChannel, uint8_t note, uint8_t velocity) = 0;
    virtual bool sendNoteOff(uint8_t zeroBasedChannel, uint8_t note, uint8_t velocity) = 0;
    virtual bool sendControlChange(uint8_t zeroBasedChannel, uint8_t controller, uint8_t value) {
        (void)zeroBasedChannel; (void)controller; (void)value; return false;
    }
    virtual bool sendTimingClock() { return false; }
    virtual bool sendStart() { return false; }
    virtual bool sendContinue() { return false; }
    virtual bool sendStop() { return false; }
    virtual bool sendSongPositionPointer(uint16_t midiBeats) { (void)midiBeats; return false; }
    virtual void flush() = 0;
};

#endif  // GROOVEPUTER_MIDI_TRANSPORT_H
