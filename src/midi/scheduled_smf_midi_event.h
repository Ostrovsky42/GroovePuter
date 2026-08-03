#pragma once

#include <cstdint>

enum class ScheduledSmfMidiEventType : uint8_t {
    NoteOn = 0,
    NoteOff,
    SongPositionPointer,
};

struct ScheduledSmfMidiEvent {
    ScheduledSmfMidiEventType type{ScheduledSmfMidiEventType::NoteOn};
    uint8_t channel{0};       // zero-based MIDI channel 0..15
    uint8_t note{0};          // MIDI data byte 0..127, or SPP low 7 bits
    uint8_t velocity{0};      // MIDI data byte 0..127, or SPP high 7 bits
    uint8_t trackIndex{0};    // source SMF track 0..63
    uint8_t reserved{0};
    uint32_t blockSequence{0};
    uint16_t frameOffset{0};
    // A compact non-zero tag identifies PROJECT transport epochs. Zero denotes
    // ORIGINAL/file-clock playback and system-common transport intents.
    uint16_t projectTransportEpoch{0};
    uint32_t generation{0};
    uint32_t publicationSequence{0};
};

inline constexpr uint16_t scheduledSmfSongPositionPointerValue(
        const ScheduledSmfMidiEvent& event) {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(event.velocity & 0x7Fu) << 7u) |
        static_cast<uint16_t>(event.note & 0x7Fu));
}

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

static_assert(sizeof(ScheduledSmfMidiEvent) == 24,
              "track-aware SMF event must retain a bounded packed layout");
