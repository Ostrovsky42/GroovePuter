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
    // A compact non-zero tag keeps the 128-slot queue at its original size.
    // Zero denotes ORIGINAL/file-clock playback.
    uint16_t projectTransportEpoch{0};
    uint32_t generation{0};
    uint32_t publicationSequence{0};
};

inline constexpr uint16_t scheduledSmfMidiEventTransportEpochTag(
        uint32_t transportEpoch) {
    return transportEpoch == 0
        ? 0
        : static_cast<uint16_t>(((transportEpoch - 1u) % 65535u) + 1u);
}

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

inline constexpr bool scheduledSmfMidiEventTransportEpochIsCurrent(
        const ScheduledSmfMidiEvent& event,
        uint32_t currentTransportEpoch) {
    return event.projectTransportEpoch == 0 ||
           event.projectTransportEpoch ==
               scheduledSmfMidiEventTransportEpochTag(currentTransportEpoch);
}

static_assert(sizeof(ScheduledSmfMidiEvent) == 20,
              "SMF event must retain its DRAM-neutral packed layout");
