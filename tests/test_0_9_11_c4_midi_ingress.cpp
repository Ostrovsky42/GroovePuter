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

}  // namespace

int main() {
    parserNormalizesUsbAndPreservesRealtimeUart();
    lifecycleRejectsStaleUsbSession();
    staleEnumerationCannotReadyNewAttachment();
    overflowPreservesReleaseReserveAndRequestsRecovery();
    return 0;
}
