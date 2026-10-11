#pragma once
#ifndef GROOVEPUTER_MIDI_EXTERNAL_NOTE_QUEUE_H
#define GROOVEPUTER_MIDI_EXTERNAL_NOTE_QUEUE_H

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "src/input/performance_intent.h"
#include "midi_input_dispatcher.h"
#include "midi_latest_pitch_bend.h"

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
    static constexpr int8_t kNudgeEnd = 2;  // Event::nudge value for "the pitch button was released"

    struct Event {
        bool on{false};       // NoteOn, or sustain pressed when `sustain` is set
        bool sustain{false};  // sustain event (note and velocity unused)
        int8_t nudge{0};      // pitch-button nudge (-1 / +1); note, velocity and sustain unused
        uint8_t mod{0};       // ModPhase (1 press, 2 release, 3 cancel); everything else unused
        uint8_t note{0};
        uint8_t velocity{0};
        uint32_t stampUs{0};  // producer time when a clock is set (acceptance diagnostics), else 0
        PerformanceInputIdentity input{};  // NOTE ownership; empty for non-note controls
        uint8_t controller{0};
        bool controlChange{false};
        bool pitchBendControl{false};
        uint16_t pitchBend14{8192};
        bool allSoundOffControl{false};
    };

    // Optional microsecond clock (diagnostics only): events carry the push time.
    void setClock(uint32_t (*clock)()) { clock_ = clock; }

    void externalNoteOn(uint8_t note, uint8_t velocity, PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (used >= kCapacity - kNoteOffReserve) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        Event event{true, false, 0, 0, note, velocity, clock_ ? clock_() : 0u};
        event.input = input;
        push(head, event);
    }

    void externalNoteOn(uint8_t note, uint8_t velocity) {
        externalNoteOn(note, velocity, PerformanceInputIdentity{0, 0, 0, note});
    }

    void externalNoteOff(uint8_t note, PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) >= kCapacity) {
            recovery_.store(true, std::memory_order_release);
            return;
        }
        Event event{false, false, 0, 0, note, 0, clock_ ? clock_() : 0u};
        event.input = input;
        push(head, event);
    }

    void externalNoteOff(uint8_t note) {
        externalNoteOff(note, PerformanceInputIdentity{0, 0, 0, note});
    }

    // Pressing is droppable like a NoteOn; releasing is critical like a NoteOff.
    void externalSustain(bool down) override {
        externalSustain(down, PerformanceInputIdentity{});
    }

    void externalSustain(bool down, PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (down) {
            if (used >= kCapacity - kNoteOffReserve) {
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        } else if (used >= kCapacity) {
            recovery_.store(true, std::memory_order_release);
            return;
        }
        Event event{down, true, 0, 0, 0, 0, clock_ ? clock_() : 0u};
        event.input = input;
        push(head, event);
    }

    // A press (-1 / +1) is droppable like a NoteOn. The release (0) is critical like a NoteOff: losing it
    // would leave the auto-repeat running, so it uses the reserve and raises the recovery request.
    void externalNudge(int direction) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (direction == 0) {
            if (used >= kCapacity) {
                recovery_.store(true, std::memory_order_release);
                return;
            }
            push(head, Event{false, false, kNudgeEnd, 0, 0, 0, clock_ ? clock_() : 0u});
            return;
        }
        if (used >= kCapacity - kNoteOffReserve) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        push(head, Event{false, false, static_cast<int8_t>(direction < 0 ? -1 : 1), 0, 0, 0,
                         clock_ ? clock_() : 0u});
    }

    // A press is droppable like a NoteOn (a lost press is a button that did nothing). A release or a
    // cancel is critical like a NoteOff: losing it would leave the consumer waiting on a held button,
    // so it uses the reserve and raises the recovery request when even that is full.
    void externalMod(ModPhase phase) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (phase == ModPhase::Press) {
            if (used >= kCapacity - kNoteOffReserve) {
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        } else if (used >= kCapacity) {
            recovery_.store(true, std::memory_order_release);
            return;
        }
        push(head, Event{false, false, 0, static_cast<uint8_t>(phase), 0, 0, clock_ ? clock_() : 0u});
    }

    void externalControlChange(uint8_t controller, uint8_t value,
                               PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (used >= kCapacity - kNoteOffReserve) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        Event event{false, false, 0, 0, 0, value, clock_ ? clock_() : 0u};
        event.input = input;
        event.controller = controller;
        event.controlChange = true;
        push(head, event);
    }

    // Pitch bend is continuous and may be dropped under pressure; unlike a NoteOff,
    // losing one update must never trigger note recovery or a panic.
    void externalPitchBend(uint16_t value14, PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (used >= kCapacity - kNoteOffReserve) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            pendingPitchBends_.publish(static_cast<InputSource>(input.source),
                                       input.generation, input.channel, value14,
                                       input.cable);
            return;
        }
        Event event{false, false, 0, 0, 0, 0, clock_ ? clock_() : 0u};
        event.input = input;
        event.pitchBendControl = true;
        event.pitchBend14 = value14 > 16383u ? 16383u : value14;
        push(head, event);
    }

    void externalAllSoundOff(PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) >= kCapacity) {
            recovery_.store(true, std::memory_order_release);
            return;
        }
        Event event{false, false, 0, 0, 0, 0, clock_ ? clock_() : 0u};
        event.input = input;
        event.allSoundOffControl = true;
        push(head, event);
    }

    bool pop(Event& out) {
        const uint32_t tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) {
            LatestPitchBend bend{};
            if (!pendingPitchBends_.takeNext(bend)) return false;
            out = Event{};
            out.input = PerformanceInputIdentity{
                bend.generation, static_cast<uint8_t>(bend.source), bend.channel,
                0, bend.cable};
            out.pitchBendControl = true;
            out.pitchBend14 = bend.value14;
            return true;
        }
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
    MidiLatestPitchBendStore pendingPitchBends_;
    uint32_t (*clock_)(){nullptr};
};

}  // namespace GroovePuterMidi

#endif
