// Pattern slide on the MIDI wire (SEQTRAK): portamento is driven per note with
// CC65, and the receiver is switched to MONO (CC26=0) with a portamento time
// (CC5) only when a Pattern actually slides. Every CC precedes its NoteOn.
#include <cassert>
#include <cstdint>
#include <vector>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_pattern_startup_routes.h"
#include "src/midi/usb_midi_output.h"

namespace {

enum class PacketType : uint8_t { NoteOn, NoteOff, ControlChange };
struct Packet {
    PacketType type;
    uint8_t channel;
    uint8_t data1;
    uint8_t data2;
};

class FakeUsbMidiTransport final : public IUsbMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return true; }
    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({PacketType::NoteOn, channel, note, velocity});
        return true;
    }
    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({PacketType::NoteOff, channel, note, velocity});
        return true;
    }
    bool sendControlChange(uint8_t channel, uint8_t controller, uint8_t value) override {
        packets.push_back({PacketType::ControlChange, channel, controller, value});
        return true;
    }
    void flush() override {}
    std::vector<Packet> packets;
};

constexpr uint8_t kSynthAChannel = 7;  // SEQTRAK SYNTH 1 (CH8)

MusicalEvent patternNoteOn(uint8_t note, bool slide,
                           MusicalEventTarget target = MusicalEventTarget::SynthA) {
    return MusicalEvent{MusicalEventType::NoteOn,
                        MusicalEventSource::PatternPlayer,
                        target,
                        0,
                        note,
                        100,
                        slide ? kMusicalEventSlide : uint8_t{0}};
}

void expectCc(const Packet& packet, uint8_t channel, uint8_t controller, uint8_t value) {
    assert(packet.type == PacketType::ControlChange);
    assert(packet.channel == channel);
    assert(packet.data1 == controller);
    assert(packet.data2 == value);
}

void expectNoteOn(const Packet& packet, uint8_t channel, uint8_t note) {
    assert(packet.type == PacketType::NoteOn);
    assert(packet.channel == channel);
    assert(packet.data1 == note);
}

bool anyControlChange(const std::vector<Packet>& packets) {
    for (const Packet& packet : packets) {
        if (packet.type == PacketType::ControlChange) return true;
    }
    return false;
}

void publishProfile(GroovePuterMidi::MidiDeviceProfile profile) {
    GroovePuterMidi::publishMidiPatternStartupRoutes(
        GroovePuterMidi::makeDefaultMidiOutputSettings(profile));
}

void testPatternWithoutSlideSendsNoControlChange() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(patternNoteOn(48, false));
    output.handleMusicalEvent(patternNoteOn(50, false));
    // The user's SEQTRAK voicing is left alone by a Pattern that never slides.
    assert(!anyControlChange(transport.packets));
}

void testSlideSwitchesReceiverAndTogglesPortamento() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(patternNoteOn(48, false));
    transport.packets.clear();

    // First slide: MONO + portamento time + switch on, all before the NoteOn.
    output.handleMusicalEvent(patternNoteOn(55, true));
    assert(transport.packets.size() == 5);
    expectCc(transport.packets[0], kSynthAChannel,
             UsbMidiOutput::kSeqtrakMonoPolyController,
             UsbMidiOutput::kSeqtrakMonoValue);
    expectCc(transport.packets[1], kSynthAChannel,
             UsbMidiOutput::kPortamentoTimeController,
             UsbMidiOutput::kSeqtrakSlidePortamentoTime);
    expectCc(transport.packets[2], kSynthAChannel,
             UsbMidiOutput::kPortamentoSwitchController, 1);
    assert(transport.packets[3].type == PacketType::NoteOff);
    expectNoteOn(transport.packets[4], kSynthAChannel, 55);
    transport.packets.clear();

    // A second consecutive slide re-sends nothing: state is cached.
    output.handleMusicalEvent(patternNoteOn(53, true));
    assert(!anyControlChange(transport.packets));
    transport.packets.clear();

    // A plain note turns portamento off before it sounds.
    output.handleMusicalEvent(patternNoteOn(48, false));
    expectCc(transport.packets[0], kSynthAChannel,
             UsbMidiOutput::kPortamentoSwitchController, 0);
    expectNoteOn(transport.packets.back(), kSynthAChannel, 48);
    transport.packets.clear();

    // Sliding again only flips the switch; MONO and time were already sent.
    output.handleMusicalEvent(patternNoteOn(55, true));
    expectCc(transport.packets[0], kSynthAChannel,
             UsbMidiOutput::kPortamentoSwitchController, 1);
    assert(transport.packets[1].type != PacketType::ControlChange);
}

void testStopReleasesPortamento() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(patternNoteOn(55, true));
    transport.packets.clear();

    output.handleMusicalEvent(MusicalEvent{MusicalEventType::AllNotesOff,
                                           MusicalEventSource::PatternPlayer,
                                           MusicalEventTarget::SynthA,
                                           0, 0, 0});
    // Stop must not leave SEQTRAK gliding for the user's own playing.
    bool portamentoOff = false;
    for (const Packet& packet : transport.packets) {
        if (packet.type == PacketType::ControlChange &&
            packet.data1 == UsbMidiOutput::kPortamentoSwitchController) {
            assert(packet.channel == kSynthAChannel && packet.data2 == 0);
            portamentoOff = true;
        }
    }
    assert(portamentoOff);
}

void testPerformancePolyForcesMonoAgainForNextSlide() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(patternNoteOn(55, true));
    // Direct POLY performance switches the same SEQTRAK track to POLY.
    output.handleMusicalEvent(MusicalEvent{MusicalEventType::NoteOn,
                                           MusicalEventSource::PerformanceKeyboardPoly,
                                           MusicalEventTarget::SynthA,
                                           0, 60, 100});
    output.handleMusicalEvent(MusicalEvent{MusicalEventType::NoteOff,
                                           MusicalEventSource::PerformanceKeyboardPoly,
                                           MusicalEventTarget::SynthA,
                                           0, 60, 0});
    transport.packets.clear();

    output.handleMusicalEvent(patternNoteOn(57, true));
    expectCc(transport.packets[0], kSynthAChannel,
             UsbMidiOutput::kSeqtrakMonoPolyController,
             UsbMidiOutput::kSeqtrakMonoValue);
}

void testNonSeqtrakProfileNeverSendsVendorControlChanges() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::GeneralMidi);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(patternNoteOn(48, false));
    output.handleMusicalEvent(patternNoteOn(55, true));
    output.handleMusicalEvent(patternNoteOn(48, false));
    assert(!anyControlChange(transport.packets));
}

}  // namespace

int main() {
    testPatternWithoutSlideSendsNoControlChange();
    testSlideSwitchesReceiverAndTogglesPortamento();
    testStopReleasesPortamento();
    testPerformancePolyForcesMonoAgainForNextSlide();
    testNonSeqtrakProfileNeverSendsVendorControlChanges();
    return 0;
}
