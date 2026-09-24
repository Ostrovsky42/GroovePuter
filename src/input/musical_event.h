#pragma once
#ifndef GROOVEPUTER_MUSICAL_EVENT_H
#define GROOVEPUTER_MUSICAL_EVENT_H

#include <cstdint>

enum class MusicalEventType : uint8_t {
    NoteOn,
    NoteOff,
    AllNotesOff,
};

enum class MusicalEventSource : uint8_t {
    PerformanceKeyboard,
    PatternPlayer,
    Arpeggiator,
    MidiInput,
    // Direct manual POLY mode. This source is external-MIDI-only: the
    // internal Synth A/B engines remain intentionally monophonic.
    PerformanceKeyboardPoly,
};

enum class MusicalEventTarget : uint8_t {
    SynthA,
    SynthB,
    Drums,
    Dx,
};

// Articulation hints carried by a NoteOn. Sinks that do not understand them
// ignore the byte; NoteOff and AllNotesOff never carry them.
constexpr uint8_t kMusicalEventSlide = 1u << 0;

// channel is a zero-based logical channel. Internal synth outputs ignore it.
// USB MIDI synth targets map to configured physical channels. For Drums,
// channel selects the logical live lane or Pattern drum voice consumed by the
// target-specific MIDI routing layer.
struct MusicalEvent {
    MusicalEventType type{MusicalEventType::AllNotesOff};
    MusicalEventSource source{MusicalEventSource::PerformanceKeyboard};
    MusicalEventTarget target{MusicalEventTarget::SynthA};
    uint8_t channel{0};
    uint8_t note{0};
    uint8_t velocity{0};
    uint8_t flags{0};
};

#endif  // GROOVEPUTER_MUSICAL_EVENT_H
