#pragma once
#ifndef GROOVEPUTER_MIDI_EXTERNAL_NOTE_QUEUE_H
#define GROOVEPUTER_MIDI_EXTERNAL_NOTE_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "midi_input_dispatcher.h"

namespace GroovePuterMidi {

// Single-producer (MidiDispatchTask, through MidiInputDispatcher) / single-consumer (the UI loop,
// which owns the PERFORM keyboard) queue of external-keyboard notes. Bounded, no allocation.
// NoteOn may use only the first kCapacity - kNoteOffReserve slots; NoteOff may use all of them.
// A NoteOff that still does not fit raises a recovery request: the consumer then releases every
// external note instead of leaving a stuck one.
class ExternalNoteQueue final : public MidiExternalNoteSink {
public:
    static constexpr std::size_t kCapacity = 32;  // power of two
    static constexpr std::size_t kNoteOffReserve = 8;

    struct Event {
        bool on{false};
        uint8_t note{0};
        uint8_t velocity{0};
        uint32_t stampUs{0};  // producer time when a clock is set (acceptance diagnostics), else 0
    };

    // Optional microsecond clock (diagnostics only): events carry the push time.
    void setClock(uint32_t (*clock)()) { clock_ = clock; }

    void externalNoteOn(uint8_t note, uint8_t velocity) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (used >= kCapacity - kNoteOffReserve) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        push(head, Event{true, note, velocity, clock_ ? clock_() : 0u});
    }

    void externalNoteOff(uint8_t note) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) >= kCapacity) {
            recovery_.store(true, std::memory_order_release);
            return;
        }
        push(head, Event{false, note, 0, clock_ ? clock_() : 0u});
    }

    bool pop(Event& out) {
        const uint32_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        out = ring_[tail & (kCapacity - 1)];
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // True once after a NoteOff was lost: the consumer must release all external notes.
    bool takeRecovery() { return recovery_.exchange(false, std::memory_order_acq_rel); }
    uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    void push(uint32_t head, const Event& event) {
        ring_[head & (kCapacity - 1)] = event;
        head_.store(head + 1, std::memory_order_release);
    }

    Event ring_[kCapacity]{};
    std::atomic<uint32_t> head_{0}, tail_{0}, dropped_{0};
    std::atomic<bool> recovery_{false};
    uint32_t (*clock_)(){nullptr};
};

}  // namespace GroovePuterMidi

#endif
