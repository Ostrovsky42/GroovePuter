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
    static constexpr std::size_t kTargetCount = 4;
    static constexpr std::size_t kChannelCount = 16;
    static constexpr std::size_t kBendSlotCount = kTargetCount * kChannelCount;
    static constexpr uint8_t kSynthAMask = 1u << 0;
    static constexpr uint8_t kSynthBMask = 1u << 1;
    static constexpr uint8_t kDrumsMask = 1u << 2;
    static constexpr uint8_t kDxMask = 1u << 3;

    bool tryPush(const MusicalEvent& event) {
        // Output ownership rejects only fresh external NoteOn. Cleanup events are
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
            } else if (event.type == MusicalEventType::PitchBend) {
                droppedPitchBend_.incrementRelaxed();
                coalescePitchBend(event);
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
            if (tail == head_.loadAcquire()) return takeCoalescedPitchBend(event);

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

    uint32_t droppedPitchBendCount() const {
        return droppedPitchBend_.loadRelaxed();
    }

private:
    void coalescePitchBend(const MusicalEvent& event) {
        const std::size_t target = static_cast<std::size_t>(event.target);
        if (target >= kTargetCount || event.channel >= kChannelCount) return;
        const std::size_t slot = target * kChannelCount + event.channel;
        // Producer is the only writer of bendRevision_, so its own last store is the base.
        uint32_t next = bendRevision_[slot].loadRelaxed() + 2u;
        if (next == 0u) next = 2u;
        bendRevision_[slot].storeRelease(next - 1u);
        bendPayload_[slot].storeRelaxed(
            static_cast<uint32_t>(event.source) |
            (static_cast<uint32_t>(event.pitchBend14 > 16383u
                ? 16383u : event.pitchBend14) << 3u));
        bendRevision_[slot].storeRelease(next);
    }

    bool takeCoalescedPitchBend(MusicalEvent& event) {
        for (std::size_t checked = 0; checked < kBendSlotCount; ++checked) {
            const std::size_t slot = (bendReadCursor_ + checked) % kBendSlotCount;
            const uint32_t before = bendRevision_[slot].loadAcquire();
            if (before == 0u || (before & 1u) != 0u ||
                before == bendConsumedRevision_[slot]) continue;
            const uint32_t payload = bendPayload_[slot].loadRelaxed();
            const uint32_t after = bendRevision_[slot].loadAcquire();
            if (before != after || (after & 1u) != 0u) continue;
            bendConsumedRevision_[slot] = after;
            bendReadCursor_ = (slot + 1u) % kBendSlotCount;
            event = MusicalEvent{};
            event.type = MusicalEventType::PitchBend;
            event.target = static_cast<MusicalEventTarget>(slot / kChannelCount);
            event.channel = static_cast<uint8_t>(slot % kChannelCount);
            event.source = static_cast<MusicalEventSource>(payload & 0x7u);
            event.pitchBend14 = static_cast<uint16_t>(payload >> 3u);
            return true;
        }
        return false;
    }

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
    MidiRealtimeWord droppedPitchBend_;
    MidiRealtimeWord bendRevision_[kBendSlotCount]{};
    MidiRealtimeWord bendPayload_[kBendSlotCount]{};
    uint32_t bendConsumedRevision_[kBendSlotCount]{};  // consumer-owned
    std::size_t bendReadCursor_{0};
    uint32_t consumedSynthAEpoch_{0};
    uint32_t consumedSynthBEpoch_{0};
    uint32_t consumedDrumsEpoch_{0};
    uint32_t consumedDxEpoch_{0};
    uint32_t consumedOutputDisableEpochs_{0};
};

#endif  // GROOVEPUTER_MIDI_CONTROL_EVENT_QUEUE_H
