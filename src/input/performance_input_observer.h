#pragma once
#ifndef GROOVEPUTER_PERFORMANCE_INPUT_OBSERVER_H
#define GROOVEPUTER_PERFORMANCE_INPUT_OBSERVER_H

#include <cstdint>

// Read-only tap on the notes a person plays into PERFORM (built-in keys and
// external MIDI IN both pass through PerformanceKeyboard). It sees the input
// pitch before Smart Play / role interpretation and never changes what sounds.
// Called on the main loop only (the same context that owns PerformanceKeyboard),
// never from the audio task or the USB MIDI dispatcher.
class PerformanceInputObserver {
public:
    // tag identifies the physical source so a NoteOff finds its NoteOn:
    // a built-in key uses its normalized key character (< 0x80), an external
    // MIDI note uses performanceExternalInputTag().
    virtual void onPerformanceInputNote(bool on, uint16_t tag,
                                        uint8_t note, uint8_t velocity) = 0;

protected:
    ~PerformanceInputObserver() = default;
};

inline uint16_t performanceExternalInputTag(uint8_t source, uint8_t channel,
                                            uint8_t inputNote) {
    return static_cast<uint16_t>(0x8000u |
                                 (static_cast<uint16_t>(source & 0x03u) << 11) |
                                 (static_cast<uint16_t>(channel & 0x0fu) << 7) |
                                 (inputNote & 0x7fu));
}

#endif  // GROOVEPUTER_PERFORMANCE_INPUT_OBSERVER_H
