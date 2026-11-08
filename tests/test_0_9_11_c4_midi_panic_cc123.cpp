#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/usb_midi_output.h"
#include "src/input/musical_event_router.h"
#include "src/input/performance_intent.h"
#include "src/input/performance_keyboard.h"

namespace {

enum class PacketKind : uint8_t { NoteOn, NoteOff, ControlChange, PitchBend };
struct Packet { PacketKind kind; uint8_t channel; uint8_t data1; uint8_t data2; };

class FakeTransport final : public IUsbMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return isMounted; }
    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({PacketKind::NoteOn, channel, note, velocity}); return true;
    }
    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({PacketKind::NoteOff, channel, note, velocity}); return true;
    }
    bool sendControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
        packets.push_back({PacketKind::ControlChange, channel, controller, value}); return true;
    }
    bool sendPitchBend(uint8_t channel, uint16_t value14) override {
        if (pitchBendFailuresRemaining > 0) {
            --pitchBendFailuresRemaining;
            return false;
        }
        pitchBends.push_back({channel, value14});
        return true;
    }
    void flush() override {}
    void clear() { packets.clear(); }
    struct Bend { uint8_t channel; uint16_t value14; };
    int pitchBendFailuresRemaining{0};
    std::vector<Packet> packets;
    std::vector<Bend> pitchBends;
    bool isMounted{true};
};

MusicalEvent event(MusicalEventType type, MusicalEventSource source,
                   MusicalEventTarget target, uint8_t note = 0, uint8_t velocity = 0) {
    return MusicalEvent{type, source, target, 0, note, velocity};
}

void expectCc123(const Packet& packet, uint8_t channel) {
    assert(packet.kind == PacketKind::ControlChange);
    assert(packet.channel == channel);
    assert(packet.data1 == 123 && packet.data2 == 0);
}

void emptyLocalOwnershipStillEmitsTerminalChannelRecovery() {
    FakeTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 1);
    expectCc123(transport.packets[0], 7);
}

void recoveryWaitsForOtherOwnerBeforeCc123() {
    FakeTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA, 60, 90));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthA, 60, 110));
    assert(output.wireOwnerCount(7, 60) == 2);
    transport.clear();

    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(output.wireOwnerCount(7, 60) == 1);
    assert(transport.packets.empty());

    output.handleMusicalEvent(event(MusicalEventType::NoteOff,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthA, 60));
    assert(output.wireOwnerCount(7, 60) == 0);
    assert(transport.packets.size() == 2);
    assert(transport.packets[0].kind == PacketKind::NoteOff);
    expectCc123(transport.packets[1], 7);
}

class RouterOutput final : public IMusicalEventSink {
public:
    explicit RouterOutput(UsbMidiOutput& output) : output_(output) {}
    void handleMusicalEvent(const MusicalEvent& event) override {
        output_.handleMusicalEvent(event);
    }
private:
    UsbMidiOutput& output_;
};

void scopedInputReleaseNeverPanicsSharedWireChannel() {
    FakeTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    MusicalEventRouter router;
    RouterOutput outputSink(output);
    assert(router.addSink(outputSink));
    PerformanceKeyboard keyboard(router);
    keyboard.setTarget(MusicalEventTarget::SynthA);
    keyboard.setVoiceMode(PerformanceVoiceMode::Poly);

    output.handleMusicalEvent(event(MusicalEventType::NoteOn,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA, 60, 80));
    const PerformanceInputIdentity usb{7, 1, 0, 60, 0};
    assert(keyboard.externalNoteOn(60, 100, usb));
    assert(output.wireOwnerCount(7, 60) == 2);
    transport.clear();

    assert(keyboard.releaseExternalInput(1, 7) == 1);
    assert(output.wireOwnerCount(7, 60) == 1);
    for (const Packet& packet : transport.packets) {
        assert(packet.kind != PacketKind::ControlChange ||
               (packet.data1 != 120u && packet.data1 != 123u));
    }
    assert(transport.packets.empty()); // Pattern still owns the wire pitch.
}

void pitchBendRetryKeepsNeutralAndDropsOldSessionValue() {
    FakeTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    transport.pitchBendFailuresRemaining = 2;
    output.handleMusicalEvent(MusicalEvent{
        MusicalEventType::PitchBend, MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA, 0, 0, 0, 0, 4500});
    output.handleMusicalEvent(MusicalEvent{
        MusicalEventType::PitchBend, MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA, 0, 0, 0, 0, 8192});
    assert(transport.pitchBends.empty());

    transport.pitchBendFailuresRemaining = 0;
    output.servicePitchBendRetries();
    assert(transport.pitchBends.size() == 1u);
    assert(transport.pitchBends[0].value14 == 8192u);

    transport.pitchBendFailuresRemaining = 1;
    output.handleMusicalEvent(MusicalEvent{
        MusicalEventType::PitchBend, MusicalEventSource::PerformanceKeyboard,
        MusicalEventTarget::SynthA, 0, 0, 0, 0, 4500});
    transport.isMounted = false;
    output.pollConnection();
    transport.isMounted = true;
    output.pollConnection();
    output.servicePitchBendRetries();
    assert(transport.pitchBends.size() == 1u); // Old session's pending value was discarded.
}

}  // namespace

int main() {
    emptyLocalOwnershipStillEmitsTerminalChannelRecovery();
    recoveryWaitsForOtherOwnerBeforeCc123();
    scopedInputReleaseNeverPanicsSharedWireChannel();
    pitchBendRetryKeepsNeutralAndDropsOldSessionValue();
    return 0;
}
