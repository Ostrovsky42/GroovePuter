#include <cassert>
#include <cstdint>
#include <vector>

#include "src/input/musical_event_router.h"
#include "src/midi/midi_input_dispatcher.h"
#include "src/midi/midi_input_parser.h"
#include "src/midi/midi_input_queue.h"
#include "src/midi/midi_io_state.h"

using namespace GroovePuterMidi;

namespace {
class CaptureSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override { events.push_back(event); }
    std::vector<MusicalEvent> events;
};

MidiIoState readyUsbInput() {
    MidiIoState io;
    io.setRoutes(MidiRoutes{true, false, false, false});
    io.requestUsbRole(UsbRole::Device);
    io.boot();
    io.usbAttached();
    io.usbReady(true, true);
    return io;
}

void usbNoteOnOffReachesConfiguredMusicalTarget() {
    MusicalEventRouter router;
    CaptureSink sink;
    assert(router.addSink(sink));

    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    MidiInputDispatcher dispatcher(router, io);

    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.channelMode = MidiInputChannelMode::Single;
    config.channel = 7;  // arbitrary inbound CH8; not an outbound-route alias
    config.target = MidiInputTarget::SynthB;
    assert(dispatcher.setConfig(config));

    const uint8_t on[4] = {0x09, 0x97, 64, 100};
    const uint8_t off[4] = {0x08, 0x87, 64, 0};
    const ParseResult parsedOn = parser.usbPacket(on, 100);
    const ParseResult parsedOff = parser.usbPacket(off, 200);
    assert(parsedOn.hasInput && queue.tryPush(parsedOn.input));
    assert(parsedOff.hasInput && queue.tryPush(parsedOff.input));

    assert(dispatcher.service(queue) == 2u);
    assert(sink.events.size() == 2u);
    assert(sink.events[0].type == MusicalEventType::NoteOn);
    assert(sink.events[0].source == MusicalEventSource::MidiInput);
    assert(sink.events[0].target == MusicalEventTarget::SynthB);
    assert(sink.events[0].note == 64u);
    assert(sink.events[1].type == MusicalEventType::NoteOff);
    assert(sink.events[1].target == MusicalEventTarget::SynthB);
}

void defaultConfigIsOffAndGenerationChangeReleasesOwnedNotes() {
    MusicalEventRouter router;
    CaptureSink sink;
    assert(router.addSink(sink));

    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    MidiInputDispatcher dispatcher(router, io);

    MidiInputEvent on{InputKey{InputSource::Usb, io.usbInputGeneration(), 0, 60},
                      InputKind::NoteOn, 100, 100};
    assert(queue.tryPush(on));
    assert(dispatcher.service(queue) == 1u);
    assert(sink.events.empty());

    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::SynthA;
    assert(dispatcher.setConfig(config));
    on.id.generation = io.usbInputGeneration();
    assert(queue.tryPush(on));
    assert(dispatcher.service(queue) == 1u);
    assert(sink.events.size() == 1u && sink.events[0].type == MusicalEventType::NoteOn);

    io.usbDetached();
    // service() must notice the input-generation boundary and release owners
    // even when no new packet arrives; otherwise a detached keyboard can leave
    // the internal synth sounding forever.
    assert(dispatcher.service(queue) == 0u);
    assert(sink.events.size() == 2u);
    assert(sink.events[1].type == MusicalEventType::NoteOff);
}
}  // namespace

int main() {
    usbNoteOnOffReachesConfiguredMusicalTarget();
    defaultConfigIsOffAndGenerationChangeReleasesOwnedNotes();
    return 0;
}
