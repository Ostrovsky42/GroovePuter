#pragma once
#ifndef GROOVEPUTER_SCHEDULED_MIDI_TRANSPORT_EVENT_H
#define GROOVEPUTER_SCHEDULED_MIDI_TRANSPORT_EVENT_H

#include <cstdint>

#include "scheduled_musical_event.h"

enum class MidiTransportEventType : uint8_t {
    Clock,
    Start,
    Continue,
    Stop,
};

// Sample-timed MIDI transport event. Transport events deliberately do not reuse
// MusicalEvent/MusicalEventTarget: MIDI Clock and Start/Continue/Stop are system
// realtime messages with their own lifecycle and overflow semantics.
struct ScheduledMidiTransportEvent {
    MidiTransportEventType type{MidiTransportEventType::Clock};
    uint32_t blockSequence{0};
    uint16_t frameOffset{0};
    uint32_t generation{0};
    uint32_t publicationSequence{0};
};

inline constexpr uint8_t midiTransportEventPriority(
        MidiTransportEventType type) {
    switch (type) {
        case MidiTransportEventType::Start:
        case MidiTransportEventType::Continue:
        case MidiTransportEventType::Stop:
            return 0;
        case MidiTransportEventType::Clock:
            return 1;
    }
    return 2;
}

inline constexpr bool scheduledMidiTransportEventBefore(
        const ScheduledMidiTransportEvent& lhs,
        const ScheduledMidiTransportEvent& rhs) {
    if (lhs.blockSequence != rhs.blockSequence) {
        return midiSequenceBefore(lhs.blockSequence, rhs.blockSequence);
    }
    if (lhs.frameOffset != rhs.frameOffset) {
        return lhs.frameOffset < rhs.frameOffset;
    }
    const uint8_t lhsPriority = midiTransportEventPriority(lhs.type);
    const uint8_t rhsPriority = midiTransportEventPriority(rhs.type);
    if (lhsPriority != rhsPriority) return lhsPriority < rhsPriority;
    if (lhs.publicationSequence != rhs.publicationSequence) {
        return midiSequenceBefore(lhs.publicationSequence,
                                  rhs.publicationSequence);
    }
    return false;
}

// Cross-queue scheduling contract used by the dispatcher: a transport event
// precedes a musical event when its sample timestamp is earlier, and transport
// owns an exactly equal sample timestamp. Within transport traffic the helper
// above keeps lifecycle messages ahead of Clock.
inline constexpr bool scheduledMidiTransportEventBeforeMusical(
        const ScheduledMidiTransportEvent& transport,
        const ScheduledMusicalEvent& musical) {
    if (transport.blockSequence != musical.blockSequence) {
        return midiSequenceBefore(transport.blockSequence,
                                  musical.blockSequence);
    }
    if (transport.frameOffset != musical.frameOffset) {
        return transport.frameOffset < musical.frameOffset;
    }
    return true;
}

inline constexpr bool scheduledMidiTransportEventFrameIsValid(
        const ScheduledMidiTransportEvent& scheduled,
        uint16_t blockFrames) {
    return blockFrames > 0 && scheduled.frameOffset < blockFrames;
}

inline constexpr bool scheduledMidiTransportEventGenerationIsCurrent(
        const ScheduledMidiTransportEvent& scheduled,
        uint32_t currentGeneration) {
    return scheduled.generation == currentGeneration;
}

#endif  // GROOVEPUTER_SCHEDULED_MIDI_TRANSPORT_EVENT_H
