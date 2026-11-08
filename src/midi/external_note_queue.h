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
// A lost identity-bearing release requests recovery for that source session. Legacy events without
// identity retain the conservative global external-note recovery behavior.
class ExternalNoteQueue final : public MidiExternalNoteSink {
public:
    static constexpr std::size_t kCapacity = 32;  // power of two
    static constexpr std::size_t kNoteOffReserve = 8;
    static constexpr std::size_t kRecoveryCapacity = 8;
    static constexpr std::size_t kControlCoalesceSlots = 16;
    static constexpr int8_t kNudgeEnd = 2;  // Event::nudge value for "the pitch button was released"

    struct Event {
        bool on{false};       // NoteOn, or sustain pressed when `sustain` is set
        bool sustain{false};  // sustain event (note and velocity unused)
        int8_t nudge{0};      // pitch-button nudge (-1 / +1); note, velocity and sustain unused
        uint8_t mod{0};       // ModPhase (1 press, 2 release, 3 cancel); everything else unused
        uint8_t note{0};
        uint8_t velocity{0};
        uint32_t stampUs{0};  // producer time when a clock is set (acceptance diagnostics), else 0
        PerformanceInputIdentity input{};
        bool hasInputIdentity{false};
        bool allSoundOff{false};
        bool controlChange{false};
        uint8_t controller{0};
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
        push(head, Event{true, false, 0, 0, note, velocity, clock_ ? clock_() : 0u});
    }

    void externalNoteOn(uint8_t note, uint8_t velocity,
                        PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (used >= kCapacity - kNoteOffReserve) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        push(head, Event{true, false, 0, 0, note, velocity,
                         clock_ ? clock_() : 0u, input, true});
    }

    void externalNoteOff(uint8_t note) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) >= kCapacity) {
            requestRecovery(PerformanceInputIdentity{}, false);
            return;
        }
        push(head, Event{false, false, 0, 0, note, 0, clock_ ? clock_() : 0u});
    }

    void externalNoteOff(uint8_t note, PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) >= kCapacity) {
            requestRecovery(input, true);
            return;
        }
        push(head, Event{false, false, 0, 0, note, 0,
                         clock_ ? clock_() : 0u, input, true});
    }

    // Pressing is droppable like a NoteOn; releasing is critical like a NoteOff.
    void externalSustain(bool down) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (down) {
            if (used >= kCapacity - kNoteOffReserve) {
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        } else if (used >= kCapacity) {
            requestRecovery(PerformanceInputIdentity{}, false);
            return;
        }
        push(head, Event{down, true, 0, 0, 0, 0, clock_ ? clock_() : 0u});
    }

    void externalSustain(bool down, PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (down && used >= kCapacity - kNoteOffReserve) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        if (!down && used >= kCapacity) {
            requestRecovery(input, true);
            return;
        }
        push(head, Event{down, true, 0, 0, 0, 0,
                         clock_ ? clock_() : 0u, input, true});
    }

    void externalAllSoundOff(PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        if (head - tail_.load(std::memory_order_acquire) >= kCapacity) {
            requestRecovery(input, true);
            return;
        }
        Event event{};
        event.input = input;
        event.hasInputIdentity = true;
        event.allSoundOff = true;
        event.stampUs = clock_ ? clock_() : 0u;
        push(head, event);
    }

    // A press (-1 / +1) is droppable like a NoteOn. The release (0) is critical like a NoteOff: losing it
    // would leave the auto-repeat running, so it uses the reserve and raises the recovery request.
    void externalNudge(int direction) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (direction == 0) {
            if (used >= kCapacity) {
                requestRecovery(PerformanceInputIdentity{}, false);
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

    void externalNudge(int direction, PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (direction == 0) {
            if (used >= kCapacity) {
                requestRecovery(input, true);
                return;
            }
            push(head, Event{false, false, kNudgeEnd, 0, 0, 0,
                             clock_ ? clock_() : 0u, input, true});
            return;
        }
        if (used >= kCapacity - kNoteOffReserve) {
            dropped_.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        push(head, Event{false, false, static_cast<int8_t>(direction < 0 ? -1 : 1), 0, 0, 0,
                         clock_ ? clock_() : 0u, input, true});
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
            requestRecovery(PerformanceInputIdentity{}, false);
            return;
        }
        push(head, Event{false, false, 0, static_cast<uint8_t>(phase), 0, 0, clock_ ? clock_() : 0u});
    }

    void externalMod(ModPhase phase, PerformanceInputIdentity input) override {
        const uint32_t head = head_.load(std::memory_order_relaxed);
        const uint32_t used = head - tail_.load(std::memory_order_acquire);
        if (phase == ModPhase::Press) {
            if (used >= kCapacity - kNoteOffReserve) {
                dropped_.fetch_add(1, std::memory_order_relaxed);
                return;
            }
        } else if (used >= kCapacity) {
            requestRecovery(input, true);
            return;
        }
        push(head, Event{false, false, 0, static_cast<uint8_t>(phase), 0, 0,
                         clock_ ? clock_() : 0u, input, true});
    }

    void externalControlChange(uint8_t controller, uint8_t value,
                               PerformanceInputIdentity input) override {
        std::size_t selected = kControlCoalesceSlots;
        std::size_t free = kControlCoalesceSlots;
        for (std::size_t i = 0; i < kControlCoalesceSlots; ++i) {
            const LatestControlSlot& slot = controls_[i];
            if (!slot.initialized.load(std::memory_order_acquire)) {
                if (free == kControlCoalesceSlots) free = i;
                continue;
            }
            if (slot.source.load(std::memory_order_relaxed) == input.source &&
                slot.generation.load(std::memory_order_relaxed) == input.generation &&
                slot.channel.load(std::memory_order_relaxed) == input.channel &&
                slot.cable.load(std::memory_order_relaxed) == input.cable &&
                slot.controller.load(std::memory_order_relaxed) == controller) {
                selected = i;
                break;
            }
        }
        if (selected == kControlCoalesceSlots) {
            selected = free < kControlCoalesceSlots ? free
                : nextControlSlot_++ % kControlCoalesceSlots;
        }
        LatestControlSlot& slot = controls_[selected];
        uint32_t sequence = slot.sequence.load(std::memory_order_relaxed);
        if (sequence & 1u) ++sequence;
        slot.sequence.store(sequence + 1u, std::memory_order_release);
        slot.source.store(input.source, std::memory_order_relaxed);
        slot.generation.store(input.generation, std::memory_order_relaxed);
        slot.channel.store(input.channel, std::memory_order_relaxed);
        slot.cable.store(input.cable, std::memory_order_relaxed);
        slot.controller.store(controller, std::memory_order_relaxed);
        slot.value.store(value, std::memory_order_relaxed);
        slot.stampUs.store(clock_ ? clock_() : 0u, std::memory_order_relaxed);
        slot.initialized.store(true, std::memory_order_relaxed);
        slot.sequence.store(sequence + 2u, std::memory_order_release);
    }

    bool pop(Event& out) {
        const uint32_t tail = tail_.load(std::memory_order_relaxed);
        if (tail != head_.load(std::memory_order_acquire)) {
            out = ring_[tail & (kCapacity - 1)];
            tail_.store(tail + 1, std::memory_order_release);
            return true;
        }
        return popLatestControl(out);
    }

    bool popRecovery(PerformanceInputIdentity& input) {
        const uint32_t tail = recoveryTail_.load(std::memory_order_relaxed);
        if (tail == recoveryHead_.load(std::memory_order_acquire)) return false;
        input = recovery_[tail & (kRecoveryCapacity - 1u)];
        recoveryTail_.store(tail + 1u, std::memory_order_release);
        return true;
    }

    bool takeGlobalRecovery() { return globalRecovery_.exchange(false, std::memory_order_acq_rel); }

    // Compatibility helper for tests and legacy consumers; consumes every pending recovery scope.
    bool takeRecovery() {
        bool pending = takeGlobalRecovery();
        PerformanceInputIdentity ignored{};
        while (popRecovery(ignored)) pending = true;
        return pending;
    }
    uint32_t dropped() const { return dropped_.load(std::memory_order_relaxed); }

private:
    struct LatestControlSlot {
        std::atomic<uint32_t> sequence{0};
        std::atomic<uint32_t> generation{0};
        std::atomic<uint32_t> stampUs{0};
        std::atomic<uint8_t> source{0};
        std::atomic<uint8_t> channel{0};
        std::atomic<uint8_t> cable{0};
        std::atomic<uint8_t> controller{0};
        std::atomic<uint8_t> value{0};
        std::atomic<bool> initialized{false};
        uint32_t consumedSequence{0};  // consumer-owned
    };

    bool popLatestControl(Event& out) {
        for (std::size_t offset = 0; offset < kControlCoalesceSlots; ++offset) {
            const std::size_t index = (nextControlRead_ + offset) % kControlCoalesceSlots;
            LatestControlSlot& slot = controls_[index];
            const uint32_t before = slot.sequence.load(std::memory_order_acquire);
            if (before == 0u || (before & 1u) != 0u ||
                before == slot.consumedSequence ||
                !slot.initialized.load(std::memory_order_relaxed)) continue;
            const uint8_t source = slot.source.load(std::memory_order_relaxed);
            const uint32_t generation = slot.generation.load(std::memory_order_relaxed);
            const uint8_t channel = slot.channel.load(std::memory_order_relaxed);
            const uint8_t cable = slot.cable.load(std::memory_order_relaxed);
            const uint8_t controller = slot.controller.load(std::memory_order_relaxed);
            const uint8_t value = slot.value.load(std::memory_order_relaxed);
            const uint32_t stamp = slot.stampUs.load(std::memory_order_relaxed);
            if (before != slot.sequence.load(std::memory_order_acquire)) continue;
            slot.consumedSequence = before;
            nextControlRead_ = (index + 1u) % kControlCoalesceSlots;
            out = Event{};
            out.controlChange = true;
            out.controller = controller;
            out.velocity = value;
            out.input = PerformanceInputIdentity{generation, source, channel,
                                                  controller, cable};
            out.hasInputIdentity = true;
            out.stampUs = stamp;
            return true;
        }
        return false;
    }

    static bool sameRecoveryScope(const PerformanceInputIdentity& lhs,
                                  const PerformanceInputIdentity& rhs) {
        return lhs.source == rhs.source && lhs.generation == rhs.generation;
    }

    void requestRecovery(PerformanceInputIdentity input, bool hasIdentity) {
        if (!hasIdentity) {
            globalRecovery_.store(true, std::memory_order_release);
            return;
        }
        const uint32_t head = recoveryHead_.load(std::memory_order_relaxed);
        const uint32_t tail = recoveryTail_.load(std::memory_order_acquire);
        for (uint32_t cursor = tail; cursor != head; ++cursor) {
            if (sameRecoveryScope(recovery_[cursor & (kRecoveryCapacity - 1u)], input)) return;
        }
        if (head - tail >= kRecoveryCapacity) {
            globalRecovery_.store(true, std::memory_order_release);
            return;
        }
        recovery_[head & (kRecoveryCapacity - 1u)] = input;
        recoveryHead_.store(head + 1u, std::memory_order_release);
    }

    void push(uint32_t head, const Event& event) {
        ring_[head & (kCapacity - 1)] = event;
        head_.store(head + 1, std::memory_order_release);
    }

    Event ring_[kCapacity]{};
    LatestControlSlot controls_[kControlCoalesceSlots]{};
    PerformanceInputIdentity recovery_[kRecoveryCapacity]{};
    std::atomic<uint32_t> head_{0}, tail_{0}, dropped_{0};
    std::atomic<uint32_t> recoveryHead_{0}, recoveryTail_{0};
    std::atomic<bool> globalRecovery_{false};
    uint32_t (*clock_)(){nullptr};
    std::size_t nextControlSlot_{0};  // producer-owned
    std::size_t nextControlRead_{0};  // consumer-owned
};

}  // namespace GroovePuterMidi

#endif
