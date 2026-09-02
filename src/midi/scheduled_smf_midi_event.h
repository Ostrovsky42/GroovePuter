#pragma once

#include <cstdint>

#include "smf_track_output_route.h"

enum class ScheduledSmfMidiEventType : uint8_t {
    NoteOn = 0,
    NoteOff,
    SongPositionPointer,
};

struct ScheduledSmfMidiEvent {
    // Type needs two bits and SEQTRAK track identity needs six. Keeping both
    // in one byte preserves every value while avoiding four bytes of padding
    // in each element of the 128-slot realtime queue.
    ScheduledSmfMidiEventType type : 2;
    uint8_t trackIndex : 6; // source SMF track 0..63
    uint8_t channel{0};       // zero-based MIDI channel 0..15
    uint8_t note{0};          // MIDI data byte 0..127, or SPP low 7 bits
    uint8_t velocity{0};      // MIDI data byte 0..127, or SPP high 7 bits
    uint32_t blockSequence{0};
    uint16_t frameOffset{0};
    // A compact non-zero tag identifies PROJECT transport epochs. Zero denotes
    // ORIGINAL/file-clock playback and system-common transport intents.
    uint16_t projectTransportEpoch{0};
    // Low 28 bits are the scheduler queue generation. The high nibble carries
    // the per-track output-route revision captured at scheduling time. This
    // keeps live rerouting DRAM-neutral: the 128-slot realtime queue stays at
    // exactly 16 bytes per event.
    uint32_t generation{0};

    constexpr ScheduledSmfMidiEvent()
        : type(ScheduledSmfMidiEventType::NoteOn), trackIndex(0) {}
};

constexpr uint32_t kScheduledSmfQueueGenerationMask = 0x0FFFFFFFu;
constexpr uint8_t kScheduledSmfRouteRevisionShift = 28u;

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

inline constexpr uint32_t scheduledSmfMidiEventPackGeneration(
        uint32_t queueGeneration,
        uint8_t routeRevisionTag) {
    return (queueGeneration & kScheduledSmfQueueGenerationMask) |
           (static_cast<uint32_t>(routeRevisionTag & 0x0Fu)
            << kScheduledSmfRouteRevisionShift);
}

inline constexpr uint32_t scheduledSmfMidiEventQueueGeneration(
        const ScheduledSmfMidiEvent& event) {
    return event.generation & kScheduledSmfQueueGenerationMask;
}

inline constexpr uint8_t scheduledSmfMidiEventRouteRevisionTag(
        const ScheduledSmfMidiEvent& event) {
    return static_cast<uint8_t>(
        (event.generation >> kScheduledSmfRouteRevisionShift) & 0x0Fu);
}

inline constexpr bool scheduledSmfMidiEventQueueGenerationIsCurrent(
        const ScheduledSmfMidiEvent& event,
        uint32_t currentGeneration) {
    return scheduledSmfMidiEventQueueGeneration(event) ==
           (currentGeneration & kScheduledSmfQueueGenerationMask);
}

inline bool scheduledSmfMidiEventRouteRevisionIsCurrent(
        const ScheduledSmfMidiEvent& event) {
    if (event.type == ScheduledSmfMidiEventType::SongPositionPointer) {
        return true;
    }
    const uint8_t eventRevision =
        scheduledSmfMidiEventRouteRevisionTag(event);
    // Consumer-generated scoped cleanup has already removed the logical owner
    // from the bounded track table. It must survive another immediate reroute;
    // otherwise the second change could suppress the only physical NoteOff.
    if (eventRevision ==
        GroovePuterMidi::kSmfTrackOutputRouteRevisionCleanup) {
        return true;
    }
    return eventRevision ==
           GroovePuterMidi::smfTrackOutputRouteState()
               .revisionTagForRealtime(event.trackIndex);
}

inline bool scheduledSmfMidiEventGenerationIsCurrent(
        const ScheduledSmfMidiEvent& event,
        uint32_t currentGeneration) {
    return scheduledSmfMidiEventQueueGenerationIsCurrent(
               event, currentGeneration) &&
           scheduledSmfMidiEventRouteRevisionIsCurrent(event);
}

inline constexpr bool scheduledSmfMidiEventTransportEpochIsCurrent(
        const ScheduledSmfMidiEvent& event,
        uint32_t currentTransportEpoch) {
    return event.projectTransportEpoch == 0 ||
           event.projectTransportEpoch ==
               scheduledSmfMidiEventTransportEpochTag(currentTransportEpoch);
}

static_assert(sizeof(ScheduledSmfMidiEvent) == 16,
              "track-aware SMF event must retain its DRAM-neutral layout");
