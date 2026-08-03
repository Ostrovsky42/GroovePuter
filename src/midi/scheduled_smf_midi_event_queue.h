#pragma once

#include <cstddef>
#include <cstdint>

#include "midi_realtime_word.h"
#include "scheduled_smf_midi_event.h"
#include "smf_track_mute.h"
#include "smf_track_note_ownership.h"

// Single-producer/single-consumer queue for the SmfPlayerTask ->
// MidiDispatchTask path. Normal NoteOn traffic is shed before the queue becomes
// full so NoteOff has reserved capacity. If critical cleanup still cannot be
// queued, the producer invalidates the generation and publishes a fixed panic
// mailbox; the consumer must release all SMF-owned wire notes for that epoch.
//
// Immediate track mute and seek SPP are also consumer-owned. tryPop() consumes
// bounded mailboxes and preflights ownership capacity, while recordDispatched()
// commits ownership only after the physical USB write succeeds. TinyUSB still
// has one writer and Pattern/PERFORM ownership is untouched.
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
                       uint32_t projectTransportEpoch = 0,
                       uint8_t trackIndex = 0) {
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
                                 projectTransportEpoch,
                                 trackIndex));
    }

    bool tryPushNoteOff(uint8_t channel,
                        uint8_t note,
                        uint8_t velocity,
                        uint32_t blockSequence,
                        uint16_t frameOffset,
                        uint32_t projectTransportEpoch = 0,
                        uint8_t trackIndex = 0) {
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
                              projectTransportEpoch,
                              trackIndex))) {
            return true;
        }

        criticalOverflow_.incrementRelaxed();
        invalidateAndRequestPanic();
        return false;
    }

    // Latest seek wins. The mailbox is separate from normal queue capacity so
    // a dense chord cannot prevent the transport position from being published.
    // Generation tagging still lets a subsequent route/stop/panic invalidate it.
    bool tryPushSongPositionPointer(uint16_t midiBeats,
                                    uint32_t blockSequence,
                                    uint16_t frameOffset) {
        if (transportFailed()) return false;
        const uint16_t value = midiBeats > 0x3FFFu ? 0x3FFFu : midiBeats;
        pendingSppValue_.storeRelaxed(value);
        pendingSppBlockSequence_.storeRelaxed(blockSequence);
        pendingSppFrameOffset_.storeRelaxed(frameOffset);
        pendingSppGeneration_.storeRelaxed(generation_.loadAcquire());
        const uint32_t epoch = pendingSppEpoch_.loadRelaxed() + 1u;
        pendingSppEpoch_.storeRelease(epoch);
        sppRequests_.incrementRelaxed();
        return true;
    }

    bool tryPop(ScheduledSmfMidiEvent& event) {
        for (;;) {
            if (releaseTrackActive_) {
                uint8_t channel = 0;
                uint8_t note = 0;
                if (activeNotes_.takeNextForTrack(
                        releaseTrack_, channel, note)) {
                    event = makeImmediateTrackRelease(
                        releaseTrack_, channel, note);
                    immediateTrackReleases_.incrementRelaxed();
                    return true;
                }
                releaseTrackActive_ = false;
            }

            uint8_t requestedTrack = 0;
            if (GroovePuterMidi::smfTrackMuteState()
                    .takePendingReleaseTrack(requestedTrack)) {
                releaseTrack_ = requestedTrack;
                releaseTrackActive_ = true;
                continue;
            }

            if (takePendingSongPositionPointer(event)) return true;

            const uint32_t tail = tail_.loadRelaxed();
            if (tail == head_.loadAcquire()) return false;
            event = events_[tail];
            tail_.storeRelease((tail + 1u) % kStorageSize);

            lastPoppedBlockSequence_ = event.blockSequence;
            lastPoppedFrameOffset_ = event.frameOffset;

            // Stale generations are returned unchanged so the dispatcher keeps
            // its accepted diagnostics and drop policy. They must not mutate
            // active ownership: an invalidated event never reached the wire.
            if (event.generation != generation_.loadAcquire()) return true;

            if (event.type == ScheduledSmfMidiEventType::SongPositionPointer) {
                return true;
            }

            const bool noteOn =
                event.type == ScheduledSmfMidiEventType::NoteOn;
            if (!GroovePuterMidi::shouldEmitSmfTrackEvent(
                    noteOn, event.trackIndex)) {
                mutedNoteOnDrops_.incrementRelaxed();
                continue;
            }

            if (noteOn && !activeNotes_.canAcquire(
                    event.trackIndex, event.channel, event.note)) {
                // Capacity is checked before the wire write, but ownership is
                // committed only by recordDispatched() after that write succeeds.
                ownershipOverflowDrops_.incrementRelaxed();
                continue;
            }
            return true;
        }
    }

    // Called by MidiDispatchTask only after dispatchSmfEvent() succeeds. This
    // keeps logical track ownership aligned with UsbMidiOutput's physical owner
    // counts and prevents late/muted/backpressured NoteOn drops from consuming
    // bounded ownership entries.
    bool recordDispatched(const ScheduledSmfMidiEvent& event) {
        switch (event.type) {
            case ScheduledSmfMidiEventType::NoteOn:
                if (!activeNotes_.acquire(
                        event.trackIndex, event.channel, event.note)) {
                    ownershipCommitFailures_.incrementRelaxed();
                    return false;
                }
                return true;
            case ScheduledSmfMidiEventType::NoteOff:
                // Immediate mute already removes its logical owner before
                // emitting the scoped NoteOff. An unmatched release is valid.
                (void)activeNotes_.release(
                    event.trackIndex, event.channel, event.note);
                return true;
            case ScheduledSmfMidiEventType::SongPositionPointer:
                return true;
        }
        return false;
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
        // This method is consumed by MidiDispatchTask. The following global
        // SMF cleanup owns every remaining wire note, so local per-track
        // ownership must be forgotten without emitting duplicate releases.
        activeNotes_.clearWithoutRelease();
        releaseTrackActive_ = false;
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
    uint32_t mutedNoteOnDropCount() const {
        return mutedNoteOnDrops_.loadRelaxed();
    }
    uint32_t immediateTrackReleaseCount() const {
        return immediateTrackReleases_.loadRelaxed();
    }
    uint32_t ownershipOverflowDropCount() const {
        return ownershipOverflowDrops_.loadRelaxed();
    }
    uint32_t ownershipCommitFailureCount() const {
        return ownershipCommitFailures_.loadRelaxed();
    }
    uint32_t songPositionRequestCount() const {
        return sppRequests_.loadRelaxed();
    }

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
                                    uint32_t projectTransportEpoch,
                                    uint8_t trackIndex) {
        ScheduledSmfMidiEvent event{};
        event.type = type;
        event.channel = channel;
        event.note = note;
        event.velocity = velocity;
        event.trackIndex = trackIndex > 63u ? 63u : trackIndex;
        event.blockSequence = blockSequence;
        event.frameOffset = frameOffset;
        event.generation = generation_.loadAcquire();
        event.projectTransportEpoch =
            scheduledSmfMidiEventTransportEpochTag(projectTransportEpoch);
        return event;
    }

    bool takePendingSongPositionPointer(ScheduledSmfMidiEvent& event) {
        const uint32_t epoch = pendingSppEpoch_.loadAcquire();
        if (epoch == consumedSppEpoch_) return false;
        consumedSppEpoch_ = epoch;

        const uint16_t value = static_cast<uint16_t>(
            pendingSppValue_.loadRelaxed() & 0x3FFFu);
        event = ScheduledSmfMidiEvent{};
        event.type = ScheduledSmfMidiEventType::SongPositionPointer;
        event.note = static_cast<uint8_t>(value & 0x7Fu);
        event.velocity = static_cast<uint8_t>((value >> 7u) & 0x7Fu);
        event.blockSequence = pendingSppBlockSequence_.loadRelaxed();
        event.frameOffset = static_cast<uint16_t>(
            pendingSppFrameOffset_.loadRelaxed());
        event.generation = pendingSppGeneration_.loadRelaxed();
        event.projectTransportEpoch = 0;
        return true;
    }

    ScheduledSmfMidiEvent makeImmediateTrackRelease(uint8_t trackIndex,
                                                     uint8_t channel,
                                                     uint8_t note) {
        ScheduledSmfMidiEvent event{};
        event.type = ScheduledSmfMidiEventType::NoteOff;
        event.channel = channel;
        event.note = note;
        event.velocity = 0;
        event.trackIndex = trackIndex;
        // Reusing the latest consumer deadline deliberately makes the release
        // immediately due (or slightly late). The existing late policy always
        // dispatches NoteOff and never turns it into a catch-up NoteOn burst.
        event.blockSequence = lastPoppedBlockSequence_;
        event.frameOffset = lastPoppedFrameOffset_;
        event.generation = generation_.loadAcquire();
        event.projectTransportEpoch = 0;
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
    MidiRealtimeWord droppedNoteOn_;
    MidiRealtimeWord criticalOverflow_;
    MidiRealtimeWord panicRecovery_;
    MidiRealtimeWord invalidEvent_;
    MidiRealtimeWord panicGeneration_;
    MidiRealtimeWord panicEpoch_;
    MidiRealtimeWord transportFailed_;
    MidiRealtimeWord transportFailureEpoch_;
    MidiRealtimeWord transportRecoveryEpoch_;
    MidiRealtimeWord mutedNoteOnDrops_;
    MidiRealtimeWord immediateTrackReleases_;
    MidiRealtimeWord ownershipOverflowDrops_;
    MidiRealtimeWord ownershipCommitFailures_;
    MidiRealtimeWord pendingSppValue_;
    MidiRealtimeWord pendingSppBlockSequence_;
    MidiRealtimeWord pendingSppFrameOffset_;
    MidiRealtimeWord pendingSppGeneration_;
    MidiRealtimeWord pendingSppEpoch_;
    MidiRealtimeWord sppRequests_;
    GroovePuterMidi::SmfTrackNoteOwnership<> activeNotes_;
    uint32_t consumedPanicEpoch_{0};
    uint32_t consumedTransportFailureEpoch_{0};
    uint32_t consumedTransportRecoveryEpoch_{0};
    uint32_t consumedSppEpoch_{0};
    uint32_t lastPoppedBlockSequence_{0};
    uint16_t lastPoppedFrameOffset_{0};
    uint8_t releaseTrack_{0};
    bool releaseTrackActive_{false};
};
