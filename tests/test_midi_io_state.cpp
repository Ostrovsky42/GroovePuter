#include "src/midi/midi_input_event.h"
#include "src/midi/midi_input_queue.h"
#include "src/midi/midi_io_state.h"

#include <cassert>

using namespace GroovePuterMidi;

namespace {

MidiInputEvent noteOn(uint32_t generation, uint8_t note) {
    return MidiInputEvent{
        InputKey{InputSource::Usb, generation, 0, note},
        InputKind::NoteOn,
        100,
        123,
    };
}

void attachDoesNotChangeRoutes() {
    MidiIoState state;
    state.setRoutes(MidiRoutes{true, false, true, false});
    const MidiRoutes before = state.routes();

    state.requestUsbRole(UsbRole::Host);
    assert(state.activeUsbRole() == UsbRole::Off);
    assert(state.pendingUsbRole() == UsbRole::Host);
    state.boot();
    state.usbAttached();
    state.usbReady(true, false);

    assert(state.routes() == before);
    assert(state.usbPhase() == UsbPhase::Ready);
    assert(state.usbCanReceive());
    assert(!state.usbCanSend());
}

void staleInputIsRejectedAfterDetach() {
    MidiIoState state;
    state.setRoutes(MidiRoutes{true, false, false, false});
    state.requestUsbRole(UsbRole::Host);
    state.boot();
    state.usbAttached();
    state.usbReady(true, false);
    const uint32_t session = state.usbSessionGeneration();

    assert(state.acceptsInput(noteOn(session, 60)));
    state.usbDetached();
    assert(!state.acceptsInput(noteOn(session, 60)));
}

void criticalOverflowRequestsRecoveryWithoutNeedingQueueSpace() {
    MidiInputQueue queue;
    for (std::size_t index = 0; index < MidiInputQueue::kNoteOnCapacity; ++index) {
        assert(queue.tryPush(noteOn(1, static_cast<uint8_t>(index))));
    }
    for (std::size_t index = 0; index < MidiInputQueue::kCriticalReserve; ++index) {
        assert(queue.tryPush(MidiInputEvent{
            InputKey{InputSource::Usb, 1, 0, static_cast<uint8_t>(index)},
            InputKind::NoteOff,
            0,
            456,
        }));
    }
    assert(!queue.tryPush(MidiInputEvent{
        InputKey{InputSource::Usb, 1, 0, 0}, InputKind::NoteOff, 0, 456}));
    assert(queue.recoveryPending());
    assert(queue.takeRecoveryPending());
    assert(!queue.recoveryPending());
}

void uartRouteIsIndependentOfUsbLifecycle() {
    MidiIoState state;
    state.setRoutes(MidiRoutes{false, true, false, true});
    state.requestUsbRole(UsbRole::Host);
    state.boot();
    state.usbFault();

    assert(state.acceptsInput(MidiInputEvent{
        InputKey{InputSource::Uart, 77, 15, 127}, InputKind::NoteOn, 1, 0}));
}

void invalidMidiAddressIsRejected() {
    MidiIoState state;
    state.setRoutes(MidiRoutes{false, true, false, false});
    assert(!state.acceptsInput(MidiInputEvent{
        InputKey{InputSource::Uart, 0, 16, 60}, InputKind::NoteOn, 100, 0}));
    assert(!state.acceptsInput(MidiInputEvent{
        InputKey{InputSource::Uart, 0, 0, 128}, InputKind::NoteOn, 100, 0}));
}

}  // namespace

int main() {
    attachDoesNotChangeRoutes();
    staleInputIsRejectedAfterDetach();
    criticalOverflowRequestsRecoveryWithoutNeedingQueueSpace();
    uartRouteIsIndependentOfUsbLifecycle();
    invalidMidiAddressIsRejected();
}
