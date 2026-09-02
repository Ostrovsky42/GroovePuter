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

}  // namespace

int main() {
    secondaryIsSilentUntilEnabled();
    bothWiresReceiveIdenticalTraffic();
    secondaryRejectionIsCountedNotEscalated();
    usbBackpressureStillReportsFailure();
    dinRunsStandaloneWithNoUsbHost();
    unmountedEverywhereReportsUnmounted();
    linkKindDegradesToTheLeastVerifiableActiveWire();
    realtimeAndRecoveryAlsoTee();
    beginSucceedsIfEitherWireStarts();
    flushFollowsTheEnableFlag();
    return 0;
}
