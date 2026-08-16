#include <cassert>
#include <cstdint>
#include <cstdio>
#include <type_traits>

#include "src/input/midi_input_message.h"
#include "src/input/midi_input_queue.h"

namespace {

NormalizedMidiInputMessage makeMessage(uint32_t sequence) {
    NormalizedMidiInputMessage message{};
    message.sessionId = 1000u + sequence;
    message.transportId = 7u;
    message.type = (sequence & 1u) != 0u
        ? MidiInputMessageType::NoteOn
        : MidiInputMessageType::NoteOff;
    message.channel = static_cast<uint8_t>(sequence % 16u);
    message.data1 = static_cast<uint8_t>(sequence % 128u);
    message.value = static_cast<uint16_t>((sequence * 3u) % 128u);
    return message;
}

void assertSame(const NormalizedMidiInputMessage& expected,
                const NormalizedMidiInputMessage& actual) {
    assert(actual.sessionId == expected.sessionId);
    assert(actual.transportId == expected.transportId);
    assert(actual.type == expected.type);
    assert(actual.channel == expected.channel);
    assert(actual.data1 == expected.data1);
    assert(actual.value == expected.value);
}

void testMessageContract() {
    static_assert(std::is_standard_layout<NormalizedMidiInputMessage>::value,
                  "message must be standard-layout");
    static_assert(std::is_trivially_copyable<NormalizedMidiInputMessage>::value,
                  "message must be trivially copyable");
    static_assert(sizeof(NormalizedMidiInputMessage) == 12,
                  "message DRAM contract changed");

    NormalizedMidiInputMessage message{};
    assert(message.sessionId == kInvalidMidiInputSessionId);
    assert(message.transportId == kInvalidMidiInputTransportId);
    assert(message.type == MidiInputMessageType::NoteOff);
    assert(message.channel == 0u);
    assert(message.data1 == 0u);
    assert(message.value == 0u);
}

void testFifoAndCapacity() {
    MidiInputQueue queue;
    assert(queue.approximateSize() == 0u);
    assert(queue.droppedCount() == 0u);
    assert(queue.maxObservedDepth() == 0u);

    for (std::size_t i = 0; i < MidiInputQueue::kCapacity; ++i) {
        assert(queue.tryPush(makeMessage(static_cast<uint32_t>(i))));
    }
    assert(queue.approximateSize() == MidiInputQueue::kCapacity);
    assert(queue.maxObservedDepth() == MidiInputQueue::kCapacity);

    const auto rejected = makeMessage(999u);
    assert(!queue.tryPush(rejected));
    assert(queue.droppedCount() == 1u);
    assert(queue.approximateSize() == MidiInputQueue::kCapacity);

    for (std::size_t i = 0; i < MidiInputQueue::kCapacity; ++i) {
        NormalizedMidiInputMessage actual{};
        assert(queue.tryPop(actual));
        assertSame(makeMessage(static_cast<uint32_t>(i)), actual);
    }

    NormalizedMidiInputMessage none{};
    assert(!queue.tryPop(none));
    assert(queue.approximateSize() == 0u);
}

void testWrapAround() {
    MidiInputQueue queue;
    constexpr uint32_t kIterations = 4096u;

    for (uint32_t i = 0; i < kIterations; ++i) {
        const auto expected = makeMessage(i);
        assert(queue.tryPush(expected));

        NormalizedMidiInputMessage actual{};
        assert(queue.tryPop(actual));
        assertSame(expected, actual);
    }

    assert(queue.approximateSize() == 0u);
    assert(queue.droppedCount() == 0u);
    assert(queue.maxObservedDepth() == 1u);
}

void testPartialDrainAndRefill() {
    MidiInputQueue queue;
    constexpr std::size_t kFirstBurst = 20u;
    constexpr std::size_t kDrain = 13u;

    for (std::size_t i = 0; i < kFirstBurst; ++i) {
        assert(queue.tryPush(makeMessage(static_cast<uint32_t>(i))));
    }
    for (std::size_t i = 0; i < kDrain; ++i) {
        NormalizedMidiInputMessage actual{};
        assert(queue.tryPop(actual));
        assertSame(makeMessage(static_cast<uint32_t>(i)), actual);
    }

    for (std::size_t i = kFirstBurst;
         i < kFirstBurst + kDrain;
         ++i) {
        assert(queue.tryPush(makeMessage(static_cast<uint32_t>(i))));
    }
    assert(queue.approximateSize() == kFirstBurst);

    for (std::size_t i = kDrain;
         i < kFirstBurst + kDrain;
         ++i) {
        NormalizedMidiInputMessage actual{};
        assert(queue.tryPop(actual));
        assertSame(makeMessage(static_cast<uint32_t>(i)), actual);
    }
    assert(queue.approximateSize() == 0u);
    assert(queue.droppedCount() == 0u);
}

}  // namespace

int main() {
    testMessageContract();
    testFifoAndCapacity();
    testWrapAround();
    testPartialDrainAndRefill();

    std::printf(
        "MIDI Input 0.9.10 R2 SUCCESS: message=%zu queue=%zu capacity=%zu\n",
        sizeof(NormalizedMidiInputMessage),
        sizeof(MidiInputQueue),
        MidiInputQueue::kCapacity);
    return 0;
}
