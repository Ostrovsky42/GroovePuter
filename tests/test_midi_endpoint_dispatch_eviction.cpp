#include <cassert>
#include <cstdint>

#include "src/midi/midi_endpoint_dispatch.h"

using namespace GroovePuterMidi;

namespace {

class FakeTransport final : public IMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return true; }
    bool sendNoteOn(uint8_t, uint8_t, uint8_t) override {
        ++noteOnAttempts;
        return accept;
    }
    bool sendNoteOff(uint8_t, uint8_t, uint8_t) override { return accept; }
    void flush() override {}

    bool accept{true};
    unsigned noteOnAttempts{0};
};

MidiEndpointEvent noteOn(uint32_t sequence) {
    return MidiEndpointEvent{sequence, MidiEndpointEventType::NoteOn, 0, 60, 100};
}

}  // namespace

int main() {
    FakeTransport usb;
    FakeTransport uart;
    usb.accept = false;

    MidiEndpointDispatcher dispatcher(usb, uart);
    dispatcher.setEnabled(MidiEndpoint::Usb, true);
    dispatcher.setEnabled(MidiEndpoint::Uart, true);
    dispatcher.deliver(noteOn(41));
    for (uint32_t sequence = 42; sequence <= 49; ++sequence) {
        dispatcher.deliver(noteOn(sequence));
    }

    const unsigned uartAttemptsBeforeRetry = uart.noteOnAttempts;
    dispatcher.deliver(noteOn(41));

    // A retry after newer USB backpressure must never replay an attack already
    // accepted by UART. The current eight-slot cache violates this contract.
    assert(uart.noteOnAttempts == uartAttemptsBeforeRetry);
}
