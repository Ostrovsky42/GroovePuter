#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_QUEUE_H
#define GROOVEPUTER_MIDI_INPUT_QUEUE_H

#include <cstddef>
#include <cstdint>

#include "midi_input_event.h"
#include "midi_realtime_word.h"

namespace GroovePuterMidi {

// Single-producer/single-consumer input handoff. Fresh attacks may consume at
// most 48 slots, preserving 16 positions for the releases that prevent stuck
// notes. If even that reserve is exhausted, recovery is a separate flag rather
// than another queued event.
class MidiInputQueue {
public:
    static constexpr std::size_t kCapacity = 64;
    static constexpr std::size_t kCriticalReserve = 16;
    static constexpr std::size_t kNoteOnCapacity = kCapacity - kCriticalReserve;

    bool tryPush(const MidiInputEvent& event) {
        const uint32_t head = head_.loadRelaxed();
        const uint32_t tail = tail_.loadAcquire();
        const std::size_t used = occupied(head, tail);
        if (event.kind == InputKind::NoteOn && used >= kNoteOnCapacity) {
            droppedNoteOn_.incrementRelaxed();
            return false;
        }
        const uint32_t next = advance(head);
        if (next == tail) {
            if (event.kind == InputKind::NoteOn) {
                droppedNoteOn_.incrementRelaxed();
            } else {
                droppedCritical_.incrementRelaxed();
                recoveryEpoch_.incrementRelaxed();
            }
            return false;
        }
        events_[head] = event;
        head_.storeRelease(next);
        return true;
    }

    bool tryPop(MidiInputEvent& event) {
        const uint32_t tail = tail_.loadRelaxed();
        if (tail == head_.loadAcquire()) return false;
        event = events_[tail];
        tail_.storeRelease(advance(tail));
        return true;
    }

    bool recoveryPending() const {
        return recoveryEpoch_.loadAcquire() != consumedRecoveryEpoch_;
    }
    bool takeRecoveryPending() {
        const uint32_t current = recoveryEpoch_.loadAcquire();
        if (current == consumedRecoveryEpoch_) return false;
        consumedRecoveryEpoch_ = current;
        return true;
    }
    std::size_t approximateSize() const {
        return occupied(head_.loadAcquire(), tail_.loadAcquire());
    }
    uint32_t droppedNoteOnCount() const { return droppedNoteOn_.loadRelaxed(); }
    uint32_t droppedCriticalCount() const { return droppedCritical_.loadRelaxed(); }

private:
    static constexpr uint32_t kStorageSize = static_cast<uint32_t>(kCapacity + 1u);

    static constexpr uint32_t advance(uint32_t index) {
        return (index + 1u) % kStorageSize;
    }
    static constexpr std::size_t occupied(uint32_t head, uint32_t tail) {
        return head >= tail
            ? static_cast<std::size_t>(head - tail)
            : static_cast<std::size_t>(kStorageSize - tail + head);
    }

    MidiInputEvent events_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord recoveryEpoch_;
    MidiRealtimeWord droppedNoteOn_;
    MidiRealtimeWord droppedCritical_;
    uint32_t consumedRecoveryEpoch_{0};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_INPUT_QUEUE_H
