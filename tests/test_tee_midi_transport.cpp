#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/tee_midi_transport.h"

using GroovePuterMidi::TeeMidiTransport;

namespace {

struct Message {
    uint8_t status;
    uint8_t d1;
    uint8_t d2;
};

class FakeTransport final : public IMidiTransport {
public:
    explicit FakeTransport(MidiTransportLink link = MidiTransportLink::Enumerated)
        : link_(link) {}

    bool begin() override {
        begun = true;
        return beginResult;
    }
    bool mounted() const override { return mountedFlag; }
    MidiTransportLink linkKind() const override { return link_; }

    bool sendNoteOn(uint8_t c, uint8_t n, uint8_t v) override {
        return record(0x90, c, n, v);
    }
    bool sendNoteOff(uint8_t c, uint8_t n, uint8_t v) override {
        return record(0x80, c, n, v);
    }
    bool sendControlChange(uint8_t c, uint8_t cc, uint8_t v) override {
        return record(0xB0, c, cc, v);
    }
    bool sendTimingClock() override { return record(0xF8, 0, 0, 0); }
    bool sendStart() override { return record(0xFA, 0, 0, 0); }
    bool sendStop() override { return record(0xFC, 0, 0, 0); }
    void flush() override { ++flushes; }

    bool mountedFlag{true};
    bool beginResult{true};
    bool accept{true};
    bool begun{false};
    unsigned flushes{0};
    std::vector<Message> sent;

private:
    bool record(uint8_t status, uint8_t c, uint8_t d1, uint8_t d2) {
        if (!accept) return false;
        sent.push_back(Message{static_cast<uint8_t>(status | c), d1, d2});
        return true;
    }
    MidiTransportLink link_;
};

void secondaryIsSilentUntilEnabled() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);

    assert(!tee.secondaryEnabled());
    assert(tee.sendNoteOn(0, 60, 100));
    assert(usb.sent.size() == 1);
    assert(din.sent.empty());
}

void bothWiresReceiveIdenticalTraffic() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    assert(tee.sendNoteOn(3, 64, 100));
    assert(tee.sendNoteOff(3, 64, 0));
    assert(usb.sent.size() == 2 && din.sent.size() == 2);
    for (std::size_t i = 0; i < usb.sent.size(); ++i) {
        assert(usb.sent[i].status == din.sent[i].status);
        assert(usb.sent[i].d1 == din.sent[i].d1);
        assert(usb.sent[i].d2 == din.sent[i].d2);
    }
}

void secondaryRejectionIsCountedNotEscalated() {
    // Escalating here would make the SMF path retry a note USB already
    // delivered, putting a duplicate on the wire instead of recovering.
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);
    din.accept = false;

    assert(tee.sendNoteOn(0, 60, 100));
    assert(tee.diagnostics().secondaryRejected == 1);
    assert(usb.sent.size() == 1);
}

void usbBackpressureStillReportsFailure() {
    // The authoritative answer stays USB while a host is attached, so existing
    // retry and cleanup policy is unchanged.
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);
    usb.accept = false;

    assert(!tee.sendNoteOn(0, 60, 100));

    // And the secondary must NOT have sent it. The owner records this call as
    // "never sent", so a note on the DIN wire here would have no owner and
    // therefore never receive its NoteOff. An earlier version did send it, and
    // this assertion pinned that bug as contract.
    assert(din.sent.empty());
    assert(tee.diagnostics().secondarySkipped == 1);
}

void aSingleUsbRejectLeavesNothingSoundingOnTheSecondary() {
    // Transient backpressure, far below the stall threshold, followed by a
    // successful retry: exactly one NoteOn must exist on each wire.
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    usb.accept = false;
    assert(!tee.sendNoteOn(0, 60, 100));
    assert(din.sent.empty());

    usb.accept = true;
    assert(tee.sendNoteOn(0, 60, 100));
    assert(usb.sent.size() == 1);
    assert(din.sent.size() == 1);

    assert(tee.sendNoteOff(0, 60, 0));
    assert(usb.sent.size() == 2);
    assert(din.sent.size() == 2);
}

void smfStyleRetriesDoNotStackDuplicatesOnTheSecondary() {
    // The SMF path retries a refused send up to 24 times. Every refusal must
    // leave the secondary untouched, or those retries become duplicate NoteOn
    // messages on the DIN wire.
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    usb.accept = false;
    for (int attempt = 0; attempt < 24; ++attempt) {
        assert(!tee.sendNoteOn(0, 60, 100));
    }
    assert(din.sent.empty());

    usb.accept = true;
    assert(tee.sendNoteOn(0, 60, 100));
    assert(din.sent.size() == 1);
}

void dinRunsStandaloneWithNoUsbHost() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);
    usb.mountedFlag = false;

    assert(tee.mounted());
    assert(tee.sendNoteOn(0, 60, 100));
    assert(usb.sent.empty());
    assert(din.sent.size() == 1);
    assert(tee.diagnostics().secondaryOnlyDelivered == 1);
}

void unmountedEverywhereReportsUnmounted() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    usb.mountedFlag = false;
    din.mountedFlag = false;
    tee.setSecondaryEnabled(true);
    assert(!tee.mounted());
    assert(!tee.sendNoteOn(0, 60, 100));
}

void linkKindDegradesToTheLeastVerifiableActiveWire() {
    // A UI must not claim a verified connection when part of the stream is
    // going out on a wire that cannot report anything.
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);

    assert(tee.linkKind() == MidiTransportLink::Enumerated);
    tee.setSecondaryEnabled(true);
    assert(tee.linkKind() == MidiTransportLink::Unverifiable);
    din.mountedFlag = false;
    assert(tee.linkKind() == MidiTransportLink::Enumerated);
}

void realtimeAndRecoveryAlsoTee() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    assert(tee.sendTimingClock());
    assert(tee.sendStart());
    assert(tee.sendStop());
    assert(tee.sendControlChange(0, 123, 0));
    assert(usb.sent.size() == 4 && din.sent.size() == 4);
    assert(din.sent[3].status == 0xB0 && din.sent[3].d1 == 123);
}

void beginSucceedsIfEitherWireStarts() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    usb.beginResult = false;
    assert(tee.begin());
    // Both must be offered begin() regardless, or a later enable would find an
    // unopened port.
    assert(usb.begun && din.begun);
}

void flushFollowsTheEnableFlag() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.flush();
    assert(usb.flushes == 1 && din.flushes == 0);
    tee.setSecondaryEnabled(true);
    tee.flush();
    assert(usb.flushes == 2 && din.flushes == 1);
}


// --- stalled primary must not silence the secondary ----------------------
//
// Reproduces the hardware failure: USB plugged into a computer with no open
// MIDI port. The device stays mounted, the TX FIFO fills after about sixteen
// packets, and every later write is refused. With "mounted" treated as
// "authoritative" that dead wire vetoed the live one and DIN went silent.

void aStalledPrimaryLosesAuthorityAndTheSecondaryKeepsPlaying() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    // The FIFO takes a few packets, then refuses everything.
    for (int i = 0; i < 16; ++i) assert(tee.sendNoteOn(0, 60, 100));
    usb.accept = false;

    bool sawFailure = false;
    for (unsigned i = 0; i < TeeMidiTransport::kPrimaryStallRejects; ++i) {
        if (!tee.sendNoteOn(0, 61, 100)) sawFailure = true;
    }
    // Normal backpressure must still be reported while below the threshold,
    // otherwise the caller's retry policy silently changes meaning.
    assert(sawFailure);
    assert(tee.primaryStalled());

    // Past the threshold the live wire wins.
    assert(tee.sendNoteOn(0, 62, 100));
    assert(tee.diagnostics().primaryStallDemotions == 1);
    assert(!din.sent.empty());
}

void aRecoveredPrimaryRegainsAuthority() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);
    usb.accept = false;

    for (unsigned i = 0; i <= TeeMidiTransport::kPrimaryStallRejects; ++i) {
        tee.sendNoteOn(0, 60, 100);
    }
    assert(tee.primaryStalled());

    // Traffic must keep being offered while stalled, which is how the host
    // opening its port is noticed at all.
    const std::size_t before = usb.sent.size();
    usb.accept = true;
    assert(tee.sendNoteOn(0, 60, 100));
    assert(usb.sent.size() == before + 1);
    assert(!tee.primaryStalled());

    // Authoritative again: a fresh USB rejection is reported as failure.
    usb.accept = false;
    assert(!tee.sendNoteOn(0, 60, 100));
}

void transientBackpressureDoesNotDemoteThePrimary() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    for (unsigned i = 0; i < TeeMidiTransport::kPrimaryStallRejects * 4; ++i) {
        usb.accept = (i % 8) != 0;   // occasional refusal, always recovering
        tee.sendNoteOn(0, 60, 100);
    }
    assert(!tee.primaryStalled());
    assert(tee.diagnostics().primaryStallDemotions == 0);
}

void unmountingThePrimaryClearsTheStallCounter() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);
    usb.accept = false;
    for (unsigned i = 0; i <= TeeMidiTransport::kPrimaryStallRejects; ++i) {
        tee.sendNoteOn(0, 60, 100);
    }
    assert(tee.primaryStalled());

    usb.mountedFlag = false;
    tee.sendNoteOn(0, 60, 100);
    assert(!tee.primaryStalled());
}

}  // namespace

int main() {
    secondaryIsSilentUntilEnabled();
    bothWiresReceiveIdenticalTraffic();
    secondaryRejectionIsCountedNotEscalated();
    usbBackpressureStillReportsFailure();
    aSingleUsbRejectLeavesNothingSoundingOnTheSecondary();
    smfStyleRetriesDoNotStackDuplicatesOnTheSecondary();
    dinRunsStandaloneWithNoUsbHost();
    unmountedEverywhereReportsUnmounted();
    linkKindDegradesToTheLeastVerifiableActiveWire();
    realtimeAndRecoveryAlsoTee();
    beginSucceedsIfEitherWireStarts();
    flushFollowsTheEnableFlag();
    aStalledPrimaryLosesAuthorityAndTheSecondaryKeepsPlaying();
    aRecoveredPrimaryRegainsAuthority();
    transientBackpressureDoesNotDemoteThePrimary();
    unmountingThePrimaryClearsTheStallCounter();
    return 0;
}
