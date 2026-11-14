// D5 integration: UsbMidiOutput over the USB+DIN tee. A refused NoteOff must
// reach the DIN wire through the bounded retry cadence alone, and a note shared
// by two owners must not be cut by the first owner's release.
#include <cassert>
#include <cstdint>
#include <vector>

#include "src/input/musical_event_router.h"
#include "src/midi/midi_pattern_startup_routes.h"
#include "src/midi/tee_midi_transport.h"
#include "src/midi/usb_midi_output.h"

namespace {

struct Wire {
    uint8_t status;
    uint8_t channel;
    uint8_t note;
};

class ScriptedTransport final : public IMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return mountedFlag; }
    MidiTransportLink linkKind() const override {
        return MidiTransportLink::Enumerated;
    }
    bool sendNoteOn(uint8_t c, uint8_t n, uint8_t) override {
        if (!mountedFlag || rejectAll) return false;
        log.push_back({0x90, c, n});
        return true;
    }
    bool sendNoteOff(uint8_t c, uint8_t n, uint8_t) override {
        if (!mountedFlag) return false;
        if (rejectNoteOffs > 0) {
            --rejectNoteOffs;
            ++refusedNoteOffs;
            return false;
        }
        if (rejectAll) return false;
        log.push_back({0x80, c, n});
        return true;
    }
    bool sendControlChange(uint8_t c, uint8_t cc, uint8_t) override {
        if (!mountedFlag || rejectAll) return false;
        log.push_back({0xB0, c, cc});
        return true;
    }
    bool sendTimingClock() override { return true; }
    bool sendStart() override { return true; }
    bool sendContinue() override { return true; }
    bool sendStop() override { return true; }
    bool sendSongPositionPointer(uint16_t) override { return true; }
    void flush() override {}

    bool mountedFlag{true};
    bool rejectAll{false};
    int rejectNoteOffs{0};
    int refusedNoteOffs{0};
    std::vector<Wire> log;

    int count(uint8_t status, uint8_t note) const {
        int total = 0;
        for (const Wire& w : log) {
            if (w.status == status && w.note == note) ++total;
        }
        return total;
    }
};

MusicalEvent event(MusicalEventType type,
                   uint8_t note,
                   MusicalEventSource source,
                   MusicalEventTarget target = MusicalEventTarget::SynthA,
                   uint8_t flags = 0) {
    const uint8_t velocity = type == MusicalEventType::NoteOn ? 100u : 0u;
    return MusicalEvent{type, source, target, 0, note, velocity, flags};
}

// Primary refuses every NoteOff. DIN accepts. The note must still reach DIN:
// once the primary is stalled the tee demotes it, and the cadence keeps retrying
// without any further musical event. The demotion comes after the tee's stall
// threshold of refusals, so DIN sounds for that bounded window.
void testUsbStalledNoteOffReachesDinThroughCadence() {
    ScriptedTransport usb;
    ScriptedTransport din;
    GroovePuterMidi::TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);
    UsbMidiOutput output(tee, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60,
                                    MusicalEventSource::PatternPlayer));
    assert(usb.count(0x90, 60) == 1);
    assert(din.count(0x90, 60) == 1);

    usb.rejectNoteOffs = 1000;
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 60,
                                    MusicalEventSource::PatternPlayer));
    assert(din.count(0x80, 60) == 0);  // authoritative USB held DIN back

    uint32_t now = 10000u;
    int attempts = 0;
    while (din.count(0x80, 60) == 0 && attempts < 200) {
        output.servicePendingReleases(now);
        now += UsbMidiOutput::kPendingReleaseMaxIntervalUs;
        ++attempts;
    }
    assert(din.count(0x80, 60) == 1);
    assert(attempts <= static_cast<int>(
        GroovePuterMidi::TeeMidiTransport::kPrimaryStallRejects) + 8);
    assert(usb.count(0x80, 60) == 0);
}

// Pattern holds a note whose NoteOff the wire refused. PERFORM then takes the
// same physical note on the same channel. The retry must not end the note while
// PERFORM still owns it; only PERFORM's own release may send the NoteOff.
void testSharedNoteRetryDoesNotCutSecondOwner() {
    ScriptedTransport usb;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 64,
                                    MusicalEventSource::PatternPlayer));
    assert(usb.count(0x90, 64) == 1);

    usb.rejectNoteOffs = 1;
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 64,
                                    MusicalEventSource::PatternPlayer));
    assert(usb.count(0x80, 64) == 0);

    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 64,
                                    MusicalEventSource::PerformanceKeyboard));
    assert(usb.count(0x90, 64) == 1);  // no second wire NoteOn for one note

    assert(output.servicePendingReleases(1000u));
    assert(usb.count(0x80, 64) == 0);  // PERFORM still holds it: no cut

    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 64,
                                    MusicalEventSource::PerformanceKeyboard));
    assert(usb.count(0x80, 64) == 1);  // last owner releases it once
}

// Each lane family retries its own refused NoteOff without a new event.
void testMelodicLaneRetriesWithoutNewEvent() {
    ScriptedTransport usb;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60,
                                    MusicalEventSource::PatternPlayer));
    usb.rejectNoteOffs = 1;
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 60,
                                    MusicalEventSource::PatternPlayer));
    assert(usb.count(0x80, 60) == 0);
    assert(output.servicePendingReleases(1000u));
    assert(usb.count(0x80, 60) == 1);
}

void testDrumLaneRetriesWithoutNewEvent() {
    ScriptedTransport usb;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::Drums));
    assert(usb.log.size() == 1);
    const uint8_t wireNote = usb.log[0].note;
    const uint8_t channel = usb.log[0].channel;
    usb.rejectNoteOffs = 1;
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 36,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::Drums));
    assert(usb.count(0x80, wireNote) == 0);
    assert(output.servicePendingReleases(1000u));
    assert(usb.count(0x80, wireNote) == 1);
    assert(usb.log.back().channel == channel);
}

void testChordNoteRetriesWithoutNewEvent() {
    ScriptedTransport usb;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA,
                                    kMusicalEventChord));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 64,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA,
                                    kMusicalEventChord));
    assert(usb.count(0x90, 60) == 1 && usb.count(0x90, 64) == 1);
    usb.rejectNoteOffs = 1;
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 60,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA,
                                    kMusicalEventChord));
    assert(usb.count(0x80, 60) == 0);
    assert(output.servicePendingReleases(1000u));
    assert(usb.count(0x80, 60) == 1);
    assert(usb.count(0x80, 64) == 0);  // the other chord note keeps sounding
}

void testGeneratedPerformRetriesWithoutNewEvent() {
    ScriptedTransport usb;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 67,
                                    MusicalEventSource::PerformanceKeyboard));
    usb.rejectNoteOffs = 1;
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 67,
                                    MusicalEventSource::PerformanceKeyboard));
    assert(usb.count(0x80, 67) == 0);
    assert(output.servicePendingReleases(1000u));
    assert(usb.count(0x80, 67) == 1);
}

// A refused write stops the pass: a full FIFO would refuse the rest too, and
// each refused write costs a paced wait on the wire.
void testPassStopsAtFirstRefusal() {
    ScriptedTransport usb;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 62,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    usb.rejectNoteOffs = 2;
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 60,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 62,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    usb.rejectNoteOffs = 5;
    assert(!output.servicePendingReleases(1000u));
    assert(usb.rejectNoteOffs == 4);  // one attempt only, the second lane waited
}

// micros() wraps every 2^32 us. The cadence must keep firing across the wrap.
// The cadence is anchored near the wrap through an unmounted call, which is the
// path the dispatcher takes while the output is down.
void testCadenceAcrossMicrosWrap() {
    ScriptedTransport usb;
    usb.mountedFlag = false;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();
    const uint32_t t0 = 0xFFFFF000u;
    assert(output.servicePendingReleases(t0 - 10000u));  // anchors the cadence
    usb.mountedFlag = true;
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60,
                                    MusicalEventSource::PatternPlayer));
    usb.rejectNoteOffs = 1;
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 60,
                                    MusicalEventSource::PatternPlayer));
    assert(usb.count(0x80, 60) == 0);
    assert(output.servicePendingReleases(t0 - 5000u));   // due, before the wrap
    assert(usb.count(0x80, 60) == 1);
    // Crossing the wrap keeps the cadence running.
    usb.rejectNoteOffs = 1;
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 62,
                                    MusicalEventSource::PatternPlayer));
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 62,
                                    MusicalEventSource::PatternPlayer));
    assert(output.servicePendingReleases(t0 + 1000u));   // now past the wrap
    assert(usb.count(0x80, 62) == 1);
}

// A long unmounted spell must not hold retries back after the remount. The
// unmounted calls keep the cadence anchored to the present.
void testRemountAfterLongUnmountedSpellRetriesPromptly() {
    ScriptedTransport usb;
    usb.mountedFlag = false;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();
    assert(output.servicePendingReleases(0x80000000u));  // unmounted, 2^31 us later
    usb.mountedFlag = true;
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60,
                                    MusicalEventSource::PatternPlayer));
    usb.rejectNoteOffs = 1;
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 60,
                                    MusicalEventSource::PatternPlayer));
    assert(output.servicePendingReleases(0x80000000u + 1000u));
    assert(usb.count(0x80, 60) == 1);
}

// An idle output sends nothing, however long the cadence runs.
void testIdleCadenceSendsNothing() {
    ScriptedTransport usb;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();
    for (uint32_t t = 0; t < 100000u; t += 5000u) {
        assert(output.servicePendingReleases(t));
    }
    assert(usb.log.empty());
}

// A profile publication after startup must not change the active route. If
// USB drops while DIN remains available, the last owner's NoteOff reaches DIN;
// USB must clear its stale note before it receives any post-reconnect notes.
// The trace also crosses a section-like NoteOff/NoteOn boundary and closes all
// note ownership before teardown.
void testRouterTeeDisconnectAndFrozenRouteIntegration() {
    GroovePuterMidi::MidiOutputRouteProjection patternRoutes{};
    patternRoutes.patternSynthA = {true, 7};
    patternRoutes.patternSynthB = {true, 8};
    GroovePuterMidi::MidiPerformanceRouteProjection performanceRoutes{};
    performanceRoutes.complete = true;
    performanceRoutes.enabled = true;
    performanceRoutes.synthA = {true, 7};
    performanceRoutes.synthB = {true, 8};
    performanceRoutes.dx = {true, 9};
    performanceRoutes.receiverModeControl =
        GroovePuterMidi::MidiReceiverModeControl::None;
    GroovePuterMidi::midiPatternStartupRouteRuntime().publish(
        patternRoutes, performanceRoutes);

    ScriptedTransport usb;
    ScriptedTransport din;
    GroovePuterMidi::TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);
    UsbMidiOutput output(tee);
    MusicalEventRouter router;
    assert(router.addSink(output));
    assert(output.begin());
    output.pollConnection();

    router.route(event(MusicalEventType::NoteOn, 60,
                       MusicalEventSource::PatternPlayer));
    router.route(event(MusicalEventType::NoteOn, 60,
                       MusicalEventSource::PerformanceKeyboard));
    assert(output.wireOwnerCount(7, 60) == 2);
    assert(usb.log.size() == 1 && usb.log[0].status == 0x90 &&
           usb.log[0].channel == 7 && usb.log[0].note == 60);
    assert(din.log.size() == 1 && din.log[0].status == 0x90 &&
           din.log[0].channel == 7 && din.log[0].note == 60);

    // Simulate a newly selected profile being published for the next output
    // session. The running output retains the channel it captured at begin().
    patternRoutes.patternSynthA.channel = 0;
    performanceRoutes.synthA.channel = 0;
    GroovePuterMidi::midiPatternStartupRouteRuntime().publish(
        patternRoutes, performanceRoutes);
    assert(output.channelFor(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 7);

    usb.mountedFlag = false;
    output.pollConnection();
    assert(output.wireOwnerCount(7, 60) == 2);

    router.route(event(MusicalEventType::NoteOff, 60,
                       MusicalEventSource::PatternPlayer));
    assert(output.wireOwnerCount(7, 60) == 1);
    assert(usb.count(0x80, 60) == 0 && din.count(0x80, 60) == 0);

    router.route(event(MusicalEventType::NoteOff, 60,
                       MusicalEventSource::PerformanceKeyboard));
    assert(output.wireOwnerCount(7, 60) == 0);
    assert(usb.count(0x80, 60) == 0);
    assert(din.count(0x80, 60) == 1);

    const std::size_t usbBeforeReconnect = usb.log.size();
    const std::size_t dinBeforeReconnect = din.log.size();
    usb.mountedFlag = true;
    output.pollConnection();
    assert(usb.log.size() == usbBeforeReconnect);
    assert(din.log.size() == dinBeforeReconnect);

    router.route(event(MusicalEventType::NoteOn, 62,
                       MusicalEventSource::PatternPlayer));
    assert(usb.log.size() == usbBeforeReconnect + 2);
    assert(usb.log[usbBeforeReconnect].status == 0xB0 &&
           usb.log[usbBeforeReconnect].channel == 7 &&
           usb.log[usbBeforeReconnect].note == 123);
    assert(usb.log[usbBeforeReconnect + 1].status == 0x90 &&
           usb.log[usbBeforeReconnect + 1].channel == 7 &&
           usb.log[usbBeforeReconnect + 1].note == 62);
    assert(din.log.size() == dinBeforeReconnect + 1);
    assert(din.log.back().status == 0x90 && din.log.back().channel == 7 &&
           din.log.back().note == 62);
    assert(output.wireOwnerCount(7, 62) == 1);

    // Song section boundary: release the old section note before the next one.
    router.route(event(MusicalEventType::NoteOff, 62,
                       MusicalEventSource::PatternPlayer));
    router.route(event(MusicalEventType::NoteOn, 64,
                       MusicalEventSource::PatternPlayer));
    assert(output.wireOwnerCount(7, 62) == 0);
    assert(output.wireOwnerCount(7, 64) == 1);
    router.route(event(MusicalEventType::NoteOff, 64,
                       MusicalEventSource::PatternPlayer));
    assert(output.wireOwnerCount(7, 64) == 0);
}

}  // namespace

int main() {
    testUsbStalledNoteOffReachesDinThroughCadence();
    testSharedNoteRetryDoesNotCutSecondOwner();
    testMelodicLaneRetriesWithoutNewEvent();
    testDrumLaneRetriesWithoutNewEvent();
    testChordNoteRetriesWithoutNewEvent();
    testGeneratedPerformRetriesWithoutNewEvent();
    testPassStopsAtFirstRefusal();
    testCadenceAcrossMicrosWrap();
    testRemountAfterLongUnmountedSpellRetriesPromptly();
    testIdleCadenceSendsNothing();
    testRouterTeeDisconnectAndFrozenRouteIntegration();
    return 0;
}
