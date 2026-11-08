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
        if ((status & 0xF0u) == 0xB0u && d1 == 123u && rejectRecoveryCc123 > 0u) {
            --rejectRecoveryCc123;
            return false;
        }
        if (!accept) return false;
        sent.push_back(Message{status, d1, d2});
        return true;
    }

    bool mountedFlag{true};
    bool accept{true};
    unsigned rejectRecoveryCc123{0};
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

    const std::size_t dinBefore = din.sent.size();
    assert(tee.sendNoteOn(0, 64, 100));
    assert(tee.primaryStalled());
    assert(din.sent.size() == dinBefore + 1u);
}

void cleanupDebtSurvivesDisconnectReconnect() {
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

    usb.mountedFlag = false;
    usb.accept = true;
    const std::size_t dinBeforeDisconnectTraffic = din.sent.size();
    assert(tee.sendNoteOn(1, 62, 100));
    assert(din.sent.size() == dinBeforeDisconnectTraffic + 1u);

    usb.mountedFlag = true;
    const std::size_t usbBeforeReconnect = usb.sent.size();
    assert(tee.sendNoteOn(2, 64, 100));
    assert(usb.sent.size() == usbBeforeReconnect + 2u);
    assert(usb.sent[usbBeforeReconnect].status == 0xB0u);
    assert(usb.sent[usbBeforeReconnect].d1 == 123u);
    assert((usb.sent[usbBeforeReconnect].status & 0x0Fu) == 0u);
    assert(usb.sent[usbBeforeReconnect + 1u].status == 0x92u);
    assert(usb.sent[usbBeforeReconnect + 1u].d1 == 64u);
}

void multipleDirtyChannelsAreReconciledBeforePrimaryTraffic() {
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

    assert(tee.sendControlChange(2, 123, 0));
    usb.accept = true;
    const std::size_t usbBefore = usb.sent.size();
    assert(tee.sendNoteOn(4, 67, 100));
    assert(usb.sent.size() == usbBefore + 3u);
    assert(usb.sent[usbBefore].status == 0xB0u && usb.sent[usbBefore].d1 == 123u);
    assert((usb.sent[usbBefore].status & 0x0Fu) == 0u);
    assert(usb.sent[usbBefore + 1u].status == 0xB2u && usb.sent[usbBefore + 1u].d1 == 123u);
    assert(usb.sent[usbBefore + 2u].status == 0x94u && usb.sent[usbBefore + 2u].d1 == 67u);
}

void repeatedCleanupFailureKeepsOneDinNoteOnPerDispatch() {
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

    usb.accept = true;
    usb.rejectRecoveryCc123 = 2u;
    std::size_t dinBefore = din.sent.size();
    assert(tee.sendNoteOn(1, 62, 100));
    assert(din.sent.size() == dinBefore + 1u);
    assert((din.sent.back().status & 0xF0u) == 0x90u);
    assert(tee.primaryStalled());

    dinBefore = din.sent.size();
    assert(tee.sendNoteOn(1, 63, 100));
    assert(din.sent.size() == dinBefore + 1u);
    assert((din.sent.back().status & 0xF0u) == 0x90u);
    assert(tee.primaryStalled());

    dinBefore = din.sent.size();
    assert(tee.sendNoteOn(1, 64, 100));
    assert(din.sent.size() == dinBefore + 1u);
    assert((din.sent.back().status & 0xF0u) == 0x90u);
    assert(!tee.primaryStalled());
    assert(tee.diagnostics().primaryRecoveryCleanupRejects == 2u);
}
}  // namespace

int main() {
    recoveredPrimaryIsCleanedBeforeItRegainsAuthority();
    failedRecoveryCleanupKeepsPrimaryDemoted();
    cleanupDebtSurvivesDisconnectReconnect();
    multipleDirtyChannelsAreReconciledBeforePrimaryTraffic();
    repeatedCleanupFailureKeepsOneDinNoteOnPerDispatch();
    return 0;
}
