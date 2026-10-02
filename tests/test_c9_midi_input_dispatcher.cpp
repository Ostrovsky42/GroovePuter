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
    assert(dispatcher.service(queue) == 0u);
    assert(sink.events.size() == 2u);
    assert(sink.events[1].type == MusicalEventType::NoteOff);
}

void reconfigurationReleasesNotesOwnedByPreviousPolicy() {
    MusicalEventRouter router;
    CaptureSink sink;
    assert(router.addSink(sink));

    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    MidiInputDispatcher dispatcher(router, io);

    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.channelMode = MidiInputChannelMode::Omni;
    config.target = MidiInputTarget::SynthA;
    assert(dispatcher.setConfig(config));

    MidiInputEvent on{InputKey{InputSource::Usb, io.usbInputGeneration(), 2, 60},
                      InputKind::NoteOn, 100, 100};
    assert(queue.tryPush(on));
    assert(dispatcher.service(queue) == 1u);
    assert(sink.events.back().type == MusicalEventType::NoteOn);
    assert(sink.events.back().target == MusicalEventTarget::SynthA);

    MidiInputRoutingConfig disabled = config;
    disabled.enabled = false;
    assert(dispatcher.setConfig(disabled));
    assert(sink.events.back().type == MusicalEventType::NoteOff);
    assert(sink.events.back().target == MusicalEventTarget::SynthA);

    MidiInputRoutingConfig channelConfig = config;
    channelConfig.channelMode = MidiInputChannelMode::Single;
    channelConfig.channel = 2;
    assert(dispatcher.setConfig(channelConfig));
    on.id.key = 62;
    assert(queue.tryPush(on));
    assert(dispatcher.service(queue) == 1u);
    assert(sink.events.back().type == MusicalEventType::NoteOn);

    MidiInputRoutingConfig otherChannel = channelConfig;
    otherChannel.channel = 3;
    assert(dispatcher.setConfig(otherChannel));
    assert(sink.events.back().type == MusicalEventType::NoteOff);
    assert(sink.events.back().target == MusicalEventTarget::SynthA);
    assert(sink.events.back().note == 62u);

    MidiInputRoutingConfig synthA = config;
    assert(dispatcher.setConfig(synthA));
    on.id.key = 64;
    assert(queue.tryPush(on));
    assert(dispatcher.service(queue) == 1u);
    assert(sink.events.back().type == MusicalEventType::NoteOn);
    assert(sink.events.back().target == MusicalEventTarget::SynthA);

    MidiInputRoutingConfig synthB = synthA;
    synthB.target = MidiInputTarget::SynthB;
    assert(dispatcher.setConfig(synthB));
    assert(sink.events.back().type == MusicalEventType::NoteOff);
    assert(sink.events.back().target == MusicalEventTarget::SynthA);
    assert(sink.events.back().note == 64u);
}
}  // namespace

class CaptureNotes final : public MidiExternalNoteSink {
public:
    struct Entry { bool on; uint8_t note; uint8_t velocity; };
    void externalNoteOn(uint8_t note, uint8_t velocity) override { entries.push_back({true, note, velocity}); }
    void externalNoteOff(uint8_t note) override { entries.push_back({false, note, 0}); }
    void externalSustain(bool down) override { sustain.push_back(down); }
    void externalNudge(int direction) override { nudges.push_back(direction); }
    void externalMod() override { ++mods; }
    int mods = 0;
    std::vector<int> nudges;
    std::vector<bool> sustain;
    std::vector<Entry> entries;
};

void performTargetFeedsTheKeyboardBridgeNotTheRouter() {
    MusicalEventRouter router;
    CaptureSink routed;
    assert(router.addSink(routed));
    CaptureNotes bridge;

    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);

    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.channelMode = MidiInputChannelMode::Omni;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    // Three held pitches are all accepted (polyphonic, no mono arbitration), pitch is absolute
    // (no synth-range clamp: 100 stays 100 and 10 stays 10), velocity 0 NoteOn is a NoteOff.
    const uint8_t on60[4] = {0x09, 0x90, 60, 90};
    const uint8_t on100[4] = {0x09, 0x90, 100, 80};
    const uint8_t on10[4] = {0x09, 0x90, 10, 70};
    const uint8_t zeroVel[4] = {0x09, 0x90, 60, 0};
    for (const auto* packet : {&on60, &on100, &on10, &zeroVel}) {
        const ParseResult parsed = parser.usbPacket(*packet, 100);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
    }
    assert(dispatcher.service(queue) == 4u);
    assert(routed.events.empty());  // nothing went through the router
    assert(bridge.entries.size() == 4u);
    assert(bridge.entries[0].on && bridge.entries[0].note == 60 && bridge.entries[0].velocity == 90);
    assert(bridge.entries[1].on && bridge.entries[1].note == 100);
    assert(bridge.entries[2].on && bridge.entries[2].note == 10);
    assert(!bridge.entries[3].on && bridge.entries[3].note == 60);

    // Detach (new generation) releases only the notes this source still owns (100 and 10).
    bridge.entries.clear();
    io.usbDetached();
    (void)dispatcher.service(queue);
    assert(bridge.entries.size() == 2u);
    for (const auto& entry : bridge.entries) assert(!entry.on);

    // A policy change away from PERFORM releases what PERFORM owned before the change.
    io.usbAttached();
    io.usbReady(true, true);
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    const ParseResult again = parser.usbPacket(on60, 300);
    assert(again.hasInput && queue.tryPush(again.input));
    assert(dispatcher.service(queue) == 1u);
    bridge.entries.clear();
    config.target = MidiInputTarget::SynthA;
    assert(dispatcher.setConfig(config));
    assert(bridge.entries.size() == 1u && !bridge.entries[0].on && bridge.entries[0].note == 60);
}

void sustainMapsToThePerformBridgeOnly() {
    MusicalEventRouter router;
    CaptureSink routed;
    assert(router.addSink(routed));
    CaptureNotes bridge;
    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);

    const uint8_t down[4] = {0x0B, 0xB0, 64, 127};
    const uint8_t stillDown[4] = {0x0B, 0xB0, 64, 100};
    const uint8_t up[4] = {0x0B, 0xB0, 64, 0};

    // Direct synth target: parsed but not applied (historical R6 policy).
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::SynthA;
    assert(dispatcher.setConfig(config));
    {
        const ParseResult parsed = parser.usbPacket(down, 100);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
        (void)dispatcher.service(queue);
        assert(bridge.sustain.empty() && routed.events.empty());
    }

    // PERFORM: press and release are edges, a repeated press is not a new edge.
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));
    for (const auto* packet : {&down, &stillDown, &up}) {
        const ParseResult parsed = parser.usbPacket(*packet, 200);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
    }
    (void)dispatcher.service(queue);
    assert(bridge.sustain.size() == 2u && bridge.sustain[0] && !bridge.sustain[1]);

    // A held button is released when the session goes away (no stuck LATCH).
    {
        const ParseResult parsed = parser.usbPacket(down, 300);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
        (void)dispatcher.service(queue);
        assert(bridge.sustain.size() == 3u && bridge.sustain[2]);
        io.usbDetached();
        (void)dispatcher.service(queue);
        assert(bridge.sustain.size() == 4u && !bridge.sustain[3]);
    }
    // A policy change away from PERFORM while held releases it too.
    io.usbAttached();
    io.usbReady(true, true);
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    {
        const ParseResult parsed = parser.usbPacket(down, 400);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
        (void)dispatcher.service(queue);
        assert(bridge.sustain.size() == 5u && bridge.sustain[4]);
        config.target = MidiInputTarget::SynthB;
        assert(dispatcher.setConfig(config));
        assert(bridge.sustain.size() == 6u && !bridge.sustain[5]);
    }
}

void pitchButtonsAreOneShotNudgesOnPerformOnly() {
    MusicalEventRouter router;
    CaptureSink routed;
    assert(router.addSink(routed));
    CaptureNotes bridge;
    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);

    // USB-MIDI pitch bend packets: CIN 0x0E, status 0xE0, LSB, MSB (centre = 64).
    const uint8_t left[4] = {0x0E, 0xE0, 0, 0};          // button pressed: extreme low
    const uint8_t centre[4] = {0x0E, 0xE0, 0, 64};       // spring back
    const uint8_t right[4] = {0x0E, 0xE0, 127, 127};     // extreme high
    const uint8_t rightHeld[4] = {0x0E, 0xE0, 100, 120};

    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::SynthA;
    assert(dispatcher.setConfig(config));
    {
        const ParseResult parsed = parser.usbPacket(left, 100);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
        (void)dispatcher.service(queue);
        assert(bridge.nudges.empty() && routed.events.empty());   // direct targets ignore it
    }

    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));
    for (const auto* packet : {&left, &centre, &right, &rightHeld, &centre, &left}) {
        const ParseResult parsed = parser.usbPacket(*packet, 200);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
    }
    (void)dispatcher.service(queue);
    // left press, (centre), right press, (right again is not a new edge), (centre), left press
    assert(bridge.nudges.size() == 3u);
    assert(bridge.nudges[0] == -1 && bridge.nudges[1] == 1 && bridge.nudges[2] == -1);
    assert(routed.events.empty());                                 // never reaches the router
}

void modButtonIsAOneShotPressOnPerformOnly() {
    MusicalEventRouter router;
    CaptureSink routed;
    assert(router.addSink(routed));
    CaptureNotes bridge;
    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);

    const uint8_t press[4] = {0x0B, 0xB0, 1, 127};
    const uint8_t held[4] = {0x0B, 0xB0, 1, 120};
    const uint8_t release[4] = {0x0B, 0xB0, 1, 0};

    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::SynthA;
    assert(dispatcher.setConfig(config));
    {
        const ParseResult parsed = parser.usbPacket(press, 100);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
        (void)dispatcher.service(queue);
        assert(bridge.mods == 0 && routed.events.empty());   // direct targets ignore it
    }
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));
    for (const auto* packet : {&press, &held, &release, &press}) {
        const ParseResult parsed = parser.usbPacket(*packet, 200);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
    }
    (void)dispatcher.service(queue);
    assert(bridge.mods == 2);                                // two presses, the repeat is not an edge
    assert(routed.events.empty());
}

int main() {
    modButtonIsAOneShotPressOnPerformOnly();
    pitchButtonsAreOneShotNudgesOnPerformOnly();
    sustainMapsToThePerformBridgeOnly();
    performTargetFeedsTheKeyboardBridgeNotTheRouter();
    usbNoteOnOffReachesConfiguredMusicalTarget();
    defaultConfigIsOffAndGenerationChangeReleasesOwnedNotes();
    reconfigurationReleasesNotesOwnedByPreviousPolicy();
    return 0;
}
