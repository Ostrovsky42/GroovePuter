#include <cassert>
#include <cstddef>
#include <cstdint>

#include "src/midi/midi_input_parser.h"
#include "src/midi/midi_input_queue.h"
#include "src/midi/midi_io_state.h"

using namespace GroovePuterMidi;

namespace {

MidiInputEvent usbNote(uint32_t generation, uint8_t note, InputKind kind = InputKind::NoteOn) {
    return MidiInputEvent{InputKey{InputSource::Usb, generation, 0, note}, kind,
                          kind == InputKind::NoteOn ? static_cast<uint8_t>(100) : static_cast<uint8_t>(0), 123};
}

void parserNormalizesUsbAndPreservesRealtimeUart() {
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, 7});
    const uint8_t on[4] = {0x09, 0x90, 60, 100};
    const uint8_t off[4] = {0x09, 0x90, 60, 0};
    const ParseResult acceptedOn = parser.usbPacket(on, 10);
    const ParseResult acceptedOff = parser.usbPacket(off, 20);
    assert(acceptedOn.hasInput && acceptedOn.input.kind == InputKind::NoteOn);
    assert(acceptedOn.input.id.generation == 7 && acceptedOn.input.id.key == 60);
    assert(acceptedOff.hasInput && acceptedOff.input.kind == InputKind::NoteOff);

    parser.reset(InputSession{InputSource::Uart, 11});
    assert(!parser.uartByte(0x90, 1).hasInput);
    assert(!parser.uartByte(60, 2).hasInput);
    const ParseResult clock = parser.uartByte(0xf8, 3);
    assert(clock.hasRealtime && clock.realtimeStatus == 0xf8);
    const ParseResult uartOn = parser.uartByte(100, 4);
    assert(uartOn.hasInput && uartOn.input.kind == InputKind::NoteOn);
}

void parserAcceptsOnlyTheSelectedUsbCable() {
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, 12});
    const uint8_t cableOneNote[4] = {0x19, 0x90, 60, 100};
    assert(!parser.usbPacket(cableOneNote, 1).hasInput);
    assert(parser.setUsbCable(1));
    assert(parser.usbCable() == 1u);
    const ParseResult selected = parser.usbPacket(cableOneNote, 2);
    assert(selected.hasInput && selected.input.kind == InputKind::NoteOn);
    assert(selected.input.id.generation == 12u && selected.input.id.key == 60u);
    const uint8_t cableZeroNote[4] = {0x09, 0x90, 61, 100};
    assert(!parser.usbPacket(cableZeroNote, 3).hasInput);
    assert(!parser.setUsbCable(16));
}

void parserPreservesFullFourteenBitPitchBend() {
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, 17});
    const uint8_t bend[4] = {0x0e, 0xe2, 0x23, 0x65};
    const ParseResult parsed = parser.usbPacket(bend, 99);
    assert(parsed.hasInput && parsed.input.kind == InputKind::PitchBend);
    assert(parsed.input.id.channel == 2);
    assert(parsed.input.pitchBend14 == static_cast<uint16_t>((0x65u << 7u) | 0x23u));
}

void lifecycleRejectsStaleUsbSession() {
    MidiIoState state;
    state.setRoutes(MidiRoutes{true, false, false, false});
    state.requestUsbRole(UsbRole::Host);
    state.boot();
    state.usbAttached();
    state.usbReady(true, false);
    const uint32_t acceptedGeneration = state.usbInputGeneration();
    assert(state.acceptsInput(usbNote(acceptedGeneration, 60)));
    state.usbDetached();
    assert(!state.acceptsInput(usbNote(acceptedGeneration, 60)));
}

void staleEnumerationCannotReadyNewAttachment() {
    MidiIoState state;
    state.setRoutes(MidiRoutes{true, false, false, false});
    state.requestUsbRole(UsbRole::Host);
    state.boot();
    state.usbAttached();
    const uint32_t firstAttach = state.usbAttachGeneration();
    state.usbDetached();
    state.usbAttached();
    state.usbReady(firstAttach, true, false);
    assert(state.usbPhase() == UsbPhase::Enumerating);
    assert(!state.usbCanReceive());
    state.usbReady(state.usbAttachGeneration(), true, false);
    assert(state.usbPhase() == UsbPhase::Ready && state.usbCanReceive());
}

void overflowPreservesReleaseReserveAndRequestsRecovery() {
    MidiInputQueue queue;
    for (std::size_t i = 0; i < MidiInputQueue::kNoteOnCapacity; ++i) {
        assert(queue.tryPush(usbNote(1, static_cast<uint8_t>(i))));
    }
    assert(!queue.tryPush(usbNote(1, 100)));
    for (std::size_t i = 0; i < MidiInputQueue::kCriticalReserve; ++i) {
        assert(queue.tryPush(usbNote(1, static_cast<uint8_t>(i), InputKind::NoteOff)));
    }
    assert(!queue.tryPush(usbNote(1, 0, InputKind::NoteOff)));
    assert(queue.recoveryPending());
    assert(queue.takeRecoveryPending());
    assert(!queue.recoveryPending());
}

void expressionOverflowPreservesCriticalReleaseReserve() {
    MidiInputQueue queue;
    for (std::size_t i = 0; i < MidiInputQueue::kNoteOnCapacity; ++i) {
        assert(queue.tryPush(usbNote(3, static_cast<uint8_t>(i))));
    }
    MidiInputEvent bend = usbNote(3, 0, InputKind::PitchBend);
    bend.pitchBend14 = 12000;
    assert(!queue.tryPush(bend));
    MidiInputEvent cc = usbNote(3, 0, InputKind::ControlChange);
    cc.controller = 23;
    cc.velocity = 127;
    assert(!queue.tryPush(cc));
    assert(queue.approximateSize() == MidiInputQueue::kNoteOnCapacity);
    assert(queue.droppedPitchBendCount() == 1);
    assert(queue.droppedControlCount() == 1);

    for (std::size_t i = 0; i < MidiInputQueue::kCriticalReserve; ++i) {
        assert(queue.tryPush(usbNote(3, static_cast<uint8_t>(i), InputKind::NoteOff)));
    }
    assert(!queue.tryPush(bend));
    assert(!queue.tryPush(cc));
    assert(!queue.recoveryPending());
    assert(queue.droppedPitchBendCount() == 2);
    assert(queue.droppedControlCount() == 2);
    assert(queue.droppedCriticalCount() == 0);
    assert(queue.approximateSize() == MidiInputQueue::kCapacity);
    assert(!queue.tryPush(usbNote(3, 0, InputKind::NoteOff)));
    assert(queue.recoveryPending());

    MidiInputQueue bendQueue;
    for (std::size_t i = 0; i < MidiInputQueue::kNoteOnCapacity; ++i) {
        assert(bendQueue.tryPush(usbNote(3, static_cast<uint8_t>(i))));
    }
    bend.id.generation = 77;
    bend.pitchBend14 = 12000;
    assert(!bendQueue.tryPush(bend));
    bend.pitchBend14 = 8192;
    assert(!bendQueue.tryPush(bend));
    MidiInputEvent drained{};
    for (std::size_t i = 0; i < MidiInputQueue::kNoteOnCapacity; ++i) {
        assert(bendQueue.tryPop(drained));
        assert(drained.kind == InputKind::NoteOn);
    }
    assert(bendQueue.tryPop(drained));
    assert(drained.kind == InputKind::PitchBend);
    assert(drained.id.generation == 77u && drained.id.channel == 0u);
    assert(drained.pitchBend14 == 8192u);
}

}  // namespace

int main() {
    parserNormalizesUsbAndPreservesRealtimeUart();
    parserAcceptsOnlyTheSelectedUsbCable();
    parserPreservesFullFourteenBitPitchBend();
    lifecycleRejectsStaleUsbSession();
    staleEnumerationCannotReadyNewAttachment();
    overflowPreservesReleaseReserveAndRequestsRecovery();
    expressionOverflowPreservesCriticalReleaseReserve();
    return 0;
}
