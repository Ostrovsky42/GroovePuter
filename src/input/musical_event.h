#pragma once
#ifndef GROOVEPUTER_MUSICAL_EVENT_H
#define GROOVEPUTER_MUSICAL_EVENT_H

#include <cstdint>

enum class MusicalEventType : uint8_t {
    NoteOn,
    NoteOff,
    AllNotesOff,
    PitchBend,
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
// One note of a Melody chord. Unlike the other hints it is carried by NoteOn
// AND NoteOff: the sink keeps chord notes in their own per-note set instead of
// the one-note Pattern lane, so a NoteOff must say which set it belongs to.
constexpr uint8_t kMusicalEventChord = 1u << 1;
// CALL/RESPONSE answer lane: channel is the zero-based MIDI wire channel the
// partner chose (not a logical lane), and the note belongs to the answer's own
// one-note lane in the USB MIDI output. Carried by NoteOn AND NoteOff, so a NoteOff
// finds the channel of its NoteOn. Internal synth outputs ignore it (the answer
// targets Dx, which is external-only).
constexpr uint8_t kMusicalEventWireChannel = 1u << 2;
// Partner owns a distinct voice while following the configured DX channel.
constexpr uint8_t kMusicalEventAnswerFollowDx = 1u << 3;

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
    uint16_t pitchBend14{8192};
};

#endif  // GROOVEPUTER_MUSICAL_EVENT_H
