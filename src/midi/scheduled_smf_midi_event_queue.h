#pragma once

#include <cstddef>
#include <cstdint>

#include "midi_realtime_word.h"
#include "scheduled_smf_midi_event.h"

// Single-producer/single-consumer queue for the SmfPlayerTask ->
// MidiDispatchTask path. Normal NoteOn traffic is shed before the queue becomes
// full so NoteOff has reserved capacity. If critical cleanup still cannot be
// queued, the producer invalidates the generation and publishes a fixed panic
// mailbox; the consumer must release all SMF-owned wire notes for that epoch.
class ScheduledSmfMidiEventQueue {
public:
    static constexpr std::size_t kStorageSize = 128;
    static constexpr std::size_t kCapacity = kStorageSize - 1;
    static constexpr std::size_t kCriticalReserve = 16;

    bool tryPushNoteOn(uint8_t channel,
                       uint8_t note,
                       uint8_t velocity,
                       uint32_t blockSequence,
                       uint16_t frameOffset,
                       uint32_t projectTransportEpoch = 0) {
        if (transportFailed()) return false;
        if (!validData(channel, note, velocity)) {
            invalidEvent_.incrementRelaxed();
            return false;
        }

        const uint32_t head = head_.loadRelaxed();
        const uint32_t tail = tail_.loadAcquire();
        if (sizeFrom(head, tail) >= (kCapacity - kCriticalReserve)) {
            droppedNoteOn_.incrementRelaxed();
            return false;
        }

        return tryPush(makeEvent(ScheduledSmfMidiEventType::NoteOn,
                                 channel,
                                 note,
                                 velocity,
                                 blockSequence,
                                 frameOffset,
                                 projectTransportEpoch));
    }

    bool tryPushNoteOff(uint8_t channel,
                        uint8_t note,
                        uint8_t velocity,
                        uint32_t blockSequence,
                        uint16_t frameOffset,
                        uint32_t projectTransportEpoch = 0) {
        if (transportFailed()) return false;
        if (!validData(channel, note, velocity)) {
            invalidEvent_.incrementRelaxed();
            return false;
        }

        if (tryPush(makeEvent(ScheduledSmfMidiEventType::NoteOff,
                              channel,
                              note,
                              velocity,
                              blockSequence,
                              frameOffset,
                              projectTransportEpoch))) {
            return true;
        }

        criticalOverflow_.incrementRelaxed();
        invalidateAndRequestPanic();
        return false;
    }

    bool tryPop(ScheduledSmfMidiEvent& event) {
        const uint32_t tail = tail_.loadRelaxed();
        if (tail == head_.loadAcquire()) return false;
        event = events_[tail];
        tail_.storeRelease((tail + 1u) % kStorageSize);
        return true;
    }

    uint32_t invalidateAndRequestPanic() {
        const uint32_t generation = invalidateScheduledEvents();
        panicGeneration_.storeRelaxed(generation);
        const uint32_t epoch = panicEpoch_.loadRelaxed() + 1u;
        panicEpoch_.storeRelease(epoch);
        return generation;
    }

    // Rebuild future deadlines without releasing notes already owned by the
    // USB output. The new generation will re-publish their future NoteOffs.
    uint32_t invalidateScheduledEvents() {
        return generation_.incrementRelaxed();
    }

    bool takePendingPanic(uint32_t& generation) {
        const uint32_t epoch = panicEpoch_.loadAcquire();
        if (epoch == consumedPanicEpoch_) return false;
        generation = panicGeneration_.loadRelaxed();
        consumedPanicEpoch_ = epoch;
        panicRecovery_.incrementRelaxed();
        return true;
    }

    // Consumer-to-producer mailbox used when USB backpressure prevents even
    // bounded NoteOff cleanup. Blocking new publications keeps the player from
    // refilling the queue until it has observed and surfaced the failure.
    void reportTransportFailure() {
        transportFailed_.storeRelease(1u);
        const uint32_t epoch = transportFailureEpoch_.loadRelaxed() + 1u;
        transportFailureEpoch_.storeRelease(epoch);
    }

    bool takePendingTransportFailure(uint32_t& generation) {
        const uint32_t epoch = transportFailureEpoch_.loadAcquire();
        if (epoch == consumedTransportFailureEpoch_) return false;
        consumedTransportFailureEpoch_ = epoch;
        // Generation remains single-writer: only the SMF producer task mutates
        // it, while the dispatcher merely publishes the failure mailbox.
        generation = generation_.incrementRelaxed();
        return true;
    }

    // Consumer-to-producer mailbox announcing that the endpoint accepts data
    // again. A host that stops reading the bulk IN endpoint is a recoverable
    // condition, so the stall is published as a pause and withdrawn here rather
    // than ending playback.
    void reportTransportRecovery() {
        transportFailed_.storeRelease(0u);
        const uint32_t epoch = transportRecoveryEpoch_.loadRelaxed() + 1u;
        transportRecoveryEpoch_.storeRelease(epoch);
    }

    bool takePendingTransportRecovery() {
        const uint32_t epoch = transportRecoveryEpoch_.loadAcquire();
        if (epoch == consumedTransportRecoveryEpoch_) return false;
        consumedTransportRecoveryEpoch_ = epoch;
        return true;
    }

    bool transportFailed() const {
        return transportFailed_.loadAcquire() != 0u;
    }

    uint32_t generation() const { return generation_.loadAcquire(); }

    std::size_t approximateSize() const {
        return sizeFrom(head_.loadAcquire(), tail_.loadAcquire());
    }

    uint32_t droppedNoteOnCount() const { return droppedNoteOn_.loadRelaxed(); }
    uint32_t criticalOverflowCount() const { return criticalOverflow_.loadRelaxed(); }
    uint32_t panicRecoveryCount() const { return panicRecovery_.loadRelaxed(); }
    uint32_t invalidEventCount() const { return invalidEvent_.loadRelaxed(); }

private:
    static bool validData(uint8_t channel, uint8_t note, uint8_t velocity) {
        return channel <= 15 && note <= 127 && velocity <= 127;
    }

    static std::size_t sizeFrom(uint32_t head, uint32_t tail) {
        return head >= tail
            ? static_cast<std::size_t>(head - tail)
            : static_cast<std::size_t>(kStorageSize - tail + head);
    }

    ScheduledSmfMidiEvent makeEvent(ScheduledSmfMidiEventType type,
                                    uint8_t channel,
                                    uint8_t note,
                                    uint8_t velocity,
                                    uint32_t blockSequence,
                                    uint16_t frameOffset,
                                    uint32_t projectTransportEpoch) {
        ScheduledSmfMidiEvent event{};
        event.type = type;
        event.channel = channel;
        event.note = note;
        event.velocity = velocity;
        event.blockSequence = blockSequence;
        event.frameOffset = frameOffset;
        event.generation = generation_.loadAcquire();
        event.publicationSequence = publicationSequence_.incrementRelaxed();
        event.projectTransportEpoch =
            scheduledSmfMidiEventTransportEpochTag(projectTransportEpoch);
        return event;
    }

    bool tryPush(const ScheduledSmfMidiEvent& event) {
        const uint32_t head = head_.loadRelaxed();
        const uint32_t next = (head + 1u) % kStorageSize;
        if (next == tail_.loadAcquire()) return false;
        events_[head] = event;
        head_.storeRelease(next);
        return true;
    }

    ScheduledSmfMidiEvent events_[kStorageSize]{};
    MidiRealtimeWord head_;
    MidiRealtimeWord tail_;
    MidiRealtimeWord generation_;
    MidiRealtimeWord publicationSequence_;
    MidiRealtimeWord droppedNoteOn_;
    MidiRealtimeWord criticalOverflow_;
    MidiRealtimeWord panicRecovery_;
    MidiRealtimeWord invalidEvent_;
    MidiRealtimeWord panicGeneration_;
    MidiRealtimeWord panicEpoch_;
    MidiRealtimeWord transportFailed_;
    MidiRealtimeWord transportFailureEpoch_;
    MidiRealtimeWord transportRecoveryEpoch_;
    uint32_t consumedPanicEpoch_{0};
    uint32_t consumedTransportFailureEpoch_{0};
    uint32_t consumedTransportRecoveryEpoch_{0};
};
