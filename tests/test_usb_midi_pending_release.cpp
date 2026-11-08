// D5 integration: UsbMidiOutput over the USB+DIN tee. A refused NoteOff must
// reach the DIN wire through the bounded retry cadence alone, and a note shared
// by two owners must not be cut by the first owner's release.
#include <cassert>
#include <cstdint>
#include <vector>

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
                   MusicalEventTarget target = MusicalEventTarget::SynthA) {
    const uint8_t velocity = type == MusicalEventType::NoteOn ? 100u : 0u;
    return MusicalEvent{type, source, target, 0, note, velocity};
}

// Primary refuses every NoteOff. DIN accepts. The note must still reach DIN:
// once the primary is stalled the tee demotes it, and the cadence keeps retrying
// without any further musical event.
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

    usb.rejectNoteOffs = 1000;  // USB refuses every NoteOff from now on
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 60,
                                    MusicalEventSource::PatternPlayer));
    assert(din.count(0x80, 60) == 0);  // authoritative USB held DIN back

    uint32_t now = 10000u;
    int attempts = 0;
    while (din.count(0x80, 60) == 0 && attempts < 200) {
        output.servicePendingReleases(now);
        now += UsbMidiOutput::kPendingReleaseRetryIntervalUs;
        ++attempts;
    }
    // The tee demotes a stalled primary after kPrimaryStallRejects refusals.
    // That is bounded, and it happens without a new musical event.
    assert(din.count(0x80, 60) == 1);
    assert(attempts <= static_cast<int>(
        GroovePuterMidi::TeeMidiTransport::kPrimaryStallRejects) + 8);
    assert(usb.count(0x80, 60) == 0);
}

// Pattern and PERFORM hold the same physical note on the same channel. The
// first owner's NoteOff must not end the note while the other still holds it.
void testSharedNoteSurvivesFirstOwnerRelease() {
    ScriptedTransport usb;
    UsbMidiOutput output(usb, UsbMidiRouteConfig{7, 7, 8, true, true});
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 64,
                                    MusicalEventSource::PatternPlayer));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 64,
                                    MusicalEventSource::PerformanceKeyboard));
    assert(usb.count(0x90, 64) == 1);  // one wire NoteOn for two owners

    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 64,
                                    MusicalEventSource::PatternPlayer));
    assert(usb.count(0x80, 64) == 0);  // PERFORM still holds the note

    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 64,
                                    MusicalEventSource::PerformanceKeyboard));
    assert(usb.count(0x80, 64) == 1);  // last owner releases it once
}

// An idle output with nothing pending sends nothing, however long it is called.
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

}  // namespace

int main() {
    testUsbStalledNoteOffReachesDinThroughCadence();
    testSharedNoteSurvivesFirstOwnerRelease();
    testIdleCadenceSendsNothing();
    return 0;
}
