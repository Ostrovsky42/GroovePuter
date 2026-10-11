#include <cassert>
#include <cstdint>
#include <vector>

#include "src/input/musical_event_router.h"
#include "src/midi/midi_input_dispatcher.h"
#include "src/midi/midi_input_parser.h"
#include "src/midi/midi_input_queue.h"
#include "src/midi/external_note_queue.h"
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
    config.pitchBendMode = PitchBendMode::LegacyNudge;
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
void genericModulationControllersStayControlChanges() {
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, 1});
    const uint8_t modWheel[4] = {0x0B, 0xB0, 1, 127};
    const uint8_t nanoKeyMod[4] = {0x0B, 0xB0, 94, 127};
    const ParseResult cc1 = parser.usbPacket(modWheel, 100);
    const ParseResult cc94 = parser.usbPacket(nanoKeyMod, 200);
    assert(cc1.hasInput && cc1.input.kind == InputKind::ControlChange);
    assert(cc1.input.controller == 1u && cc1.input.velocity == 127u);
    assert(cc94.hasInput && cc94.input.kind == InputKind::ControlChange);
    assert(cc94.input.controller == 94u && cc94.input.velocity == 127u);
}

}  // namespace

class CaptureNotes final : public MidiExternalNoteSink {
public:
    struct Entry { bool on; uint8_t note; uint8_t velocity; PerformanceInputIdentity input; };
    void externalNoteOn(uint8_t note, uint8_t velocity, PerformanceInputIdentity input) override {
        entries.push_back({true, note, velocity, input});
    }
    void externalNoteOff(uint8_t note, PerformanceInputIdentity input) override {
        entries.push_back({false, note, 0, input});
    }
    void externalSustain(bool down) override { sustain.push_back(down); }
    void externalSustain(bool down, PerformanceInputIdentity input) override {
        sustain.push_back(down);
        sustainInputs.push_back(input);
    }
    void externalNudge(int direction) override { nudges.push_back(direction); }
    void externalPitchBend(uint16_t value14, PerformanceInputIdentity input) override {
        bends.push_back({value14, input});
    }
    void externalAllSoundOff(PerformanceInputIdentity input) override {
        allSoundOffs.push_back(input);
    }
    void externalMod(ModPhase phase) override {
        modPhases.push_back(phase);
        if (phase == ModPhase::Press) ++mods;
    }
    void externalControlChange(uint8_t cc, uint8_t value,
                               PerformanceInputIdentity input) override {
        controls.push_back({cc, value, input});
    }
    struct Control { uint8_t cc; uint8_t value; PerformanceInputIdentity input; };
    struct Bend { uint16_t value14; PerformanceInputIdentity input; };
    std::vector<ModPhase> modPhases;
    int mods = 0;  // presses
    std::vector<int> nudges;
    std::vector<bool> sustain;
    std::vector<PerformanceInputIdentity> sustainInputs;
    std::vector<Entry> entries;
    std::vector<Control> controls;
    std::vector<Bend> bends;
    std::vector<PerformanceInputIdentity> allSoundOffs;
};

void allSoundOffUsesForceScopedPerformCleanup() {
    MusicalEventRouter router;
    CaptureSink routed;
    assert(router.addSink(routed));
    CaptureNotes bridge;
    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const uint32_t generation = io.usbInputGeneration();
    assert(queue.tryPush(MidiInputEvent{
        InputKey{InputSource::Usb, generation, 4, 60}, InputKind::NoteOn, 100, 1}));
    assert(queue.tryPush(MidiInputEvent{
        InputKey{InputSource::Usb, generation, 4, 0}, InputKind::AllSoundOff, 0, 2}));
    assert(dispatcher.service(queue) == 2u);
    assert(bridge.entries.size() == 1u && bridge.entries[0].on);
    assert(bridge.allSoundOffs.size() == 1u);
    const PerformanceInputIdentity scoped = bridge.allSoundOffs[0];
    assert(scoped.generation == generation);
    assert(scoped.source == static_cast<uint8_t>(InputSource::Usb));
    assert(scoped.channel == 4u);
    assert(routed.events.empty());

    // CC123 remains ordinary NoteOff and therefore honors the keyboard's sustain policy.
    assert(queue.tryPush(MidiInputEvent{
        InputKey{InputSource::Usb, generation, 4, 62}, InputKind::NoteOn, 100, 3}));
    assert(queue.tryPush(MidiInputEvent{
        InputKey{InputSource::Usb, generation, 4, 0}, InputKind::AllNotesOff, 0, 4}));
    assert(dispatcher.service(queue) == 2u);
    assert(bridge.entries.size() == 3u && !bridge.entries[2].on);
    assert(bridge.allSoundOffs.size() == 1u);
}

void musicalPitchBendPreservesFourteenBitsByDefault() {
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
    config.target = MidiInputTarget::Perform;
    assert(config.pitchBendMode == PitchBendMode::Musical);
    assert(dispatcher.setConfig(config));

    const uint8_t bend[4] = {0x0e, 0xe3, 0x23, 0x65};
    const ParseResult parsed = parser.usbPacket(bend, 1234);
    assert(parsed.hasInput && parsed.input.pitchBend14 == 0x32a3u);
    assert(queue.tryPush(parsed.input));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.bends.size() == 1u);
    assert(bridge.bends[0].value14 == 0x32a3u);
    assert(bridge.bends[0].input.generation == io.usbInputGeneration());
    assert(bridge.bends[0].input.channel == 3u);
    assert(bridge.nudges.empty() && routed.events.empty());

    config.pitchBendMode = PitchBendMode::LegacyNudge;
    assert(dispatcher.setConfig(config));
    assert(bridge.bends.size() == 2u && bridge.bends[1].value14 == 8192u);
    const uint8_t legacyPress[4] = {0x0e, 0xe3, 0, 0};
    assert(queue.tryPush(parser.usbPacket(legacyPress, 1250).input));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.nudges.size() == 1u && bridge.nudges[0] == -1);
    assert(bridge.bends.size() == 2u);

    config.pitchBendMode = PitchBendMode::Musical;
    assert(dispatcher.setConfig(config));
    assert(queue.tryPush(parser.usbPacket(bend, 1300).input));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.bends.size() == 3u && bridge.bends.back().value14 == 0x32a3u);
    io.usbDetached();
    assert(dispatcher.service(queue) == 0u);
    assert(bridge.bends.size() == 4u && bridge.bends.back().value14 == 8192u);
}

void genericControlChangesReachPerformAsSemanticInput() {
    MusicalEventRouter router;
    CaptureNotes bridge;
    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const uint8_t cc[4] = {0x0b, 0xb3, 74, 91};
    const ParseResult parsed = parser.usbPacket(cc, 1234);
    assert(parsed.hasInput && parsed.input.kind == InputKind::ControlChange);
    assert(parsed.input.controller == 74 && parsed.input.velocity == 91);
    assert(queue.tryPush(parsed.input));
    assert(dispatcher.service(queue) == 1);
    assert(bridge.controls.size() == 1);
    assert(bridge.controls[0].cc == 74 && bridge.controls[0].value == 91);
    assert(bridge.controls[0].input.generation == io.usbInputGeneration());
    assert(bridge.controls[0].input.channel == 3);
}

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
    assert(bridge.entries[0].input.generation == io.usbInputGeneration());
    assert(bridge.entries[0].input.source == static_cast<uint8_t>(InputSource::Usb));
    assert(bridge.entries[0].input.channel == 0 && bridge.entries[0].input.inputNote == 60);
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

void performBridgeRetainsInputIdentityAcrossSamePitchOwners() {
    MusicalEventRouter router;
    CaptureSink routed;
    assert(router.addSink(routed));
    CaptureNotes bridge;

    MidiIoState io;
    io.setRoutes(MidiRoutes{true, true, false, false});
    io.requestUsbRole(UsbRole::Device);
    io.boot();
    io.usbAttached();
    io.usbReady(true, true);
    const uint32_t usbGeneration = io.usbInputGeneration();
    const uint32_t uartGeneration = io.uartInputGeneration();
    MidiInputQueue queue;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    MidiInputEvent usbOn{InputKey{InputSource::Usb, usbGeneration, 1, 64}, InputKind::NoteOn, 90, 100};
    MidiInputEvent uartOn{InputKey{InputSource::Uart, uartGeneration, 1, 64}, InputKind::NoteOn, 80, 110};
    assert(queue.tryPush(usbOn) && queue.tryPush(uartOn));
    assert(dispatcher.service(queue) == 2u);
    assert(bridge.entries.size() == 2u);
    assert(bridge.entries[0].input.source == static_cast<uint8_t>(InputSource::Usb));
    assert(bridge.entries[1].input.source == static_cast<uint8_t>(InputSource::Uart));
    assert(bridge.entries[0].input.generation == usbGeneration);
    assert(bridge.entries[1].input.generation == uartGeneration);
    assert(bridge.entries[0].input.channel == 1 && bridge.entries[0].input.inputNote == 64);

    // A source-scoped disconnect releases only its note identity.
    io.usbDetached();
    (void)dispatcher.service(queue);
    assert(bridge.entries.size() == 3u);
    assert(!bridge.entries.back().on);
    assert(bridge.entries.back().input.source == static_cast<uint8_t>(InputSource::Usb));
    assert(bridge.entries.back().input.generation == usbGeneration);
}

void externalQueuePreservesPerformanceInputIdentity() {
    GroovePuterMidi::ExternalNoteQueue queue;
    const PerformanceInputIdentity identity{99, static_cast<uint8_t>(InputSource::Usb), 4, 67};
    queue.externalNoteOn(67, 101, identity);
    GroovePuterMidi::ExternalNoteQueue::Event event{};
    assert(queue.pop(event) && event.on && event.note == 67 && event.velocity == 101);
    assert(samePerformanceInput(event.input, identity));
    queue.externalNoteOff(67, identity);
    assert(queue.pop(event) && !event.on && event.note == 67);
    assert(samePerformanceInput(event.input, identity));
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

void sustainOwnershipSurvivesAnotherSourceDisconnect() {
    MusicalEventRouter router;
    CaptureSink routed;
    assert(router.addSink(routed));
    CaptureNotes bridge;
    MidiIoState io;
    io.setRoutes(MidiRoutes{true, true, false, false});
    io.requestUsbRole(UsbRole::Device);
    io.boot();
    io.usbAttached();
    io.usbReady(true, true);
    MidiInputQueue queue;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const uint32_t usbGeneration = io.usbInputGeneration();
    const uint32_t uartGeneration = io.uartInputGeneration();
    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Usb, usbGeneration, 0, 0},
                                        InputKind::Sustain, 127, 100}));
    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Uart, uartGeneration, 0, 0},
                                        InputKind::Sustain, 127, 110}));
    assert(dispatcher.service(queue) == 2u);
    assert(bridge.sustain.size() == 2 && bridge.sustain[0] && bridge.sustain[1]);
    assert(bridge.sustainInputs[0].source == static_cast<uint8_t>(InputSource::Usb));
    assert(bridge.sustainInputs[1].source == static_cast<uint8_t>(InputSource::Uart));

    io.usbDetached();
    (void)dispatcher.service(queue);
    assert(bridge.sustain.size() == 3 && !bridge.sustain[2]); // Only USB sustain is released.
    assert(bridge.sustainInputs[2].source == static_cast<uint8_t>(InputSource::Usb));

    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Uart, uartGeneration, 0, 0},
                                        InputKind::Sustain, 0, 120}));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.sustain.size() == 4 && !bridge.sustain[3]);
    assert(bridge.sustainInputs[3].source == static_cast<uint8_t>(InputSource::Uart));
}

void disconnectCancelsOnlyGesturesOwnedByThatSource() {
    MusicalEventRouter router;
    CaptureSink routed;
    assert(router.addSink(routed));
    CaptureNotes bridge;
    MidiIoState io;
    io.setRoutes(MidiRoutes{true, true, false, false});
    io.requestUsbRole(UsbRole::Device);
    io.boot();
    io.usbAttached();
    io.usbReady(true, true);
    MidiInputQueue queue;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    config.pitchBendMode = PitchBendMode::LegacyNudge;
    assert(dispatcher.setConfig(config));

    const uint32_t generation = io.usbInputGeneration();
    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Usb, generation, 0, 0},
                                        InputKind::PitchBend, 0, 100}));
    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Usb, generation, 0, 0},
                                        InputKind::Mod, 1, 110}));
    assert(dispatcher.service(queue) == 2u);
    assert(bridge.nudges.size() == 1 && bridge.nudges[0] == -1);
    assert(bridge.modPhases.size() == 1 && bridge.modPhases[0] == ModPhase::Press);

    io.usbDetached();
    (void)dispatcher.service(queue);
    assert(bridge.nudges.size() == 2 && bridge.nudges[1] == 0);
    assert(bridge.modPhases.size() == 2 && bridge.modPhases[1] == ModPhase::Cancel);
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
    config.pitchBendMode = PitchBendMode::LegacyNudge;
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
    // left press, centre (release), right press, (right again is not a new edge), centre (release),
    // left press: the events are -1, 0, +1, 0, -1 (0 = the button was released)
    assert(bridge.nudges.size() == 5u);
    assert(bridge.nudges[0] == -1 && bridge.nudges[1] == 0 && bridge.nudges[2] == 1 &&
           bridge.nudges[3] == 0 && bridge.nudges[4] == -1);
    // The session ending while the button is down releases it (no stuck auto-repeat).
    io.usbDetached();
    (void)dispatcher.service(queue);
    assert(bridge.nudges.size() == 6u && bridge.nudges[5] == 0);
    assert(routed.events.empty());                                 // never reaches the router
}

void modulationCCNeverTriggersLegacyModGesture() {
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
    assert(bridge.mods == 0 && bridge.controls.empty() && routed.events.empty());
    }
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));
    for (const auto* packet : {&press, &held, &release, &press}) {
        const ParseResult parsed = parser.usbPacket(*packet, 200);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
    }
    (void)dispatcher.service(queue);
    assert(bridge.mods == 0 && bridge.controls.size() == 4u);
    assert(routed.events.empty());
}

void rampedButtonsOfTheOwnersKeyboardCountOncePerPress() {
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
    config.target = MidiInputTarget::Perform;
    config.pitchBendMode = PitchBendMode::LegacyNudge;
    assert(dispatcher.setConfig(config));

    auto send = [&](uint8_t status, uint8_t d1, uint8_t d2, uint8_t cin) {
        const uint8_t packet[4] = {cin, status, d1, d2};
        const ParseResult parsed = parser.usbPacket(packet, 100);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
        (void)dispatcher.service(queue);
    };

    // CC94 uses a measured 0..15 button ramp on one controller, but stays generic CC
    // data so an unknown controller cannot invoke destructive page actions.
    for (int press = 0; press < 2; ++press) {
        for (uint8_t v = 1; v <= 15; ++v) send(0xB8, 0x5E, v, 0x0B);
        for (int v = 14; v >= 0; --v) send(0xB8, 0x5E, static_cast<uint8_t>(v), 0x0B);
    }
    assert(bridge.mods == 0 && bridge.modPhases.empty());
    assert(bridge.controls.size() == 60u);
    // A partial ramp stays control data too.
    send(0xB8, 0x5E, 3, 0x0B);
    assert(bridge.mods == 0 && bridge.controls.size() == 61u);
    // Disconnect does not invent a destructive Mod action.
    io.usbDetached();
    (void)dispatcher.service(queue);
    assert(bridge.modPhases.empty());
    io.usbAttached();
    io.usbReady(true, true);
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});

    // Pitch button left: a short touch only reaches MSB 54, then springs back; it must count once.
    for (int v = 63; v >= 54; --v) send(0xE8, 0, static_cast<uint8_t>(v), 0x0E);
    for (int v = 55; v <= 64; ++v) send(0xE8, 0, static_cast<uint8_t>(v), 0x0E);
    assert(bridge.nudges.size() == 2u && bridge.nudges[0] == -1 && bridge.nudges[1] == 0);  // press, release
    // A long hold goes on to 0 and is still one press; the button to the right gives +1.
    for (int v = 63; v >= 0; --v) send(0xE8, 0, static_cast<uint8_t>(v), 0x0E);
    for (int v = 1; v <= 64; ++v) send(0xE8, 0, static_cast<uint8_t>(v), 0x0E);
    for (int v = 65; v <= 127; ++v) send(0xE8, 0, static_cast<uint8_t>(v), 0x0E);
    for (int v = 126; v >= 64; --v) send(0xE8, 0, static_cast<uint8_t>(v), 0x0E);
    assert(bridge.nudges.size() == 6u);
    assert(bridge.nudges[2] == -1 && bridge.nudges[3] == 0 && bridge.nudges[4] == 1 && bridge.nudges[5] == 0);
    // A tiny wobble around the centre never fires (below the press deviation).
    for (int v : {62, 66, 61, 67, 64}) send(0xE8, 0, static_cast<uint8_t>(v), 0x0E);
    assert(bridge.nudges.size() == 6u);
    assert(routed.events.empty());
}

int main() {
    genericModulationControllersStayControlChanges();
    allSoundOffUsesForceScopedPerformCleanup();
    musicalPitchBendPreservesFourteenBitsByDefault();
    genericControlChangesReachPerformAsSemanticInput();
    rampedButtonsOfTheOwnersKeyboardCountOncePerPress();
    modulationCCNeverTriggersLegacyModGesture();
    pitchButtonsAreOneShotNudgesOnPerformOnly();
    sustainMapsToThePerformBridgeOnly();
    sustainOwnershipSurvivesAnotherSourceDisconnect();
    disconnectCancelsOnlyGesturesOwnedByThatSource();
    performTargetFeedsTheKeyboardBridgeNotTheRouter();
    performBridgeRetainsInputIdentityAcrossSamePitchOwners();
    externalQueuePreservesPerformanceInputIdentity();
    usbNoteOnOffReachesConfiguredMusicalTarget();
    defaultConfigIsOffAndGenerationChangeReleasesOwnedNotes();
    reconfigurationReleasesNotesOwnedByPreviousPolicy();
    return 0;
}
