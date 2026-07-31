#pragma once
#ifndef GROOVEPUTER_PATTERN_DRUM_GATE_SCHEDULER_H
#define GROOVEPUTER_PATTERN_DRUM_GATE_SCHEDULER_H

#include <cstddef>
#include <cstdint>

#include "scheduled_musical_event.h"

// MidiDispatchTask-local scheduler. There is no second task and no wall-clock
// timer: each gate deadline is derived from the original sample-timed Pattern
// NoteOn position. One slot per internal drum voice is sufficient because a
// retrigger extends that voice's gate; it must not leave an older NoteOff that
// can cut the retrigger short.
class PatternDrumGateScheduler {
public:
    static constexpr uint8_t kVoiceCount = 8;

    bool scheduleOrExtend(const ScheduledMusicalEvent& noteOn,
                          uint16_t gateMs,
                          uint32_t sampleRate,
                          uint16_t blockFrames) {
        if (noteOn.event.source != MusicalEventSource::PatternPlayer ||
            noteOn.event.target != MusicalEventTarget::Drums ||
            noteOn.event.type != MusicalEventType::NoteOn ||
            noteOn.event.channel >= kVoiceCount ||
            sampleRate == 0 || blockFrames == 0) {
            return false;
        }

        uint32_t gateFrames =
            (static_cast<uint64_t>(sampleRate) * gateMs + 999u) / 1000u;
        if (gateFrames == 0) gateFrames = 1;

        const uint64_t absoluteFrame =
            static_cast<uint64_t>(noteOn.frameOffset) + gateFrames;
        ScheduledMusicalEvent gate{};
        gate.event = MusicalEvent{
            MusicalEventType::NoteOff,
            MusicalEventSource::PatternPlayer,
            MusicalEventTarget::Drums,
            noteOn.event.channel,
            noteOn.event.note,
            0,
        };
        gate.blockSequence = noteOn.blockSequence +
            static_cast<uint32_t>(absoluteFrame / blockFrames);
        gate.frameOffset = static_cast<uint16_t>(absoluteFrame % blockFrames);
        gate.generation = noteOn.generation;
        gate.publicationSequence = noteOn.publicationSequence;

        Slot& slot = slots_[noteOn.event.channel];
        slot.active = true;
        slot.gate = gate;
        return true;
    }

    bool peekEarliest(ScheduledMusicalEvent& gate) const {
        int best = -1;
        for (uint8_t i = 0; i < kVoiceCount; ++i) {
            if (!slots_[i].active) continue;
            if (best < 0 || scheduledMusicalEventBefore(
                    slots_[i].gate, slots_[static_cast<uint8_t>(best)].gate)) {
                best = i;
            }
        }
        if (best < 0) return false;
        gate = slots_[static_cast<uint8_t>(best)].gate;
        return true;
    }

    void consume(uint8_t logicalVoice) {
        if (logicalVoice >= kVoiceCount) return;
        slots_[logicalVoice].active = false;
        slots_[logicalVoice].gate = ScheduledMusicalEvent{};
    }

    void clear() {
        for (auto& slot : slots_) {
            slot.active = false;
            slot.gate = ScheduledMusicalEvent{};
        }
    }

    bool active(uint8_t logicalVoice) const {
        return logicalVoice < kVoiceCount && slots_[logicalVoice].active;
    }

    std::size_t activeCount() const {
        std::size_t count = 0;
        for (const auto& slot : slots_) {
            if (slot.active) ++count;
        }
        return count;
    }

private:
    struct Slot {
        bool active{false};
        ScheduledMusicalEvent gate{};
    };

    Slot slots_[kVoiceCount]{};
};

#endif  // GROOVEPUTER_PATTERN_DRUM_GATE_SCHEDULER_H
