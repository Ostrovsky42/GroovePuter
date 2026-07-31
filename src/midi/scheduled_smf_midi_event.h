#pragma once

#include <cstdint>

enum class ScheduledSmfMidiEventType : uint8_t {
    NoteOn = 0,
    NoteOff,
};

struct ScheduledSmfMidiEvent {
    ScheduledSmfMidiEventType type{ScheduledSmfMidiEventType::NoteOn};
    uint8_t channel{0};       // zero-based MIDI channel 0..15
    uint8_t note{0};          // MIDI data byte 0..127
    uint8_t velocity{0};      // MIDI data byte 0..127
    uint32_t blockSequence{0};
    uint16_t frameOffset{0};
    uint32_t generation{0};
    uint32_t publicationSequence{0};
};

inline constexpr bool scheduledSmfMidiEventFrameIsValid(
        const ScheduledSmfMidiEvent& event,
        uint16_t blockFrames) {
    return blockFrames > 0 && event.frameOffset < blockFrames;
}

inline constexpr bool scheduledSmfMidiEventGenerationIsCurrent(
        const ScheduledSmfMidiEvent& event,
        uint32_t currentGeneration) {
    return event.generation == currentGeneration;
}
