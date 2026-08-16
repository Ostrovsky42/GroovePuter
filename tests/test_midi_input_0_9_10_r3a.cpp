#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "src/input/midi_input_router.h"

namespace {

class CaptureSink final : public IMusicalEventSink {
public:
    static constexpr std::size_t kMaxEvents = 256u;

    void handleMusicalEvent(const MusicalEvent& event) override {
        assert(count_ < kMaxEvents);
        events_[count_++] = event;
    }

    void clear() { count_ = 0; }
    std::size_t count() const { return count_; }
    const MusicalEvent& at(std::size_t index) const {
        assert(index < count_);
        return events_[index];
    }

private:
    MusicalEvent events_[kMaxEvents]{};
    std::size_t count_{0};
};

NormalizedMidiInputMessage noteMessage(uint8_t status,
                                       uint8_t note,
                                       uint8_t velocity,
                                       MidiInputTransportId transport = 1u,
                                       MidiInputSessionId session = 1u,
                                       uint32_t timestamp = 0u) {
    NormalizedMidiInputMessage message{};
    assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        status, note, velocity, transport, session, timestamp, message));
    return message;
}

NormalizedMidiInputMessage controlChange(uint8_t channel,
                                         uint8_t controller,
                                         uint8_t value) {
    NormalizedMidiInputMessage message{};
    assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        static_cast<uint8_t>(0xB0u | channel),
        controller,
        value,
        1u,
        1u,
        0u,
        message));
    return message;
}

void assertEvent(const MusicalEvent& event,
                 MusicalEventType type,
                 MusicalEventTarget target,
                 uint8_t channel,
                 uint8_t note,
                 uint8_t velocity) {
    assert(event.type == type);
    assert(event.source == MusicalEventSource::MidiInput);
    assert(event.target == target);
    assert(event.channel == channel);
    assert(event.note == note);
    assert(event.velocity == velocity);
}

struct Fixture {
    MusicalEventRouter fanout;
    CaptureSink sink;
    MidiInputRouter input{fanout};

    Fixture() {
        assert(fanout.addSink(sink));
    }
};

void testDefaultDisabledAndBasicRouting() {
    Fixture f;
    assert(!f.input.config().enabled);

    const auto noteOn = noteMessage(0x92u, 60u, 101u);
    assert(!f.input.handle(noteOn));
    assert(f.sink.count() == 0u);
    assert(f.input.activeNoteCount() == 0u);
    assert(f.input.diagnostics().filteredMessages == 1u);

    assert(f.input.setEnabled(true));
    assert(f.input.handle(noteOn));
    assert(f.sink.count() == 1u);
    assertEvent(f.sink.at(0),
                MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA,
                2u,
                60u,
                101u);
    assert(f.input.activeNoteCount() == 1u);

    const auto noteOff = noteMessage(0x82u, 60u, 17u);
    assert(f.input.handle(noteOff));
    assert(f.sink.count() == 2u);
    assertEvent(f.sink.at(1),
                MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA,
                2u,
                60u,
                17u);
    assert(f.input.activeNoteCount() == 0u);
}

void testConfigChangeReleasesOldTargetBeforeAdoption() {
    Fixture f;
    assert(f.input.setEnabled(true));
    assert(f.input.handle(noteMessage(0x90u, 64u, 90u)));
    assert(f.input.activeNoteCount() == 1u);

    assert(f.input.setTarget(MidiInputTarget::SynthB));
    assert(f.input.activeNoteCount() == 0u);
    assert(f.sink.count() == 2u);
    assertEvent(f.sink.at(0), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA, 0u, 64u, 90u);
    assertEvent(f.sink.at(1), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA, 0u, 64u, 0u);

    assert(!f.input.handle(noteMessage(0x80u, 64u, 0u)));
    assert(f.sink.count() == 2u);
    assert(f.input.diagnostics().orphanNoteOffs == 1u);

    assert(f.input.handle(noteMessage(0x90u, 67u, 88u)));
    assertEvent(f.sink.at(2), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthB, 0u, 67u, 88u);
}

void testSingleChannelFilterAndCleanup() {
    Fixture f;
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.channelMode = MidiInputChannelMode::Single;
    config.channel = 4u;
    config.target = MidiInputTarget::SynthB;
    assert(f.input.setConfig(config));

    assert(!f.input.handle(noteMessage(0x93u, 60u, 100u)));
    assert(f.input.handle(noteMessage(0x94u, 61u, 100u)));
    assert(f.input.activeNoteCount() == 1u);

    assert(f.input.setEnabled(false));
    assert(f.input.activeNoteCount() == 0u);
    assert(f.sink.count() == 2u);
    assertEvent(f.sink.at(0), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthB, 4u, 61u, 100u);
    assertEvent(f.sink.at(1), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthB, 4u, 61u, 0u);
}

void testRepeatedNoteRetriggersWithoutReferenceCount() {
    Fixture f;
    assert(f.input.setEnabled(true));

    const auto first = noteMessage(0x90u, 60u, 70u, 3u, 9u);
    const auto second = noteMessage(0x90u, 60u, 110u, 3u, 9u);
    assert(f.input.handle(first));
    assert(f.input.handle(second));
    assert(f.input.activeNoteCount() == 1u);
    assert(f.sink.count() == 3u);
    assertEvent(f.sink.at(0), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA, 0u, 60u, 70u);
    assertEvent(f.sink.at(1), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA, 0u, 60u, 0u);
    assertEvent(f.sink.at(2), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA, 0u, 60u, 110u);
    assert(f.input.diagnostics().repeatedNoteRetriggers == 1u);
    assert(f.input.diagnostics().routedPitchReplacements == 0u);
}

void testTransportAndSessionArePartOfOwnershipIdentity() {
    Fixture f;
    assert(f.input.setEnabled(true));

    assert(f.input.handle(noteMessage(0x90u, 60u, 80u, 1u, 1u)));
    assert(f.input.handle(noteMessage(0x90u, 61u, 81u, 1u, 2u)));
    assert(f.input.handle(noteMessage(0x90u, 62u, 82u, 2u, 1u)));
    assert(f.input.activeNoteCount() == 3u);

    assert(f.input.handle(noteMessage(0x80u, 61u, 0u, 1u, 2u)));
    assert(f.input.activeNoteCount() == 2u);
    assert(f.sink.count() == 4u);
    assertEvent(f.sink.at(3), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA, 0u, 61u, 0u);
}

void testNewestOwnerWinsSameResolvedPitch() {
    Fixture f;
    assert(f.input.setEnabled(true));

    assert(f.input.handle(noteMessage(0x90u, 60u, 80u, 1u, 1u)));
    assert(f.input.handle(noteMessage(0x90u, 60u, 100u, 1u, 2u)));
    assert(f.input.activeNoteCount() == 1u);
    assert(f.sink.count() == 3u);
    assertEvent(f.sink.at(0), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA, 0u, 60u, 80u);
    assertEvent(f.sink.at(1), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA, 0u, 60u, 0u);
    assertEvent(f.sink.at(2), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA, 0u, 60u, 100u);
    assert(f.input.diagnostics().routedPitchReplacements == 1u);

    // The retired session cannot stop the newer owner of the same pitch.
    assert(!f.input.handle(noteMessage(0x80u, 60u, 0u, 1u, 1u)));
    assert(f.sink.count() == 3u);
    assert(f.input.activeNoteCount() == 1u);

    assert(f.input.handle(noteMessage(0x80u, 60u, 7u, 1u, 2u)));
    assert(f.sink.count() == 4u);
    assertEvent(f.sink.at(3), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA, 0u, 60u, 7u);
    assert(f.input.activeNoteCount() == 0u);
}

void testClampedPitchAliasKeepsRawSourceOwnership() {
    Fixture f;
    assert(f.input.setEnabled(true));

    assert(MidiInputRouter::normalizeSynthNote(0u) == 24u);
    assert(MidiInputRouter::normalizeSynthNote(23u) == 24u);
    assert(MidiInputRouter::normalizeSynthNote(24u) == 24u);
    assert(MidiInputRouter::normalizeSynthNote(71u) == 71u);
    assert(MidiInputRouter::normalizeSynthNote(72u) == 71u);
    assert(MidiInputRouter::normalizeSynthNote(127u) == 71u);

    assert(f.input.handle(noteMessage(0x90u, 0u, 70u, 1u, 1u)));
    assertEvent(f.sink.at(0), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA, 0u, 24u, 70u);

    // Raw note 1 is a different physical key but resolves to the same internal
    // C1. It replaces raw note 0 as sole owner of routed pitch 24.
    assert(f.input.handle(noteMessage(0x90u, 1u, 90u, 1u, 2u)));
    assert(f.input.activeNoteCount() == 1u);
    assert(f.sink.count() == 3u);
    assertEvent(f.sink.at(1), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA, 0u, 24u, 0u);
    assertEvent(f.sink.at(2), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA, 0u, 24u, 90u);

    assert(!f.input.handle(noteMessage(0x80u, 0u, 0u, 1u, 1u)));
    assert(f.sink.count() == 3u);
    assert(f.input.handle(noteMessage(0x80u, 1u, 11u, 1u, 2u)));
    assertEvent(f.sink.at(3), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA, 0u, 24u, 11u);

    f.sink.clear();
    assert(f.input.handle(noteMessage(0x90u, 127u, 100u, 3u, 4u)));
    assertEvent(f.sink.at(0), MusicalEventType::NoteOn,
                MusicalEventTarget::SynthA, 0u, 71u, 100u);
    assert(f.input.handle(noteMessage(0x80u, 127u, 0u, 3u, 4u)));
    assertEvent(f.sink.at(1), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA, 0u, 71u, 0u);
}

void testBoundedOwnerCapacityNeverPublishesUnownedNoteOn() {
    Fixture f;
    assert(f.input.setEnabled(true));

    for (std::size_t i = 0; i < MidiInputRouter::kMaxActiveNotes; ++i) {
        assert(f.input.handle(noteMessage(
            0x90u,
            static_cast<uint8_t>(24u + i),
            100u,
            1u,
            1u)));
    }
    assert(f.input.activeNoteCount() == MidiInputRouter::kMaxActiveNotes);
    assert(f.sink.count() == MidiInputRouter::kMaxActiveNotes);

    assert(!f.input.handle(noteMessage(0x90u, 100u, 100u, 1u, 1u)));
    assert(f.sink.count() == MidiInputRouter::kMaxActiveNotes);
    assert(f.input.diagnostics().ownershipCapacityDrops == 1u);

    f.input.panic();
    assert(f.input.activeNoteCount() == 0u);
    assert(f.sink.count() == MidiInputRouter::kMaxActiveNotes * 2u);
}

void testQueueOverflowForcesPanicAndDiscard() {
    Fixture f;
    MidiInputQueue queue;
    assert(f.input.setEnabled(true));

    assert(queue.tryPush(noteMessage(0x90u, 60u, 100u)));
    assert(f.input.service(queue) == 1u);
    assert(f.input.activeNoteCount() == 1u);
    f.sink.clear();

    for (std::size_t i = 0; i < MidiInputQueue::kCapacity; ++i) {
        assert(queue.tryPush(noteMessage(
            0x90u,
            static_cast<uint8_t>(24u + (i % 80u)),
            90u,
            1u,
            1u,
            static_cast<uint32_t>(i))));
    }
    assert(!queue.tryPush(noteMessage(0x90u, 110u, 90u)));
    assert(queue.overflowEpoch() == 1u);

    assert(f.input.service(queue) == 0u);
    assert(queue.empty());
    assert(f.input.activeNoteCount() == 0u);
    assert(f.input.observedOverflowEpoch() == 1u);
    assert(f.input.diagnostics().overflowRecoveries == 1u);
    assert(f.sink.count() == 1u);
    assertEvent(f.sink.at(0), MusicalEventType::NoteOff,
                MusicalEventTarget::SynthA, 0u, 60u, 0u);
}

void testUnsupportedControllerMessagesDoNotMutateOwnership() {
    Fixture f;
    assert(f.input.setEnabled(true));
    assert(!f.input.handle(controlChange(0u, 64u, 127u)));
    assert(f.input.activeNoteCount() == 0u);
    assert(f.sink.count() == 0u);
    assert(f.input.diagnostics().ignoredUnsupported == 1u);
}

void testInvalidConfigRejectedWithoutPanic() {
    Fixture f;
    MidiInputRoutingConfig invalid{};
    invalid.enabled = true;
    invalid.channelMode = MidiInputChannelMode::Single;
    invalid.channel = 16u;
    assert(!f.input.setConfig(invalid));
    assert(!f.input.config().enabled);
    assert(f.input.diagnostics().configPanics == 0u);
}

}  // namespace

int main() {
    static_assert(MidiInputRouter::kSynthNoteMin == 24u,
                  "R3a synth minimum changed without engine audit");
    static_assert(MidiInputRouter::kSynthNoteMax == 71u,
                  "R3a synth maximum changed without engine audit");
    static_assert(MidiInputRouter::kMaxActiveNotes == 24u,
                  "R3a owner capacity changed without re-measurement");
    static_assert(sizeof(MidiInputRouter) <= 320u,
                  "R3a router/owner memory budget exceeded");

    testDefaultDisabledAndBasicRouting();
    testConfigChangeReleasesOldTargetBeforeAdoption();
    testSingleChannelFilterAndCleanup();
    testRepeatedNoteRetriggersWithoutReferenceCount();
    testTransportAndSessionArePartOfOwnershipIdentity();
    testNewestOwnerWinsSameResolvedPitch();
    testClampedPitchAliasKeepsRawSourceOwnership();
    testBoundedOwnerCapacityNeverPublishesUnownedNoteOn();
    testQueueOverflowForcesPanicAndDiscard();
    testUnsupportedControllerMessagesDoNotMutateOwnership();
    testInvalidConfigRejectedWithoutPanic();

    std::cout << "MidiInputRouter size=" << sizeof(MidiInputRouter)
              << " bytes activeOwners=" << MidiInputRouter::kMaxActiveNotes
              << " synthRange=" << static_cast<unsigned>(MidiInputRouter::kSynthNoteMin)
              << ".." << static_cast<unsigned>(MidiInputRouter::kSynthNoteMax)
              << "\n";
    std::cout << "0.9.10 R3a MIDI input routing ownership: PASS\n";
    return 0;
}
