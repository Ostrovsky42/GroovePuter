// CC74 cutoff on Synth A through UsbMidiOutput. Host-only: a recording transport
// stands in for USB and captures the controller and value of every CC.
#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/cutoff_sweep.h"
#include "src/midi/usb_midi_output.h"
#include "src/output/output_ownership.h"

namespace {

struct Cc {
    uint8_t channel;
    uint8_t controller;
    uint8_t value;
};

class RecordingTransport final : public IMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return mountedFlag; }
    MidiTransportLink linkKind() const override { return MidiTransportLink::Enumerated; }
    bool sendNoteOn(uint8_t c, uint8_t n, uint8_t) override {
        if (!mountedFlag || refuseAll) return false;
        notes.push_back({c, n, 0});
        return true;
    }
    bool sendNoteOff(uint8_t c, uint8_t n, uint8_t) override {
        if (!mountedFlag || refuseAll) return false;
        notesOff.push_back({c, n, 0});
        return true;
    }
    bool sendControlChange(uint8_t c, uint8_t cc, uint8_t v) override {
        if (!mountedFlag || refuseAll || refuseCc > 0) {
            if (refuseCc > 0) --refuseCc;
            return false;
        }
        ccs.push_back({c, cc, v});
        return true;
    }
    bool sendTimingClock() override { return true; }
    bool sendStart() override { return true; }
    bool sendContinue() override { return true; }
    bool sendStop() override { return true; }
    bool sendSongPositionPointer(uint16_t) override { return true; }
    void flush() override {}

    bool mountedFlag{true};
    bool refuseAll{false};
    int refuseCc{0};
    std::vector<Cc> ccs;
    std::vector<Cc> notes;
    std::vector<Cc> notesOff;
};

MusicalEvent cc(uint8_t controller, uint8_t value,
                MusicalEventTarget target = MusicalEventTarget::SynthA) {
    return MusicalEvent{MusicalEventType::ControlChange,
                        MusicalEventSource::PerformanceKeyboard,
                        target, 0, controller, value, 0};
}

// Route: live Synth A on zero-based channel 7 (MIDI channel 8).
UsbMidiRouteConfig route() { return UsbMidiRouteConfig{7, 7, 8, true, true}; }

void testEncodesCutoffOnSynthAChannel8() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    out.handleMusicalEvent(cc(74, 64));
    out.handleMusicalEvent(cc(74, 110));
    out.handleMusicalEvent(cc(74, 0));
    out.handleMusicalEvent(cc(74, 127));
    assert(usb.ccs.size() == 4);
    assert(usb.ccs[0].channel == 7 && usb.ccs[0].controller == 74 && usb.ccs[0].value == 64);
    assert(usb.ccs[1].value == 110);   // B7 4A 6E
    assert(usb.ccs[2].value == 0);     // B7 4A 00
    assert(usb.ccs[3].value == 127);   // B7 4A 7F
}

void testClampsOutOfRangeValues() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    out.handleMusicalEvent(cc(74, 200));
    assert(usb.ccs.size() == 1 && usb.ccs[0].value == 127);
}

void testDeduplicatesUnchangedValue() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    out.handleMusicalEvent(cc(74, 90));
    out.handleMusicalEvent(cc(74, 90));
    out.handleMusicalEvent(cc(74, 90));
    assert(usb.ccs.size() == 1);
    out.handleMusicalEvent(cc(74, 91));
    assert(usb.ccs.size() == 2 && usb.ccs[1].value == 91);
}

void testOnlyCutoffOnSynthsReachesTheWire() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    out.handleMusicalEvent(cc(5, 60));                         // portamento: not in scope
    out.handleMusicalEvent(cc(74, 60, MusicalEventTarget::Drums));   // drums: no CC74
    assert(usb.ccs.empty());
}

void testDoesNotTouchNoteOwnership() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    out.handleMusicalEvent(MusicalEvent{MusicalEventType::NoteOn,
                                        MusicalEventSource::PerformanceKeyboard,
                                        MusicalEventTarget::SynthA, 0, 60, 100, 0});
    out.handleMusicalEvent(cc(74, 80));
    out.handleMusicalEvent(cc(74, 81));
    assert(usb.notes.size() == 1);
    assert(usb.notesOff.empty());   // a CC never releases or retriggers a held note
}

void testRefusedCcIsRetriedByTheNextEvent() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    usb.refuseCc = 1;
    out.handleMusicalEvent(cc(74, 70));
    assert(usb.ccs.empty());        // refused: not recorded as sent
    out.handleMusicalEvent(cc(74, 70));
    assert(usb.ccs.size() == 1 && usb.ccs[0].value == 70);
}

void testNothingWhileUnmountedAndResendAfterReconnect() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    out.handleMusicalEvent(cc(74, 100));
    assert(usb.ccs.size() == 1);
    usb.mountedFlag = false;
    out.pollConnection();
    out.handleMusicalEvent(cc(74, 101));
    assert(usb.ccs.size() == 1);    // no CC while the output is down
    usb.mountedFlag = true;
    out.pollConnection();
    out.handleMusicalEvent(cc(74, 100));
    assert(usb.ccs.size() == 2 && usb.ccs[1].value == 100);  // receiver state unknown: resend
}

void testSynthBUsesItsOwnChannelAndDedupe() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    out.handleMusicalEvent(cc(74, 64));
    out.handleMusicalEvent(cc(74, 90, MusicalEventTarget::SynthB));
    out.handleMusicalEvent(cc(74, 90, MusicalEventTarget::SynthB));   // same value: no resend
    assert(usb.ccs.size() == 2);
    assert(usb.ccs[0].channel == 7 && usb.ccs[0].value == 64);        // SYNTH A: CH8
    assert(usb.ccs[1].channel == 8 && usb.ccs[1].value == 90);        // SYNTH B: CH9
    out.handleMusicalEvent(cc(74, 64));                              // A unchanged since its last send
    assert(usb.ccs.size() == 2);
}

}  // namespace

void testCutoffFollowsTrackOutputMode() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    GroovePuterOutput::setMode(GroovePuterOutput::Track::SynthA, GroovePuterOutput::Mode::Internal);
    out.handleMusicalEvent(cc(74, 40));
    assert(usb.ccs.empty());        // INTERNAL: the external synth must not hear CC74
    GroovePuterOutput::setMode(GroovePuterOutput::Track::SynthA, GroovePuterOutput::Mode::Midi);
    out.handleMusicalEvent(cc(74, 40));
    assert(usb.ccs.size() == 1 && usb.ccs[0].value == 40);  // back to MIDI: sent
}

namespace {
void testDeliveredMeansOnTheWire() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    usb.refuseCc = 1;
    out.handleMusicalEvent(cc(74, 50));
    assert(!out.cutoffDelivered(0, 74, 50));    // refused: not on the wire
    out.handleMusicalEvent(cc(74, 50));
    assert(out.cutoffDelivered(0, 74, 50));     // the level check's retry lands it
    assert(!out.cutoffDelivered(0, 74, 51));    // another value is not delivered
    usb.mountedFlag = false;
    out.pollConnection();
    usb.mountedFlag = true;
    out.pollConnection();
    assert(!out.cutoffDelivered(0, 74, 50));    // reconnect: the held value must be resent
}

}  // namespace

namespace {
void testChannelConstantsMatchTheRoute() {
    // The dispatcher's route is UsbMidiRouteConfig{7, 7, 8, true, true}; the UI
    // label and the CC go to the same channels only while these agree.
    UsbMidiRouteConfig r = route();
    assert(r.performanceSynthAChannel == GroovePuterMidi::kSynthAChannel);
    assert(r.performanceSynthBChannel == GroovePuterMidi::kSynthBChannel);
}

}  // namespace

namespace {
void testAllowedControllersOnlyAndPanRange() {
    RecordingTransport usb;
    UsbMidiOutput out(usb, route());
    assert(out.begin());
    out.pollConnection();
    const uint8_t allowed[] = {74, 73, 75, 91, 94, 7, 10};
    for (uint8_t number : allowed) out.handleMusicalEvent(cc(number, 64));
    assert(usb.ccs.size() == 7);            // every documented synth control reaches the wire
    // Not allowed: portamento, arpeggiator, DX-only, master effect parameters.
    const uint8_t refused[] = {71, 5, 65, 27, 116, 102};
    for (uint8_t number : refused) out.handleMusicalEvent(cc(number, 64));
    assert(usb.ccs.size() == 7);
    // PAN has a usable range of 1..127: zero is sent as 1, and delivery agrees.
    out.handleMusicalEvent(cc(10, 0));
    assert(usb.ccs.size() == 8 && usb.ccs[7].controller == 10 && usb.ccs[7].value == 1);
    assert(out.cutoffDelivered(0, 10, 0));
}

}  // namespace

int main() {
    testEncodesCutoffOnSynthAChannel8();
    testClampsOutOfRangeValues();
    testDeduplicatesUnchangedValue();
    testOnlyCutoffOnSynthsReachesTheWire();
    testSynthBUsesItsOwnChannelAndDedupe();
    testCutoffFollowsTrackOutputMode();
    testDeliveredMeansOnTheWire();
    testAllowedControllersOnlyAndPanRange();
    testChannelConstantsMatchTheRoute();
    testDoesNotTouchNoteOwnership();
    testRefusedCcIsRetriedByTheNextEvent();
    testNothingWhileUnmountedAndResendAfterReconnect();
    return 0;
}
