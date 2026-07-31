#pragma once
#ifndef GROOVEPUTER_MIDI_CONTROL_EVENT_QUEUE_H
#define GROOVEPUTER_MIDI_CONTROL_EVENT_QUEUE_H

#include <cstddef>
#include <cstdint>

#include "midi_realtime_word.h"
#include "../input/musical_event.h"

// Single-producer/single-consumer handoff from the Arduino control loop to the
// sole USB-MIDI owner task. Critical overflow degrades to a target-scoped live
// panic so a dropped NoteOff cannot leave a wire note active.
class MidiControlEventQueue {
public:
    static constexpr std::size_t kStorageSize = 32;
    static constexpr std::size_t kCapacity = kStorageSize - 1;
    static constexpr uint8_t kSynthAMask = 1u << 0;
    static constexpr uint8_t kSynthBMask = 1u << 1;
    static constexpr uint8_t kDrumsMask = 1u << 2;

    bool tryPush(const MusicalEvent& event) {
        const uint32_t head = head_.loadRelaxed();
        const uint32_t next = (head + 1u) % kStorageSize;
        if (next == tail_.loadAcquire()) {
            if (event.type == MusicalEventType::NoteOn) {
                droppedNoteOn_.incrementRelaxed();
            } else {
                droppedCritical_.incrementRelaxed();
                markPendingAllNotesOff(event.target);
            }
            return false;
        }

        events_[head] = event;
        head_.storeRelease(next);
        return true;
    }

    bool tryPop(MusicalEvent& event) {
        const uint32_t tail = tail_.loadRelaxed();
        if (tail == head_.loadAcquire()) return false;

        event = events_[tail];
        tail_.storeRelease((tail + 1u) % kStorageSize);
        return true;
    }

    uint8_t takePendingAllNotesOffMask() {
        uint8_t mask = 0;
        const uint32_t synthAEpoch = pendingSynthAEpoch_.loadAcquire();
        const uint32_t synthBEpoch = pendingSynthBEpoch_.loadAcquire();
        const uint32_t drumsEpoch = pendingDrumsEpoch_.loadAcquire();
        if (synthAEpoch != consumedSynthAEpoch_) {
            consumedSynthAEpoch_ = synthAEpoch;
            mask |= kSynthAMask;
        }
        if (synthBEpoch != consumedSynthBEpoch_) {
            consumedSynthBEpoch_ = synthBEpoch;
            mask |= kSynthBMask;
        }
        if (drumsEpoch != consumedDrumsEpoch_) {
            consumedDrumsEpoch_ = drumsEpoch;
            mask |= kDrumsMask;
        }
        return mask;
    }

    std::size_t approximateSize() const {
        const uint32_t head = head_.loadAcquire();
        const uint32_t tail = tail_.loadAcquire();
        return head >= tail
            ? static_cast<std::size_t>(head - tail)
            : static_cast<std::size_t>(kStorageSize - tail + head);
    }

    uint32_t droppedNoteOnCount() const {
        return droppedNoteOn_.loadRelaxed();
    }

    uint32_t droppedCriticalCount() const {
        return droppedCritical_.loadRelaxed();
    }

private:
    void markPendingAllNotesOff(MusicalEventTarget target) {
        switch (target) {
            case MusicalEventTarget::SynthA:
                pendingSynthAEpoch_.incrementRelaxed();
                break;
            case MusicalEventTarget::SynthB:
                pendingSynthBEpoch_.incrementRelaxed();
                break;
            case MusicalEventTarget::Drums:
                pendingDrumsEpoch_.incrementRelaxed();
                break;
        }
    }

    MusicalEvent events_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord pendingSynthAEpoch_;
    MidiRealtimeWord pendingSynthBEpoch_;
    MidiRealtimeWord pendingDrumsEpoch_;
    MidiRealtimeWord droppedNoteOn_;
    MidiRealtimeWord droppedCritical_;
    uint32_t consumedSynthAEpoch_{0};
    uint32_t consumedSynthBEpoch_{0};
    uint32_t consumedDrumsEpoch_{0};
};

#endif  // GROOVEPUTER_MIDI_CONTROL_EVENT_QUEUE_H
