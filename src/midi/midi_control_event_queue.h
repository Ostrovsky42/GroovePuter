#pragma once
#ifndef GROOVEPUTER_MIDI_CONTROL_EVENT_QUEUE_H
#define GROOVEPUTER_MIDI_CONTROL_EVENT_QUEUE_H

#include <cstddef>
#include <cstdint>

#include "midi_realtime_word.h"
#include "../input/musical_event.h"
#include "../output/output_ownership.h"

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
    static constexpr uint8_t kDxMask = 1u << 3;

    bool tryPush(const MusicalEvent& event) {
        // Output ownership rejects only new external NoteOn. Cleanup events are
        // still enqueued even after a live mode transition removes MIDI.
        if (event.type == MusicalEventType::NoteOn &&
            !GroovePuterOutput::allowsMidiNoteOn(event)) {
            return false;
        }

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
        while (true) {
            const uint32_t tail = tail_.loadRelaxed();
            if (tail == head_.loadAcquire()) return false;

            event = events_[tail];
            tail_.storeRelease((tail + 1u) % kStorageSize);

            // A NoteOn may have been queued immediately before the user removed
            // the MIDI side. Re-check only NoteOn at the consumer boundary;
            // NoteOff/AllNotesOff must still pass for cleanup.
            if (event.type == MusicalEventType::NoteOn &&
                !GroovePuterOutput::allowsMidiNoteOn(event)) {
                continue;
            }
            return true;
        }
    }

    uint8_t takePendingAllNotesOffMask() {
        uint8_t mask = 0;
        const uint32_t synthAEpoch = pendingSynthAEpoch_.loadAcquire();
        const uint32_t synthBEpoch = pendingSynthBEpoch_.loadAcquire();
        const uint32_t drumsEpoch = pendingDrumsEpoch_.loadAcquire();
        const uint32_t dxEpoch = pendingDxEpoch_.loadAcquire();
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
        if (dxEpoch != consumedDxEpoch_) {
            consumedDxEpoch_ = dxEpoch;
            mask |= kDxMask;
        }

        const uint32_t outputEpochs = packedOutputDisableEpochs();
        const uint32_t changed = outputEpochs ^ consumedOutputDisableEpochs_;
        if ((changed & 0x000000FFu) != 0u) mask |= kSynthAMask;
        if ((changed & 0x0000FF00u) != 0u) mask |= kSynthBMask;
        if ((changed & 0x00FF0000u) != 0u) mask |= kDrumsMask;
        consumedOutputDisableEpochs_ = outputEpochs;
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
    static uint32_t packedOutputDisableEpochs() {
        return static_cast<uint32_t>(GroovePuterOutput::midiDisableEpoch(
                   GroovePuterOutput::Track::SynthA)) |
               (static_cast<uint32_t>(GroovePuterOutput::midiDisableEpoch(
                    GroovePuterOutput::Track::SynthB)) << 8u) |
               (static_cast<uint32_t>(GroovePuterOutput::midiDisableEpoch(
                    GroovePuterOutput::Track::Drums)) << 16u);
    }

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
            case MusicalEventTarget::Dx:
                pendingDxEpoch_.incrementRelaxed();
                break;
        }
    }

    MusicalEvent events_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord pendingSynthAEpoch_;
    MidiRealtimeWord pendingSynthBEpoch_;
    MidiRealtimeWord pendingDrumsEpoch_;
    MidiRealtimeWord pendingDxEpoch_;
    MidiRealtimeWord droppedNoteOn_;
    MidiRealtimeWord droppedCritical_;
    uint32_t consumedSynthAEpoch_{0};
    uint32_t consumedSynthBEpoch_{0};
    uint32_t consumedDrumsEpoch_{0};
    uint32_t consumedDxEpoch_{0};
    uint32_t consumedOutputDisableEpochs_{0};
};

#endif  // GROOVEPUTER_MIDI_CONTROL_EVENT_QUEUE_H
