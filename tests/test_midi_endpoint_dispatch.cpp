#include <cassert>
#include <cstdint>

#include "src/midi/midi_endpoint_dispatch.h"

using namespace GroovePuterMidi;

namespace {

class FakeTransport final : public IMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return mounted_; }
    bool sendNoteOn(uint8_t, uint8_t, uint8_t) override { ++noteOnCount; return accept_; }
    bool sendNoteOff(uint8_t, uint8_t, uint8_t) override { ++noteOffCount; return accept_; }
    void flush() override {}

    bool mounted_{true};
    bool accept_{true};
    unsigned noteOnCount{0};
    unsigned noteOffCount{0};
};

MidiEndpointEvent noteOn(uint32_t sequence) {
    return MidiEndpointEvent{sequence, MidiEndpointEventType::NoteOn, 0, 60, 100};
}

void retryTargetsOnlyEndpointThatRejectedTheEvent() {
    FakeTransport usb;
    FakeTransport uart;
    MidiEndpointDispatcher dispatcher(usb, uart);
    dispatcher.setEnabled(MidiEndpoint::Usb, true);
    dispatcher.setEnabled(MidiEndpoint::Uart, true);

    usb.accept_ = false;
    const MidiEndpointDelivery first = dispatcher.deliver(noteOn(41));
    assert(!first.accepted(MidiEndpoint::Usb));
    assert(first.accepted(MidiEndpoint::Uart));
    assert(uart.noteOnCount == 1);

    usb.accept_ = true;
    const MidiEndpointDelivery retry = dispatcher.deliver(noteOn(41));
    assert(retry.accepted(MidiEndpoint::Usb));
    assert(retry.accepted(MidiEndpoint::Uart));
    assert(usb.noteOnCount == 2);
    assert(uart.noteOnCount == 1);
}

}  // namespace

int main() { retryTargetsOnlyEndpointThatRejectedTheEvent(); }
