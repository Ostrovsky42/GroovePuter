#include <cassert>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <vector>

#include "src/input/musical_event_router.h"
#include "src/midi/midi_input_dispatcher.h"
#include "src/midi/midi_input_parser.h"
#include "src/midi/midi_input_queue.h"
#include "src/midi/midi_io_state.h"
#include "src/midi/external_note_queue.h"
#include "src/input/performance_control.h"

using namespace GroovePuterMidi;

namespace {
template <typename T, typename = void>
struct HasCableField : std::false_type {};

template <typename T>
struct HasCableField<T, std::void_t<decltype(std::declval<T>().cable)>>
    : std::true_type {};

template <typename Key>
void checkParsedCable(const Key& key) {
    if constexpr (HasCableField<Key>::value) {
        assert(key.cable == 1);
    } else {
        assert(false && "parsed input identity must retain the USB cable number");
    }
}

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

void usbCableIdentitySurvivesPacketParsing() {
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, 7});
    const uint8_t cableOneNoteOn[4] = {0x19, 0x90, 60, 100};
    const ParseResult parsed = parser.usbPacket(cableOneNoteOn, 1234);
    assert(parsed.hasInput && "USB cable 1 note should be accepted");
    checkParsedCable(parsed.input.id);
}

void genericPerformanceCcIsParsedCoalescedAndMappedAbsolutely() {
    MusicalEventRouter router;
    MidiIoState io = readyUsbInput();
    MidiInputQueue inputQueue;
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    ExternalNoteQueue performQueue;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&performQueue);

    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.channelMode = MidiInputChannelMode::Single;
    config.channel = 0;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const uint8_t arpOn[4] = {0x0b, 0xb0, 80, 127};
    const uint8_t arpOff[4] = {0x0b, 0xb0, 80, 0};
    const uint8_t latchOn[4] = {0x0b, 0xb0, 81, 64};
    const ParseResult parsedOn = parser.usbPacket(arpOn, 10);
    const ParseResult parsedOff = parser.usbPacket(arpOff, 20);
    const ParseResult parsedLatch = parser.usbPacket(latchOn, 30);
    assert(parsedOn.hasInput && parsedOn.input.kind == InputKind::ControlChange);
    assert(parsedOn.input.id.key == 80 && parsedOn.input.velocity == 127);
    assert(parsedOff.hasInput && parsedLatch.hasInput);
    assert(inputQueue.tryPush(parsedOn.input));
    assert(inputQueue.tryPush(parsedOff.input));
    assert(inputQueue.tryPush(parsedLatch.input));
    assert(dispatcher.service(inputQueue) == 3u);

    ExternalNoteQueue::Event event{};
    assert(performQueue.pop(event) && event.controlChange);
    assert(event.controller == 80 && event.velocity == 0);
    PerformanceIntent intent{};
    assert(GroovePuterPerformance::makeGenericMidiToggleIntent(
        event.controller, event.velocity, event.input, intent));
    assert(intent.kind == PerformanceIntentKind::SetArpEnabled && intent.value == 0);

    assert(performQueue.pop(event) && event.controlChange);
    assert(event.controller == 81 && event.velocity == 64);
    assert(GroovePuterPerformance::makeGenericMidiToggleIntent(
        event.controller, event.velocity, event.input, intent));
    assert(intent.kind == PerformanceIntentKind::SetLatchEnabled && intent.value == 1);
    assert(!performQueue.pop(event));

    assert(dispatcher.setConfig(MidiInputRoutingConfig{
        true, MidiInputChannelMode::Single, 1, MidiInputTarget::Perform}));
    const uint8_t wrongChannel[4] = {0x0b, 0xb0, 80, 127};
    const ParseResult wrong = parser.usbPacket(wrongChannel, 40);
    assert(wrong.hasInput && inputQueue.tryPush(wrong.input));
    assert(dispatcher.service(inputQueue) == 1u);
    assert(!performQueue.pop(event));
}

void finalConsumerRejectsStaleInputIdentity() {
    MidiIoState io = readyUsbInput();
    const uint32_t oldGeneration = io.usbInputGeneration();
    assert(io.acceptsInputIdentity(InputSource::Usb, oldGeneration));
    io.usbDetached();
    assert(!io.acceptsInputIdentity(InputSource::Usb, oldGeneration));
    io.usbAttached();
    io.usbReady(true, true);
    assert(!io.acceptsInputIdentity(InputSource::Usb, oldGeneration));
    assert(io.acceptsInputIdentity(InputSource::Usb, io.usbInputGeneration()));
}
}  // namespace

class CaptureNotes final : public MidiExternalNoteSink {
public:
    struct Entry { bool on; uint8_t note; uint8_t velocity; };
    void externalNoteOn(uint8_t note, uint8_t velocity) override { entries.push_back({true, note, velocity}); }
    void externalNoteOff(uint8_t note) override { entries.push_back({false, note, 0}); }
    void externalSustain(bool down) override { sustain.push_back(down); }
    void externalNoteOn(uint8_t note, uint8_t velocity,
                        PerformanceInputIdentity input) override {
        externalNoteOn(note, velocity);
        identities.push_back(input);
    }
    void externalNoteOff(uint8_t note, PerformanceInputIdentity input) override {
        externalNoteOff(note);
        identities.push_back(input);
    }
    void externalSustain(bool down, PerformanceInputIdentity input) override {
        externalSustain(down);
        sustainIdentities.push_back(input);
    }
    void externalAllSoundOff(PerformanceInputIdentity input) override {
        allSoundOff.push_back(input);
    }
    void externalNudge(int direction) override { nudges.push_back(direction); }
    void externalMod(ModPhase phase) override {
        modPhases.push_back(phase);
        if (phase == ModPhase::Press) ++mods;
    }
    std::vector<ModPhase> modPhases;
    int mods = 0;  // presses
    std::vector<int> nudges;
    std::vector<bool> sustain;
    std::vector<Entry> entries;
    std::vector<PerformanceInputIdentity> identities;
    std::vector<PerformanceInputIdentity> sustainIdentities;
    std::vector<PerformanceInputIdentity> allSoundOff;
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

void staleGenerationPitchBendCannotAffectReconnectedInput() {
    MusicalEventRouter router;
    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    CaptureNotes bridge;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.channelMode = MidiInputChannelMode::Omni;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const uint32_t oldGeneration = io.usbInputGeneration();
    io.usbDetached();
    io.usbAttached();
    io.usbReady(true, true);
    assert(io.usbInputGeneration() != oldGeneration);

    assert(queue.tryPush(MidiInputEvent{
        InputKey{InputSource::Usb, oldGeneration, 0, 0, 0},
        InputKind::PitchBend, 0, 123}));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.nudges.empty());
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
    assert(dispatcher.setConfig(config));

    auto send = [&](uint8_t status, uint8_t d1, uint8_t d2, uint8_t cin) {
        const uint8_t packet[4] = {cin, status, d1, d2};
        const ParseResult parsed = parser.usbPacket(packet, 100);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
        (void)dispatcher.service(queue);
    };

    // Mod = CC 0x5E on channel 9: a 0..15 ramp up, then back down to 0 (measured on the device).
    // Two presses, so exactly two one-shots; the ramp down never fires.
    for (int press = 0; press < 2; ++press) {
        for (uint8_t v = 1; v <= 15; ++v) send(0xB8, 0x5E, v, 0x0B);
        for (int v = 14; v >= 0; --v) send(0xB8, 0x5E, static_cast<uint8_t>(v), 0x0B);
    }
    assert(bridge.mods == 2);
    // Each press produced exactly one Press and one Release, in order; the second ramp ended at 0.
    assert(bridge.modPhases.size() == 4u);
    assert(bridge.modPhases[0] == ModPhase::Press && bridge.modPhases[1] == ModPhase::Release);
    assert(bridge.modPhases[2] == ModPhase::Press && bridge.modPhases[3] == ModPhase::Release);
    // A press that starts from a small value after a partial release (value 2) still counts once.
    send(0xB8, 0x5E, 3, 0x0B);
    assert(bridge.mods == 3);
    // Pulling the keyboard out while the button is held cancels the press (no Release is faked).
    io.usbDetached();
    (void)dispatcher.service(queue);
    assert(bridge.modPhases.back() == ModPhase::Cancel);
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

void performNotesKeepFullInputIdentityThroughDispatch() {
    MusicalEventRouter router;
    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    CaptureNotes bridge;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const InputKey key{InputSource::Usb, io.usbInputGeneration(), 2, 60, 1};
    assert(queue.tryPush(MidiInputEvent{key, InputKind::NoteOn, 100, 1}));
    assert(queue.tryPush(MidiInputEvent{key, InputKind::NoteOff, 0, 2}));
    assert(dispatcher.service(queue) == 2u);
    assert(bridge.identities.size() == 2u);
    assert(samePerformanceInput(bridge.identities[0], PerformanceInputIdentity{
        key.generation, static_cast<uint8_t>(key.source), key.channel, key.key, key.cable}));
    assert(samePerformanceInput(bridge.identities[0], bridge.identities[1]));
}

void sustainOwnershipIsPerSourceChannelAndCable() {
    MusicalEventRouter router;
    MidiIoState io = readyUsbInput();
    io.setRoutes(MidiRoutes{true, true, false, false});
    MidiInputQueue queue;
    CaptureNotes bridge;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const uint32_t usbGen = io.usbInputGeneration();
    const uint32_t uartGen = io.uartInputGeneration();
    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Usb, usbGen, 0, 64, 0},
                                        InputKind::Sustain, 127, 1}));
    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Uart, uartGen, 0, 64, 0},
                                        InputKind::Sustain, 127, 2}));
    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Usb, usbGen, 0, 64, 0},
                                        InputKind::Sustain, 0, 3}));
    assert(dispatcher.service(queue) == 3u);
    assert(bridge.sustain.size() == 3u);
    assert(bridge.sustain[0] && bridge.sustain[1] && !bridge.sustain[2]);
    assert(bridge.sustainIdentities.size() == 3u);
    assert(bridge.sustainIdentities[0].source != bridge.sustainIdentities[1].source);
}

void sustainOwnerOverflowIsObservableAndSourceScoped() {
    MusicalEventRouter router;
    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    CaptureNotes bridge;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const uint32_t generation = io.usbInputGeneration();
    for (std::size_t i = 0; i < MidiInputDispatcher::kMaxSustainOwners; ++i) {
        const uint8_t channel = static_cast<uint8_t>(i % 16u);
        const uint8_t cable = static_cast<uint8_t>(i / 16u);
        assert(queue.tryPush(MidiInputEvent{
            InputKey{InputSource::Usb, generation, channel, 64, cable},
            InputKind::Sustain, 127, static_cast<uint32_t>(i + 1u)}));
    }
    // A new cable/channel identity is refused at capacity. It is not forwarded
    // to PERFORM and cannot release a different source's sustain owner.
    const InputKey overflow{InputSource::Usb, generation, 0, 64, 2};
    assert(queue.tryPush(MidiInputEvent{overflow, InputKind::Sustain, 127, 100}));
    assert(dispatcher.service(queue) == MidiInputDispatcher::kMaxSustainOwners + 1u);
    assert(bridge.sustain.size() == MidiInputDispatcher::kMaxSustainOwners);
    assert(dispatcher.sustainOwnerOverflowCount(InputSource::Usb) == 1u);
    assert(dispatcher.sustainOwnerOverflowCount(InputSource::Uart) == 0u);

    assert(queue.tryPush(MidiInputEvent{overflow, InputKind::Sustain, 0, 101}));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.sustain.size() == MidiInputDispatcher::kMaxSustainOwners);
    assert(dispatcher.sustainOwnerOverflowCount(InputSource::Usb) == 1u);

    const InputKey first{InputSource::Usb, generation, 0, 64, 0};
    assert(queue.tryPush(MidiInputEvent{first, InputKind::Sustain, 0, 102}));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.sustain.size() == MidiInputDispatcher::kMaxSustainOwners + 1u);
    assert(!bridge.sustain.back());

    // A freed slot admits the previously rejected identity on retry.
    assert(queue.tryPush(MidiInputEvent{overflow, InputKind::Sustain, 127, 103}));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.sustain.size() == MidiInputDispatcher::kMaxSustainOwners + 2u);
    assert(bridge.sustain.back());
    assert(dispatcher.sustainOwnerOverflowCount(InputSource::Usb) == 1u);
}

void allSoundOffIsDistinctFromAllNotesOffForPerform() {
    MusicalEventRouter router;
    MidiIoState io = readyUsbInput();
    MidiInputQueue queue;
    CaptureNotes bridge;
    MidiInputParser parser;
    parser.reset(InputSession{InputSource::Usb, io.usbInputGeneration()});
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const uint8_t noteOn[4] = {0x09, 0x90, 60, 100};
    const uint8_t allNotesOff[4] = {0x0B, 0xB0, 123, 0};
    const uint8_t allSoundOff[4] = {0x0B, 0xB0, 120, 0};
    for (const auto* packet : {&noteOn, &allNotesOff, &noteOn, &allSoundOff}) {
        const ParseResult parsed = parser.usbPacket(*packet, 100);
        assert(parsed.hasInput && queue.tryPush(parsed.input));
    }
    assert(dispatcher.service(queue) == 4u);
    assert(bridge.entries.size() == 3u);
    assert(bridge.entries[0].on && !bridge.entries[1].on && bridge.entries[2].on);
    assert(bridge.allSoundOff.size() == 1u);
    assert(bridge.allSoundOff[0].source == static_cast<uint8_t>(InputSource::Usb));
}

void inputQueueOverflowRecoveryIsScopedToTheSourceSession() {
    MusicalEventRouter router;
    MidiIoState io = readyUsbInput();
    io.setRoutes(MidiRoutes{true, true, false, false});
    MidiInputQueue queue;
    CaptureNotes bridge;
    MidiInputDispatcher dispatcher(router, io);
    dispatcher.setPerformSink(&bridge);
    MidiInputRoutingConfig config{};
    config.enabled = true;
    config.target = MidiInputTarget::Perform;
    assert(dispatcher.setConfig(config));

    const uint32_t usbGeneration = io.usbInputGeneration();
    const uint32_t uartGeneration = io.uartInputGeneration();
    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Usb, usbGeneration, 0, 60, 0},
                                        InputKind::NoteOn, 100, 1}));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.entries.size() == 1u && bridge.entries[0].on);

    for (std::size_t i = 0; i < MidiInputQueue::kNoteOnCapacity; ++i)
        assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Uart, uartGeneration, 0,
                                                    static_cast<uint8_t>(40 + i), 0},
                                            InputKind::NoteOn, 90, 2}));
    for (std::size_t i = 0; i < MidiInputQueue::kCriticalReserve; ++i)
        assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Uart, uartGeneration, 0,
                                                    static_cast<uint8_t>(40 + i), 0},
                                            InputKind::NoteOff, 0, 3}));
    assert(!queue.tryPush(MidiInputEvent{InputKey{InputSource::Uart, uartGeneration, 0, 40, 0},
                                         InputKind::NoteOff, 0, 4}));
    (void)dispatcher.service(queue);
    assert(bridge.entries.size() == 1u); // a UART overflow cannot release the USB-owned note

    assert(queue.tryPush(MidiInputEvent{InputKey{InputSource::Usb, usbGeneration, 0, 60, 0},
                                        InputKind::NoteOff, 0, 5}));
    assert(dispatcher.service(queue) == 1u);
    assert(bridge.entries.size() == 2u && !bridge.entries.back().on);
}

int main() {
    rampedButtonsOfTheOwnersKeyboardCountOncePerPress();
    modButtonIsAOneShotPressOnPerformOnly();
    pitchButtonsAreOneShotNudgesOnPerformOnly();
    staleGenerationPitchBendCannotAffectReconnectedInput();
    sustainMapsToThePerformBridgeOnly();
    performTargetFeedsTheKeyboardBridgeNotTheRouter();
    usbNoteOnOffReachesConfiguredMusicalTarget();
    defaultConfigIsOffAndGenerationChangeReleasesOwnedNotes();
    reconfigurationReleasesNotesOwnedByPreviousPolicy();
    usbCableIdentitySurvivesPacketParsing();
    genericPerformanceCcIsParsedCoalescedAndMappedAbsolutely();
    finalConsumerRejectsStaleInputIdentity();
    performNotesKeepFullInputIdentityThroughDispatch();
    sustainOwnershipIsPerSourceChannelAndCable();
    sustainOwnerOverflowIsObservableAndSourceScoped();
    allSoundOffIsDistinctFromAllNotesOffForPerform();
    inputQueueOverflowRecoveryIsScopedToTheSourceSession();
    return 0;
}
