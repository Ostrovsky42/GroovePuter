#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_QUEUE_H
#define GROOVEPUTER_MIDI_INPUT_QUEUE_H

#include <cstddef>
#include <cstdint>

#include "midi_input_event.h"
#include "midi_realtime_word.h"

namespace GroovePuterMidi {

// Single-producer/single-consumer input handoff. Attacks and controller updates
// may consume at most 48 slots. Release edges and channel cleanup retain 16
// positions; losing those requests recovery rather than leaving held state.
class MidiInputQueue {
public:
    static constexpr std::size_t kCapacity = 64;
    static constexpr std::size_t kCriticalReserve = 16;
    static constexpr std::size_t kNoteOnCapacity = kCapacity - kCriticalReserve;
    static constexpr std::size_t kRecoveryCapacity = 8;

    bool tryPush(const MidiInputEvent& event) {
        const uint32_t head = head_.loadRelaxed();
        const uint32_t tail = tail_.loadAcquire();
        const std::size_t used = occupied(head, tail);
        const bool droppable = isDroppable(event);
        if (droppable && used >= kNoteOnCapacity) {
            if (event.kind == InputKind::NoteOn) droppedNoteOn_.incrementRelaxed();
            else droppedOptional_.incrementRelaxed();
            return false;
        }
        const uint32_t next = advance(head);
        if (next == tail) {
            if (droppable) {
                if (event.kind == InputKind::NoteOn) droppedNoteOn_.incrementRelaxed();
                else droppedOptional_.incrementRelaxed();
            } else {
                droppedCritical_.incrementRelaxed();
                requestRecovery(event.id);
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
    bool popRecoverySession(InputSession& session) {
        const uint32_t tail = recoveryTail_.loadRelaxed();
        if (tail == recoveryHead_.loadAcquire()) return false;
        session = recovery_[tail & (kRecoveryCapacity - 1u)];
        recoveryTail_.storeRelease(tail + 1u);
        return true;
    }
    bool takeGlobalRecovery() {
        const uint32_t current = globalRecoveryEpoch_.loadAcquire();
        if (current == consumedGlobalRecoveryEpoch_) return false;
        consumedGlobalRecoveryEpoch_ = current;
        return true;
    }
    std::size_t approximateSize() const {
        return occupied(head_.loadAcquire(), tail_.loadAcquire());
    }
    uint32_t droppedNoteOnCount() const { return droppedNoteOn_.loadRelaxed(); }
    uint32_t droppedOptionalCount() const { return droppedOptional_.loadRelaxed(); }
    uint32_t droppedCriticalCount() const { return droppedCritical_.loadRelaxed(); }

private:
    void requestRecovery(const InputKey& input) {
        const uint32_t head = recoveryHead_.loadRelaxed();
        const uint32_t tail = recoveryTail_.loadAcquire();
        for (uint32_t cursor = tail; cursor != head; ++cursor) {
            const InputSession& pending = recovery_[cursor & (kRecoveryCapacity - 1u)];
            if (pending.source == input.source && pending.generation == input.generation) return;
        }
        if (head - tail >= kRecoveryCapacity) {
            globalRecoveryEpoch_.incrementRelaxed();
            return;
        }
        recovery_[head & (kRecoveryCapacity - 1u)] = InputSession{input.source, input.generation};
        recoveryHead_.storeRelease(head + 1u);
    }

    static bool isDroppable(const MidiInputEvent& event) {
        switch (event.kind) {
            case InputKind::NoteOn:
                return true;
            case InputKind::Mod:
                // A zero value is a release edge for button-style controller gestures.
                return event.velocity != 0u;
            case InputKind::PitchBend:
                // Button-style pitch controls spring back to the MIDI centre value.
                return event.velocity != 64u;
            case InputKind::Sustain:
                return event.velocity >= 64u;
            case InputKind::NoteOff:
            case InputKind::AllNotesOff:
            case InputKind::AllSoundOff:
                return false;
        }
        return false;
    }

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
    InputSession recovery_[kRecoveryCapacity]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord recoveryHead_;
    MidiRealtimeWord recoveryTail_;
    MidiRealtimeWord recoveryEpoch_;
    MidiRealtimeWord droppedNoteOn_;
    MidiRealtimeWord droppedOptional_;
    MidiRealtimeWord droppedCritical_;
    MidiRealtimeWord globalRecoveryEpoch_;
    uint32_t consumedGlobalRecoveryEpoch_{0};
    uint32_t consumedRecoveryEpoch_{0};
};

}  // namespace GroovePuterMidi

#endif  // GROOVEPUTER_MIDI_INPUT_QUEUE_H
