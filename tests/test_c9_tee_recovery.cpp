#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/tee_midi_transport.h"

using GroovePuterMidi::TeeMidiTransport;

namespace {
struct Message {
    uint8_t status{0};
    uint8_t d1{0};
    uint8_t d2{0};
};

class FakeTransport final : public IMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return mountedFlag; }
    MidiTransportLink linkKind() const override { return MidiTransportLink::Enumerated; }
    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        return record(static_cast<uint8_t>(0x90u | channel), note, velocity);
    }
    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        return record(static_cast<uint8_t>(0x80u | channel), note, velocity);
    }
    bool sendControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
        return record(static_cast<uint8_t>(0xB0u | channel), controller, value);
    }
    void flush() override {}

    bool record(uint8_t status, uint8_t d1, uint8_t d2) {
        if (!accept) return false;
        sent.push_back(Message{status, d1, d2});
        return true;
    }

    bool mountedFlag{true};
    bool accept{true};
    std::vector<Message> sent;
};

void recoveredPrimaryIsCleanedBeforeItRegainsAuthority() {
    FakeTransport usb;
    FakeTransport din;
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    assert(tee.sendNoteOn(0, 60, 100));
    assert(usb.sent.size() == 1u && din.sent.size() == 1u);

    usb.accept = false;
    bool releaseAcknowledged = false;
    for (unsigned attempt = 0; attempt < TeeMidiTransport::kPrimaryStallRejects; ++attempt) {
        releaseAcknowledged = tee.sendNoteOff(0, 60, 0);
    }
    assert(tee.primaryStalled());
    assert(releaseAcknowledged);
    assert(din.sent.size() == 2u);
    assert((din.sent.back().status & 0xF0u) == 0x80u);

    // USB may now accept writes, but it still has note 60 sounding because its
    // NoteOff was accepted only by DIN. The first recovered application write
    // must therefore be preceded by primary-only CC123 cleanup for CH1.
    usb.accept = true;
    assert(tee.sendNoteOn(0, 62, 100));
    assert(usb.sent.size() >= 3u);
    assert(usb.sent[1].status == 0xB0u);
    assert(usb.sent[1].d1 == 123u && usb.sent[1].d2 == 0u);
    assert(usb.sent[2].status == 0x90u && usb.sent[2].d1 == 62u);
    assert(!tee.primaryStalled());
}

void failedRecoveryCleanupKeepsPrimaryDemoted() {
    FakeTransport usb;
    FakeTransport din;
    TeeMidiTransport tee(usb, din);
    tee.setSecondaryEnabled(true);

    assert(tee.sendNoteOn(0, 60, 100));
    usb.accept = false;
    for (unsigned attempt = 0; attempt < TeeMidiTransport::kPrimaryStallRejects; ++attempt) {
        (void)tee.sendNoteOff(0, 60, 0);
    }
    assert(tee.primaryStalled());

    // Still blocked: recovery cleanup cannot complete, so DIN remains the only
    // successful endpoint and USB must not regain authority.
    const std::size_t dinBefore = din.sent.size();
    assert(tee.sendNoteOn(0, 64, 100));
    assert(tee.primaryStalled());
    assert(din.sent.size() == dinBefore + 1u);
}
}  // namespace

int main() {
    recoveredPrimaryIsCleanedBeforeItRegainsAuthority();
    failedRecoveryCleanupKeepsPrimaryDemoted();
    return 0;
}
