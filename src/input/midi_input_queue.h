#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_QUEUE_H
#define GROOVEPUTER_MIDI_INPUT_QUEUE_H

#include <cstddef>
#include <cstdint>

#include "midi_input_message.h"
#include "../midi/midi_realtime_word.h"

// Fixed single-producer/single-consumer ingress between one serialized physical
// MIDI-input publication owner and the future MidiInputRouter. R2 only owns the
// handoff: overflow recovery/panic is deliberately deferred to lifecycle R5.
class MidiInputQueue {
public:
    static constexpr std::size_t kStorageSize = 32;
    static constexpr std::size_t kCapacity = kStorageSize - 1;

    bool tryPush(const NormalizedMidiInputMessage& message) {
        const uint32_t head = head_.loadRelaxed();
        const uint32_t next = (head + 1u) % kStorageSize;
        const uint32_t tail = tail_.loadAcquire();
        if (next == tail) {
            dropped_.incrementRelaxed();
            return false;
        }

        messages_[head] = message;
        head_.storeRelease(next);

        const uint32_t depth = next >= tail
            ? next - tail
            : static_cast<uint32_t>(kStorageSize) - tail + next;
        if (depth > maxObservedDepth_.loadRelaxed()) {
            maxObservedDepth_.storeRelaxed(depth);
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

    uint32_t droppedCount() const {
        return dropped_.loadRelaxed();
    }

    uint32_t maxObservedDepth() const {
        return maxObservedDepth_.loadRelaxed();
    }

private:
    NormalizedMidiInputMessage messages_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord dropped_;
    MidiRealtimeWord maxObservedDepth_;
};

static_assert(sizeof(MidiInputQueue) == 400,
              "R2 memory contract changed: re-audit ingress DRAM");

#endif  // GROOVEPUTER_MIDI_INPUT_QUEUE_H
