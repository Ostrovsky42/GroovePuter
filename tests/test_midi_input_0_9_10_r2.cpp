#include <cassert>
#include <cstdint>
#include <iostream>

#include "src/input/midi_input_message.h"
#include "src/input/midi_input_queue.h"

namespace {

NormalizedMidiInputMessage makeNote(uint8_t note,
                                    uint8_t velocity,
                                    uint32_t timestamp,
                                    MidiInputTransportId transportId = 1u,
                                    MidiInputSessionId sessionId = 1u) {
    NormalizedMidiInputMessage message{};
    const bool ok = NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0x90u, note, velocity, transportId, sessionId, timestamp, message);
    assert(ok);
    return message;
}

void testNormalization() {
    NormalizedMidiInputMessage message{};

    assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0x92u, 60u, 100u, 7u, 42u, 123456u, message));
    assert(message.type == MidiInputMessageType::NoteOn);
    assert(message.transportId == 7u);
    assert(message.sessionId == 42u);
    assert(message.channel == 2u);
    assert(message.note() == 60u);
    assert(message.velocity() == 100u);
    assert(message.timestampMicros == 123456u);
    assert(message.isValid());

    assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0x91u, 64u, 0u, 7u, 42u, 123500u, message));
    assert(message.type == MidiInputMessageType::NoteOff);
    assert(message.channel == 1u);
    assert(message.note() == 64u);
    assert(message.velocity() == 0u);

    assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0xE3u, 0u, 64u, 7u, 42u, 200u, message));
    assert(message.type == MidiInputMessageType::PitchBend);
    assert(message.channel == 3u);
    assert(message.pitchBend14() == 8192u);

    assert(NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0xC4u, 12u, 0xFFu, 7u, 42u, 201u, message));
    assert(message.type == MidiInputMessageType::ProgramChange);
    assert(message.channel == 4u);
    assert(message.data1 == 12u);
    assert(message.data2 == 0u);

    const NormalizedMidiInputMessage before = message;
    assert(!NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0xF8u, 0u, 0u, 7u, 42u, 300u, message));
    assert(message.timestampMicros == before.timestampMicros);
    assert(message.type == before.type);
    assert(message.channel == before.channel);

    assert(!NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0x90u, 60u, 100u, kInvalidMidiInputTransportId, 42u, 0u, message));
    assert(!NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0x90u, 60u, 100u, 7u, kInvalidMidiInputSessionId, 0u, message));
    assert(!NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0x90u, 0x80u, 100u, 7u, 42u, 0u, message));
    assert(!NormalizedMidiInputMessage::fromMidi1ChannelVoice(
        0x90u, 60u, 0x80u, 7u, 42u, 0u, message));

    NormalizedMidiInputMessage nonCanonical = makeNote(60u, 100u, 1u);
    nonCanonical.data2 = 0u;
    assert(!nonCanonical.isValid());
}

void testQueueCapacityAndOverflow() {
    MidiInputQueue queue;
    assert(queue.empty());
    assert(queue.approximateSize() == 0u);
    assert(queue.droppedOverflowCount() == 0u);
    assert(queue.rejectedInvalidCount() == 0u);
    assert(queue.overflowEpoch() == 0u);
    assert(queue.highWaterMark() == 0u);

    for (std::size_t i = 0; i < MidiInputQueue::kCapacity; ++i) {
        const auto message = makeNote(
            static_cast<uint8_t>(i & 0x7Fu),
            100u,
            static_cast<uint32_t>(1000u + i));
        assert(queue.tryPush(message));
    }

    assert(queue.approximateSize() == MidiInputQueue::kCapacity);
    assert(queue.highWaterMark() == MidiInputQueue::kCapacity);

    const auto overflow = makeNote(100u, 100u, 9999u);
    assert(!queue.tryPush(overflow));
    assert(queue.droppedOverflowCount() == 1u);
    assert(queue.overflowEpoch() == 1u);

    for (std::size_t i = 0; i < MidiInputQueue::kCapacity; ++i) {
        NormalizedMidiInputMessage message{};
        assert(queue.tryPop(message));
        assert(message.note() == static_cast<uint8_t>(i & 0x7Fu));
        assert(message.timestampMicros == static_cast<uint32_t>(1000u + i));
    }

    NormalizedMidiInputMessage none{};
    assert(!queue.tryPop(none));
    assert(queue.empty());
}

void testQueueWrapAndRecoveryPrimitive() {
    MidiInputQueue queue;

    for (uint32_t i = 0; i < 512u; ++i) {
        const auto in = makeNote(
            static_cast<uint8_t>(36u + (i % 32u)),
            static_cast<uint8_t>(1u + (i % 126u)),
            i,
            2u,
            9u);
        assert(queue.tryPush(in));

        NormalizedMidiInputMessage out{};
        assert(queue.tryPop(out));
        assert(out.timestampMicros == i);
        assert(out.transportId == 2u);
        assert(out.sessionId == 9u);
    }

    for (uint32_t i = 0; i < 10u; ++i) {
        assert(queue.tryPush(makeNote(60u, 90u, i)));
    }
    assert(queue.approximateSize() == 10u);
    queue.discardPendingFromConsumer();
    assert(queue.empty());

    auto invalid = makeNote(60u, 90u, 1u);
    invalid.sessionId = kInvalidMidiInputSessionId;
    assert(!queue.tryPush(invalid));
    assert(queue.rejectedInvalidCount() == 1u);
}

}  // namespace

int main() {
    static_assert(sizeof(NormalizedMidiInputMessage) == 12u,
                  "R2 message size contract changed");
    static_assert(sizeof(MidiInputQueue) <= 800u,
                  "R2 queue budget exceeded");

    testNormalization();
    testQueueCapacityAndOverflow();
    testQueueWrapAndRecoveryPrimitive();

    std::cout << "NormalizedMidiInputMessage size="
              << sizeof(NormalizedMidiInputMessage) << " bytes\n";
    std::cout << "MidiInputQueue size=" << sizeof(MidiInputQueue)
              << " bytes capacity=" << MidiInputQueue::kCapacity << "\n";
    std::cout << "0.9.10 R2 normalized MIDI ingress: PASS\n";
    return 0;
}
