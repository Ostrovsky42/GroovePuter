#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_QUEUE_H
#define GROOVEPUTER_MIDI_INPUT_QUEUE_H

#include <cstddef>
#include <cstdint>

#include "midi_input_message.h"
#include "../midi/midi_realtime_word.h"

// Single-producer/single-consumer handoff for normalized live MIDI input.
// Physical transports must serialize publication through one producer owner;
// the queue itself has no USB/UART/BLE knowledge and never allocates or blocks.
//
// 64 storage slots / 63 usable messages is intentional: the current Cardputer
// USB dispatcher drains at most 32 packets per pass, so one complete USB burst
// can be admitted without an immediate capacity-edge drop while still keeping
// the future instantiated object well below 1 KiB.
class MidiInputQueue {
public:
    static constexpr std::size_t kStorageSize = 64u;
    static constexpr std::size_t kCapacity = kStorageSize - 1u;

    bool tryPush(const NormalizedMidiInputMessage& message) {
        if (!message.isValid()) {
            rejectedInvalid_.incrementRelaxed();
            return false;
        }

        const uint32_t head = head_.loadRelaxed();
        const uint32_t next = (head + 1u) % kStorageSize;
        const uint32_t tail = tail_.loadAcquire();
        if (next == tail) {
            droppedOverflow_.incrementRelaxed();
            overflowEpoch_.incrementRelaxed();
            return false;
        }

        messages_[head] = message;
        head_.storeRelease(next);

        const uint32_t depth = next >= tail
            ? next - tail
            : static_cast<uint32_t>(kStorageSize) - tail + next;
        if (depth > highWaterMark_.loadRelaxed()) {
            highWaterMark_.storeRelaxed(depth);
        }
        return true;
    }

    bool tryPop(NormalizedMidiInputMessage& message) {
        const uint32_t tail = tail_.loadRelaxed();
        if (tail == head_.loadAcquire()) return false;

        message = messages_[tail];
        tail_.storeRelease((tail + 1u) % kStorageSize);
        return true;
    }

    std::size_t approximateSize() const {
        const uint32_t head = head_.loadAcquire();
        const uint32_t tail = tail_.loadAcquire();
        return head >= tail
            ? static_cast<std::size_t>(head - tail)
            : static_cast<std::size_t>(kStorageSize - tail + head);
    }

    bool empty() const {
        return head_.loadAcquire() == tail_.loadAcquire();
    }

    uint32_t droppedOverflowCount() const {
        return droppedOverflow_.loadRelaxed();
    }

    uint32_t rejectedInvalidCount() const {
        return rejectedInvalid_.loadRelaxed();
    }

    uint32_t overflowEpoch() const {
        return overflowEpoch_.loadAcquire();
    }

    uint32_t highWaterMark() const {
        return highWaterMark_.loadRelaxed();
    }

    // Consumer-side recovery primitive for a later note-ownership stage. If an
    // overflow epoch changes, the consumer can discard all currently pending
    // messages and release its active input-note owners before continuing. This
    // prevents a future lost NoteOff from becoming a permanent stuck note.
    void discardPendingFromConsumer() {
        tail_.storeRelease(head_.loadAcquire());
    }

private:
    NormalizedMidiInputMessage messages_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord droppedOverflow_;
    MidiRealtimeWord rejectedInvalid_;
    MidiRealtimeWord overflowEpoch_;
    MidiRealtimeWord highWaterMark_;
};

static_assert(sizeof(MidiInputQueue) <= 800u,
              "MIDI input ingress queue must remain below the 800-byte R2 budget");

#endif  // GROOVEPUTER_MIDI_INPUT_QUEUE_H
