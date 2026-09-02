#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "src/midi/uart_midi_transport_core.h"

using GroovePuterMidi::UartMidiPriority;
using GroovePuterMidi::UartMidiTransportCore;
using GroovePuterMidi::kUartMidiCriticalReserveBytes;
using GroovePuterMidi::kUartMidiRealtimeLaneBytes;
using GroovePuterMidi::kUartMidiRingBytes;

namespace {

// Stands in for the UART driver: accepts at most `capacity` bytes per call,
// which is how a real non-blocking write behaves under backpressure.
class FakeWire {
public:
    explicit FakeWire(std::size_t capacity) : capacity_(capacity) {}

    std::size_t operator()(const uint8_t* data, std::size_t length) {
        const std::size_t accepted = length < capacity_ ? length : capacity_;
        for (std::size_t i = 0; i < accepted; ++i) sent.push_back(data[i]);
        return accepted;
    }

    void setCapacity(std::size_t capacity) { capacity_ = capacity; }
    std::vector<uint8_t> sent;

private:
    std::size_t capacity_;
};

const uint8_t kNoteOn[3] = {0x90, 0x3C, 0x64};
const uint8_t kNoteOff[3] = {0x80, 0x3C, 0x00};

void queuedMessagesReachTheWireInOrder() {
    UartMidiTransportCore core;
    FakeWire wire(1024);
    assert(core.enqueue(kNoteOn, 3, UartMidiPriority::Musical));
    assert(core.enqueue(kNoteOff, 3, UartMidiPriority::Critical));
    assert(core.pendingBytes() == 6);

    core.drain(wire, 1024);
    assert(core.empty());
    assert(wire.sent.size() == 6);
    assert(wire.sent[0] == 0x90 && wire.sent[3] == 0x80);
    assert(core.diagnostics().bytesSent == 6);
}

void partialWriteLeavesTheRemainderQueued() {
    UartMidiTransportCore core;
    FakeWire wire(2);
    assert(core.enqueue(kNoteOn, 3, UartMidiPriority::Musical));

    core.drain(wire, 1024);
    assert(wire.sent.size() == 2);
    assert(core.pendingBytes() == 1);

    wire.setCapacity(1024);
    core.drain(wire, 1024);
    assert(core.empty());
    assert(wire.sent.size() == 3);
    assert(wire.sent[2] == 0x64);
}

void enqueueIsAllOrNothing() {
    // A partial message on the wire would be interpreted as a different
    // message, so a message that does not fit must not be partially written.
    UartMidiTransportCore core;
    const std::size_t musicalUsable =
        kUartMidiRingBytes - kUartMidiCriticalReserveBytes;
    std::size_t queued = 0;
    while (core.enqueue(kNoteOn, 3, UartMidiPriority::Musical)) queued += 3;

    assert(queued <= musicalUsable);
    assert(musicalUsable - queued < 3);
    assert(core.pendingBytes() == queued);
    assert(core.diagnostics().droppedMusical == 1);
}

void reserveKeepsSpaceForNoteOffWhenMusicalTrafficSaturates() {
    // The whole point of the reserve: a NoteOn flood must never make a NoteOff
    // undeliverable, because NoteOff is the message with cleanup obligations.
    UartMidiTransportCore core;
    while (core.enqueue(kNoteOn, 3, UartMidiPriority::Musical)) {
    }
    assert(core.diagnostics().droppedMusical >= 1);

    for (int i = 0; i < 16; ++i) {
        assert(core.enqueue(kNoteOff, 3, UartMidiPriority::Critical));
    }
    assert(core.diagnostics().droppedCritical == 0);
}

void criticalTrafficCanStillBeRefusedAndIsCounted() {
    UartMidiTransportCore core;
    while (core.enqueue(kNoteOff, 3, UartMidiPriority::Critical)) {
    }
    assert(core.diagnostics().droppedCritical == 1);
    // A refused critical message must be visible, not silently swallowed.
    assert(core.diagnostics().droppedMusical == 0);
}

void realtimeDrainsAheadOfABackloggedRing() {
    // Clock behind a full ring would be delayed by the ring's whole wire time,
    // about 82 ms, against a 20.8 ms tick period at 120 BPM.
    UartMidiTransportCore core;
    while (core.enqueue(kNoteOn, 3, UartMidiPriority::Musical)) {
    }
    assert(core.enqueueRealtime(0xF8));

    FakeWire wire(1024);
    core.drain(wire, 4);
    assert(!wire.sent.empty());
    assert(wire.sent[0] == 0xF8);
}

void realtimeLaneRejectsNonRealtimeStatus() {
    UartMidiTransportCore core;
    assert(!core.enqueueRealtime(0x90));
    assert(!core.enqueueRealtime(0xF7));
    assert(core.enqueueRealtime(0xFA));
    assert(core.enqueueRealtime(0xFC));
}

void realtimeLaneOverflowIsCountedNotBlocking() {
    UartMidiTransportCore core;
    std::size_t accepted = 0;
    while (core.enqueueRealtime(0xF8)) ++accepted;
    assert(accepted == kUartMidiRealtimeLaneBytes);
    assert(core.diagnostics().droppedRealtime == 1);
}

void budgetBoundsWorkPerCall() {
    // The dispatcher must not be starved by a saturated link.
    UartMidiTransportCore core;
    for (int i = 0; i < 10; ++i) {
        assert(core.enqueue(kNoteOn, 3, UartMidiPriority::Musical));
    }
    FakeWire wire(1024);
    const std::size_t written = core.drain(wire, 8);
    assert(written == 8);
    assert(core.pendingBytes() == 22);
}

void ringWrapPreservesByteOrder() {
    UartMidiTransportCore core;
    FakeWire wire(1024);
    // Walk head and tail several times past the ring boundary. Enqueue and
    // drain must alternate: the ring only holds 208 musical bytes at once.
    for (int round = 0; round < 8; ++round) {
        for (int i = 0; i < 40; ++i) {
            assert(core.enqueue(kNoteOn, 3, UartMidiPriority::Musical));
        }
        core.drain(wire, 1024);
        assert(core.empty());
    }
    assert(wire.sent.size() == 8 * 40 * 3);
    wire.sent.clear();

    const uint8_t marker[3] = {0xB0, 0x07, 0x55};
    assert(core.enqueue(marker, 3, UartMidiPriority::Musical));
    core.drain(wire, 1024);
    assert(wire.sent.size() == 3);
    assert(wire.sent[0] == 0xB0 && wire.sent[1] == 0x07 && wire.sent[2] == 0x55);
}

void resetClearsQueuesAndCounters() {
    UartMidiTransportCore core;
    core.enqueue(kNoteOn, 3, UartMidiPriority::Musical);
    core.enqueueRealtime(0xF8);
    core.reset();
    assert(core.empty());
    assert(core.pendingBytes() == 0);
    assert(core.diagnostics().messagesQueued == 0);
    assert(core.diagnostics().bytesSent == 0);
}

void stalledWireDoesNotSpin() {
    // A wire that accepts nothing must return control immediately rather than
    // looping; the queue keeps its content for the next drain.
    UartMidiTransportCore core;
    assert(core.enqueue(kNoteOn, 3, UartMidiPriority::Musical));
    FakeWire wire(0);
    const std::size_t written = core.drain(wire, 1024);
    assert(written == 0);
    assert(core.pendingBytes() == 3);
}

}  // namespace

int main() {
    queuedMessagesReachTheWireInOrder();
    partialWriteLeavesTheRemainderQueued();
    enqueueIsAllOrNothing();
    reserveKeepsSpaceForNoteOffWhenMusicalTrafficSaturates();
    criticalTrafficCanStillBeRefusedAndIsCounted();
    realtimeDrainsAheadOfABackloggedRing();
    realtimeLaneRejectsNonRealtimeStatus();
    realtimeLaneOverflowIsCountedNotBlocking();
    budgetBoundsWorkPerCall();
    ringWrapPreservesByteOrder();
    resetClearsQueuesAndCounters();
    stalledWireDoesNotSpin();
    return 0;
}
