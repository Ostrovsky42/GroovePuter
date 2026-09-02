#include <cassert>
#include <cstdint>

#include "src/platform/cardputer_uart_midi_transport.h"

using GroovePuterMidi::CardputerUartMidiTransport;

namespace {

CardputerUartMidiTransport makeStarted() {
    CardputerUartMidiTransport transport;
    assert(transport.begin());
    return transport;
}

void linkIsReportedAsUnverifiable() {
    // A DIN wire gives no feedback: an unplugged cable, a powered-off synth
    // and a healthy link all look identical. Nothing may render this endpoint
    // as connected.
    CardputerUartMidiTransport transport;
    assert(transport.linkKind() == MidiTransportLink::Unverifiable);
    assert(!transport.mounted());
    assert(transport.begin());
    assert(transport.mounted());
}

void nothingIsQueuedBeforeBegin() {
    CardputerUartMidiTransport transport;
    assert(!transport.sendNoteOn(0, 60, 100));
    assert(!transport.sendNoteOff(0, 60, 0));
    assert(!transport.sendTimingClock());
    assert(transport.pendingBytes() == 0);
}

void channelMessagesAreThreeBytes() {
    auto transport = makeStarted();
    assert(transport.sendNoteOn(0, 60, 100));
    assert(transport.pendingBytes() == 3);
    assert(transport.sendNoteOff(0, 60, 0));
    assert(transport.pendingBytes() == 6);
    assert(transport.diagnostics().messagesQueued == 2);
}

void channelAndDataBytesAreMasked() {
    // A status byte with a stray high bit in the data would be read as a new
    // message by the receiver.
    auto transport = makeStarted();
    assert(transport.sendNoteOn(0xFF, 0xFF, 0xFF));
    assert(transport.pendingBytes() == 3);
    assert(transport.diagnostics().droppedMusical == 0);
}

void noteOffOutranksASaturatedNoteOnFlood() {
    // The reserve exists so a NoteOn flood cannot make a NoteOff undeliverable.
    auto transport = makeStarted();
    while (transport.sendNoteOn(0, 60, 100)) {
    }
    assert(transport.diagnostics().droppedMusical >= 1);

    for (int i = 0; i < 16; ++i) {
        assert(transport.sendNoteOff(0, static_cast<uint8_t>(60 + i), 0));
    }
    assert(transport.diagnostics().droppedCritical == 0);
}

void allNotesOffSharesNoteOffPriority() {
    auto transport = makeStarted();
    while (transport.sendNoteOn(0, 60, 100)) {
    }
    // CC123 is the terminal recovery path; it must not be droppable just
    // because musical traffic filled the ring.
    assert(transport.sendControlChange(0, 123, 0));
}

void realtimeUsesItsOwnLane() {
    // Queued behind a full data ring, a clock tick would be delayed by the
    // ring's whole wire time; the separate lane is what keeps clock usable.
    auto transport = makeStarted();
    while (transport.sendNoteOn(0, 60, 100)) {
    }
    assert(transport.sendTimingClock());
    assert(transport.sendStart());
    assert(transport.sendContinue());
    assert(transport.sendStop());
}

void songPositionPointerIsFourteenBitLsbFirst() {
    auto transport = makeStarted();
    assert(transport.sendSongPositionPointer(0x3FFF));
    assert(transport.pendingBytes() == 3);
    // Out-of-range beats must be masked rather than corrupting the status byte
    // of the following message.
    assert(transport.sendSongPositionPointer(0xFFFF));
    assert(transport.pendingBytes() == 6);
}

void flushDoesNotTouchTheWire() {
    // flush() is on the musical path. Draining there would block the
    // dispatcher on a 320 us-per-byte wire.
    auto transport = makeStarted();
    assert(transport.sendNoteOn(0, 60, 100));
    transport.flush();
    assert(transport.pendingBytes() == 3);
}

void serviceDrainsQueuedBytes() {
    auto transport = makeStarted();
    assert(transport.sendNoteOn(0, 60, 100));
    transport.service();
    assert(transport.pendingBytes() == 0);
    assert(transport.diagnostics().bytesSent == 3);
}

}  // namespace

int main() {
    linkIsReportedAsUnverifiable();
    nothingIsQueuedBeforeBegin();
    channelMessagesAreThreeBytes();
    channelAndDataBytesAreMasked();
    noteOffOutranksASaturatedNoteOnFlood();
    allNotesOffSharesNoteOffPriority();
    realtimeUsesItsOwnLane();
    songPositionPointerIsFourteenBitLsbFirst();
    flushDoesNotTouchTheWire();
    serviceDrainsQueuedBytes();
    return 0;
}
