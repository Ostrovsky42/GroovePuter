// CALL/RESPONSE phase 3: the answer MIDI channel. An explicit channel (PARTNER
// CHANNEL 1..16) plays on UsbMidiOutput's answer lane on that channel (USB and
// DIN through the tee); the default (DX ROUTE) keeps the phase-1/2 output on the
// PERFORM DX route. Cancel releases exactly the answer's own notes, on the
// channel of their NoteOn; NoteOn/NoteOff stay balanced per wire channel.
#include <cassert>
#include <cstdio>
#include <vector>

#include "src/input/call_response_listener.h"
#include "src/midi/tee_midi_transport.h"
#include "src/midi/usb_midi_output.h"

using namespace GroovePuterCallResponse;

namespace {

struct Packet {
    bool on;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

class FakeWire final : public IMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return true; }
    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({true, channel, note, velocity});
        return true;
    }
    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (failNoteOffs > 0) {
            --failNoteOffs;
            return false;
        }
        packets.push_back({false, channel, note, velocity});
        return true;
    }
    bool sendControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
        controls.push_back({true, channel, controller, value});
        return true;
    }
    void flush() override {}

    int count(bool on, uint8_t channel) const {
        int n = 0;
        for (const Packet& p : packets) n += p.on == on && p.channel == channel;
        return n;
    }
    // Every NoteOn has its NoteOff on the same channel/note, never an extra one.
    void assertBalanced() const {
        int open[16][128]{};
        for (const Packet& p : packets) {
            if (p.on) {
                ++open[p.channel][p.note];
            } else {
                assert(open[p.channel][p.note] > 0);
                --open[p.channel][p.note];
            }
        }
        for (auto& row : open) for (int n : row) assert(n == 0);
    }

    std::vector<Packet> packets;
    std::vector<Packet> controls;
    int failNoteOffs{0};
};

struct Rig {
    FakeWire usb;
    FakeWire din;
    GroovePuterMidi::TeeMidiTransport tee{usb, din};
    UsbMidiOutput output{tee};
    MusicalEventRouter router;

    Rig() {
        tee.setSecondaryEnabled(true);
        assert(output.begin());
        output.pollConnection();
        assert(output.status() == UsbMidiStatus::Ready);
        assert(router.addSink(output));
        usb.packets.clear();
        din.packets.clear();
    }
};

constexpr uint8_t kDxWire = 9;  // UsbMidiRouteConfig::performanceDxChannel (MIDI ch 10)

ResponsePlan twoNotes() {
    ResponsePlan plan;
    plan.count = 2;
    plan.windowSteps = 31;
    plan.notes[0] = ResponseNote{0, 4, 60, 80};
    plan.notes[1] = ResponseNote{4, 4, 64, 80};
    return plan;
}

constexpr uint32_t kSixteenth = 125000;

// Channel 5: NoteOn and NoteOff on wire channel 4, on USB and on DIN.
void testChannelFivePlaysOnChannelFiveBothWires() {
    Rig rig;
    ResponsePlayer player;
    player.setRouter(&rig.router);
    player.setWireChannel(5);
    assert(player.start(twoNotes(), 0, kSixteenth));
    player.service(0);
    player.service(4 * kSixteenth);
    player.service(8 * kSixteenth);
    for (FakeWire* wire : {&rig.usb, &rig.din}) {
        assert(wire->count(true, 4) == 2 && wire->count(false, 4) == 2);
        assert(wire->count(true, kDxWire) == 0);
        assert(wire->packets[0].on && wire->packets[0].note == 60 && wire->packets[0].velocity == 80);
        wire->assertBalanced();
    }
}

// Cancel mid-answer: one NoteOff on channel 5 for the sounding note, at once.
void testCancelReleasesOnTheSameChannel() {
    Rig rig;
    ResponsePlayer player;
    player.setRouter(&rig.router);
    player.setWireChannel(5);
    assert(player.start(twoNotes(), 0, kSixteenth));
    player.service(5 * kSixteenth);  // note 2 sounding
    assert(player.soundingCount() == 1);
    // The setting changes while the answer runs: its NoteOffs stay on channel 5.
    player.setWireChannel(12);
    assert(player.cancel() == 1);
    for (FakeWire* wire : {&rig.usb, &rig.din}) {
        assert(!wire->packets.back().on && wire->packets.back().channel == 4 &&
               wire->packets.back().note == 64);
        assert(wire->count(true, 11) == 0 && wire->count(false, 11) == 0);
        wire->assertBalanced();
    }
    // The next answer uses the new channel.
    assert(player.start(twoNotes(), 10 * kSixteenth, kSixteenth));
    player.service(10 * kSixteenth);
    assert(rig.usb.packets.back().on && rig.usb.packets.back().channel == 11);
    player.cancel();
    rig.usb.assertBalanced();
}

void testLoopChannelChangeKeepsOriginalNoteOffChannel() {
    Rig rig;
    ResponsePlayer player;
    player.setRouter(&rig.router);
    player.setWireChannel(5);
    assert(player.start(twoNotes(), 0, kSixteenth, true));
    player.service(0);
    player.setWireChannel(12);
    player.service(4 * kSixteenth);
    player.service(8 * kSixteenth);
    for (FakeWire* wire : {&rig.usb, &rig.din}) {
        assert(wire->count(true, 4) == 2 && wire->count(false, 4) == 2);
        assert(wire->count(true, 11) == 0 && wire->count(false, 11) == 0);
        wire->assertBalanced();
    }
}

// Default (follow DX): exactly the phase-1/2 output, on the DX route.
void testFollowDxIsThePhaseTwoOutput() {
    Rig rig;
    ResponsePlayer player;
    player.setRouter(&rig.router);
    assert(player.wireChannel() == ResponsePlayer::kFollowDx);
    assert(player.start(twoNotes(), 0, kSixteenth));
    player.service(0);
    player.service(4 * kSixteenth);
    player.service(8 * kSixteenth);
    assert(rig.usb.count(true, kDxWire) == 2 && rig.usb.count(false, kDxWire) == 2);
    assert(rig.din.count(true, kDxWire) == 2);
    rig.usb.assertBalanced();
}

// Shared channel/note with PERFORM: the answer's NoteOff does not cut the
// person's note (ownership through the same table), and vice versa.
void testSharedNoteKeepsOtherOwner() {
    Rig rig;
    // PERFORM DX plays 60 on channel 10; the answer plays 60 on channel 10 too.
    rig.router.route(MusicalEvent{MusicalEventType::NoteOn, MusicalEventSource::PerformanceKeyboard,
                                  MusicalEventTarget::Dx, 0, 60, 100});
    ResponsePlayer player;
    player.setRouter(&rig.router);
    player.setWireChannel(10);
    assert(player.start(twoNotes(), 0, kSixteenth));
    player.service(0);
    assert(rig.usb.count(true, kDxWire) == 1);  // already sounding: one wire NoteOn
    assert(rig.output.wireOwnerCount(kDxWire, 60) == 2);
    player.service(4 * kSixteenth);             // answer 60 off, 64 on
    assert(rig.usb.count(false, kDxWire) == 0); // the person still holds 60
    assert(rig.output.wireOwnerCount(kDxWire, 60) == 1);
    rig.router.route(MusicalEvent{MusicalEventType::NoteOff, MusicalEventSource::PerformanceKeyboard,
                                  MusicalEventTarget::Dx, 0, 60, 0});
    player.cancel();
    rig.usb.assertBalanced();
    assert(rig.output.wireOwnerCount(kDxWire, 60) == 0);
    assert(rig.output.wireOwnerCount(kDxWire, 64) == 0);
}

// Follow DX must keep human and partner ownership separate on the same note.
void testFollowDxSharesNoteSafely() {
    Rig rig;
    rig.router.route(MusicalEvent{MusicalEventType::NoteOn, MusicalEventSource::PerformanceKeyboard,
                                  MusicalEventTarget::Dx, 0, 60, 100});
    ResponsePlayer player;
    player.setRouter(&rig.router);
    assert(player.start(twoNotes(), 0, kSixteenth));
    player.service(0);
    assert(rig.output.wireOwnerCount(kDxWire, 60) == 2);
    player.service(4 * kSixteenth);
    assert(rig.output.wireOwnerCount(kDxWire, 60) == 1);
    assert(rig.usb.count(false, kDxWire) == 0);
    rig.router.route(MusicalEvent{MusicalEventType::NoteOff, MusicalEventSource::PerformanceKeyboard,
                                  MusicalEventTarget::Dx, 0, 60, 0});
    player.cancel();
    rig.usb.assertBalanced();
}

// A failed NoteOff is retried on the next event; a Dx panic (also the control
// queue's overflow fallback) releases the answer lane.
void testRetryAndPanic() {
    Rig rig;
    ResponsePlayer player;
    player.setRouter(&rig.router);
    player.setWireChannel(3);
    ResponsePlan plan = twoNotes();
    plan.notes[0].length = 2;  // a gap: 60 ends at step 2, 64 starts at step 4
    assert(player.start(plan, 0, kSixteenth));
    player.service(0);
    rig.usb.failNoteOffs = 1;        // USB rejects the next NoteOff
    player.service(2 * kSixteenth);  // 60 off is not on the wire yet
    assert(rig.usb.count(false, 2) == 0);
    rig.router.route(MusicalEvent{MusicalEventType::NoteOn, MusicalEventSource::PatternPlayer,
                                  MusicalEventTarget::SynthA, 0, 40, 90});
    assert(rig.usb.count(false, 2) == 1);  // retried before the unrelated event
    rig.router.route(MusicalEvent{MusicalEventType::NoteOff, MusicalEventSource::PatternPlayer,
                                  MusicalEventTarget::SynthA, 0, 40, 0});
    player.service(4 * kSixteenth);
    player.cancel();
    rig.usb.assertBalanced();

    Rig panic;
    ResponsePlayer second;
    second.setRouter(&panic.router);
    second.setWireChannel(3);
    assert(second.start(twoNotes(), 0, kSixteenth));
    second.service(0);
    panic.router.route(MusicalEvent{MusicalEventType::AllNotesOff,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::Dx, 0, 0, 0});
    assert(panic.usb.count(false, 2) == 1);
    second.cancel();  // its NoteOff finds nothing left: no second wire NoteOff
    assert(panic.usb.count(false, 2) == 1);
    panic.usb.assertBalanced();
}

// A rejected final NoteOff must be retried even when no further events arrive.
void testIdlePollRetriesFinalNoteOff() {
    Rig rig;
    ResponsePlayer player;
    player.setRouter(&rig.router);
    player.setWireChannel(3);
    ResponsePlan plan = twoNotes();
    plan.count = 1;
    assert(player.start(plan, 0, kSixteenth));
    player.service(0);
    rig.usb.failNoteOffs = 1;
    player.service(4 * kSixteenth);
    assert(!player.active());
    assert(rig.usb.count(false, 2) == 0);
    rig.output.pollConnection();
    assert(rig.usb.count(false, 2) == 1);
    rig.usb.assertBalanced();
    rig.din.assertBalanced();
}

// Through the listener: the channel setting reaches the answer.
void testListenerChannelSetting() {
    CallResponseListener listener;
    assert(listener.answerChannel() == CallResponseListener::kAnswerChannelFollowDx);
    listener.setAnswerChannel(5);
    assert(listener.responder().wireChannel() == 5);
    listener.cycleAnswerChannel(1);
    assert(listener.answerChannel() == 6);
    listener.setAnswerChannel(16);
    listener.cycleAnswerChannel(1);
    assert(listener.answerChannel() == 0);  // wraps to DX ROUTE
    listener.cycleAnswerChannel(-1);
    assert(listener.answerChannel() == 16);
}

}  // namespace

int main() {
    testChannelFivePlaysOnChannelFiveBothWires();
    testCancelReleasesOnTheSameChannel();
    testLoopChannelChangeKeepsOriginalNoteOffChannel();
    testFollowDxIsThePhaseTwoOutput();
    testSharedNoteKeepsOtherOwner();
    testFollowDxSharesNoteSafely();
    testRetryAndPanic();
    testIdlePollRetriesFinalNoteOff();
    testListenerChannelSetting();
    std::puts("cr answer channel tests passed");
    return 0;
}
