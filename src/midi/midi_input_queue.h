#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_QUEUE_H
#define GROOVEPUTER_MIDI_INPUT_QUEUE_H

#include <cstddef>
#include <cstdint>

#include "midi_input_event.h"
#include "midi_latest_pitch_bend.h"
#include "midi_realtime_word.h"

namespace GroovePuterMidi {

// Single-producer/single-consumer input handoff. Note attacks and droppable
// expression/control streams may consume at most 48 slots, preserving 16
// positions for critical releases. If that reserve is exhausted, recovery is
// a separate flag rather than another queued event.
class MidiInputQueue {
public:
    static constexpr std::size_t kCapacity = 64;
    static constexpr std::size_t kCriticalReserve = 16;
    static constexpr std::size_t kNoteOnCapacity = kCapacity - kCriticalReserve;

    bool tryPush(const MidiInputEvent& event) {
        const uint32_t head = head_.loadRelaxed();
        const uint32_t tail = tail_.loadAcquire();
        const std::size_t used = occupied(head, tail);
        if (used >= kNoteOnCapacity && isDroppable(event.kind)) {
            recordDropped(event.kind);
            if (event.kind == InputKind::PitchBend) coalescePitchBend(event);
            return false;
        }
        const uint32_t next = advance(head);
        if (next == tail) {
            if (isDroppable(event.kind)) {
                // Expression/control streams can shed stale updates. They
                // must not consume release reserve or trigger owner recovery.
                recordDropped(event.kind);
                if (event.kind == InputKind::PitchBend) coalescePitchBend(event);
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
        if (tail == head_.loadAcquire()) {
            LatestPitchBend bend{};
            if (!pendingPitchBends_.takeNext(bend)) return false;
            event = MidiInputEvent{};
            event.id = InputKey{bend.source, bend.generation, bend.channel, 0, bend.cable};
            event.kind = InputKind::PitchBend;
            event.pitchBend14 = bend.value14;
            return true;
        }
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
    uint32_t droppedPitchBendCount() const { return droppedPitchBend_.loadRelaxed(); }
    uint32_t droppedControlCount() const { return droppedControl_.loadRelaxed(); }
    uint32_t droppedCriticalCount() const { return droppedCritical_.loadRelaxed(); }

private:
    void coalescePitchBend(const MidiInputEvent& event) {
        pendingPitchBends_.publish(event.id.source, event.id.generation,
                                   event.id.channel, event.pitchBend14, event.id.cable);
    }

    static constexpr uint32_t kStorageSize = static_cast<uint32_t>(kCapacity + 1u);

    static constexpr bool isDroppable(InputKind kind) {
        return kind == InputKind::NoteOn || kind == InputKind::PitchBend ||
               kind == InputKind::Mod || kind == InputKind::ControlChange;
    }

    void recordDropped(InputKind kind) {
        if (kind == InputKind::NoteOn) droppedNoteOn_.incrementRelaxed();
        else if (kind == InputKind::PitchBend) droppedPitchBend_.incrementRelaxed();
        else droppedControl_.incrementRelaxed();
    }

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
    MidiRealtimeWord droppedPitchBend_;
    MidiRealtimeWord droppedControl_;
    MidiRealtimeWord droppedCritical_;
    MidiLatestPitchBendStore pendingPitchBends_;
    uint32_t consumedRecoveryEpoch_{0};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_INPUT_QUEUE_H
