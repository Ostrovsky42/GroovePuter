#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/tee_midi_transport.h"

using GroovePuterMidi::TeeMidiTransport;

namespace {
struct Message { uint8_t status; uint8_t d1; uint8_t d2; };

class FakeTransport final : public IMidiTransport {
public:
    explicit FakeTransport(MidiTransportLink link = MidiTransportLink::Enumerated)
        : link_(link) {}
    bool begin() override { begun = true; return beginResult; }
    bool mounted() const override { return mountedFlag; }
    MidiTransportLink linkKind() const override { return link_; }
    bool sendNoteOn(uint8_t c, uint8_t n, uint8_t v) override { return record(0x90, c, n, v); }
    bool sendNoteOff(uint8_t c, uint8_t n, uint8_t v) override { return record(0x80, c, n, v); }
    bool sendControlChange(uint8_t c, uint8_t cc, uint8_t v) override { return record(0xB0, c, cc, v); }
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

void authoritativeRejectNeverLeaksToDin() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    usb.accept = false;
    assert(!tee.sendNoteOn(0, 60, 100));
    assert(din.sent.empty());
    assert(tee.diagnostics().secondarySkipped == 1);

    usb.accept = true;
    assert(tee.sendNoteOn(0, 60, 100));
    assert(usb.sent.size() == 1);
    assert(din.sent.size() == 1);
    assert(tee.sendNoteOff(0, 60, 0));
    assert(usb.sent.size() == 2);
    assert(din.sent.size() == 2);
}

void smfStyleRetriesDoNotDuplicateDinNoteOns() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    usb.accept = false;
    for (int i = 0; i < 24; ++i) assert(!tee.sendNoteOn(0, 60, 100));
    assert(din.sent.empty());
    usb.accept = true;
    assert(tee.sendNoteOn(0, 60, 100));
    assert(din.sent.size() == 1);
}

void stalledPrimaryYieldsAndRecovers() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    usb.accept = false;
    for (unsigned i = 0; i < TeeMidiTransport::kPrimaryStallRejects; ++i) {
        tee.sendNoteOn(0, 61, 100);
    }
    assert(tee.primaryStalled());
    assert(tee.sendNoteOn(0, 62, 100));
    assert(!din.sent.empty());

    usb.accept = true;
    assert(tee.sendNoteOn(0, 63, 100));
    assert(!tee.primaryStalled());
}

void standaloneDinIsUnverifiableButUsable() {
    FakeTransport usb;
    FakeTransport din(MidiTransportLink::Unverifiable);
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);
    usb.mountedFlag = false;
    assert(tee.mounted());
    assert(tee.linkKind() == MidiTransportLink::Unverifiable);
    assert(tee.sendNoteOn(0, 60, 100));
    assert(din.sent.size() == 1);
}
}  // namespace

int main() {
    authoritativeRejectNeverLeaksToDin();
    smfStyleRetriesDoNotDuplicateDinNoteOns();
    stalledPrimaryYieldsAndRecovers();
    standaloneDinIsUnverifiableButUsable();
    return 0;
}
