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

// 0.9.17 Melody chords --------------------------------------------------------

MusicalEvent chordEvent(MusicalEventType type, uint8_t note) {
    return MusicalEvent{type,
                        MusicalEventSource::PatternPlayer,
                        MusicalEventTarget::SynthA,
                        0,
                        note,
                        100,
                        kMusicalEventChord};
}

MusicalEvent patternAllNotesOff() {
    return MusicalEvent{MusicalEventType::AllNotesOff,
                        MusicalEventSource::PatternPlayer,
                        MusicalEventTarget::SynthA, 0, 0, 0, 0};
}

int count(const std::vector<Packet>& packets, PacketType type, uint8_t note) {
    int n = 0;
    for (const Packet& packet : packets) {
        if (packet.type == type && packet.data1 == note) ++n;
    }
    return n;
}

void testChordNotesHoldTogetherAndReleaseIndividually() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    for (uint8_t note : {60, 64, 67}) {
        output.handleMusicalEvent(chordEvent(MusicalEventType::NoteOn, note));
    }
    assert(transport.packets.size() == 3);
    for (std::size_t i = 0; i < 3; ++i) {
        assert(transport.packets[i].type == PacketType::NoteOn);
        assert(transport.packets[i].channel == kSynthAChannel);
    }
    // A receiver GroovePuter never forced MONO is not touched.
    assert(!anyControlChange(transport.packets));

    output.handleMusicalEvent(chordEvent(MusicalEventType::NoteOff, 64));
    assert(count(transport.packets, PacketType::NoteOff, 64) == 1);
    assert(count(transport.packets, PacketType::NoteOff, 60) == 0);

    output.handleMusicalEvent(patternAllNotesOff());
    assert(count(transport.packets, PacketType::NoteOff, 60) == 1);
    assert(count(transport.packets, PacketType::NoteOff, 67) == 1);
    // Ownership is clean: a second release sends nothing more.
    output.handleMusicalEvent(patternAllNotesOff());
    assert(count(transport.packets, PacketType::NoteOff, 60) == 1);
    assert(count(transport.packets, PacketType::NoteOff, 67) == 1);
}

void testChordAfterSlideSwitchesReceiverToPoly() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(patternNoteOn(48, true));  // MONO + portamento
    transport.packets.clear();
    output.handleMusicalEvent(chordEvent(MusicalEventType::NoteOn, 60));
    bool sawPoly = false;
    std::size_t polyIndex = 0;
    std::size_t noteIndex = 0;
    for (std::size_t i = 0; i < transport.packets.size(); ++i) {
        const Packet& packet = transport.packets[i];
        if (packet.type == PacketType::ControlChange && packet.data1 == 26) {
            assert(packet.data2 == 1);
            sawPoly = true;
            polyIndex = i;
        }
        if (packet.type == PacketType::NoteOn && packet.data1 == 60) noteIndex = i;
    }
    assert(sawPoly && polyIndex < noteIndex);
    assert(count(transport.packets, PacketType::NoteOff, 48) == 1);

    // The next slide puts the receiver back into MONO.
    transport.packets.clear();
    output.handleMusicalEvent(patternNoteOn(50, true));
    bool sawMono = false;
    for (const Packet& packet : transport.packets) {
        if (packet.type == PacketType::ControlChange && packet.data1 == 26) {
            assert(packet.data2 == 0);
            sawMono = true;
        }
    }
    assert(sawMono);
}

void testOneNoteOnsetEndsTheChordFirst() {
    publishProfile(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    output.handleMusicalEvent(chordEvent(MusicalEventType::NoteOn, 60));
    output.handleMusicalEvent(chordEvent(MusicalEventType::NoteOn, 64));
    transport.packets.clear();
    output.handleMusicalEvent(patternNoteOn(50, false));
    assert(transport.packets.size() == 3);
    assert(transport.packets[0].type == PacketType::NoteOff);
    assert(transport.packets[1].type == PacketType::NoteOff);
    expectNoteOn(transport.packets[2], kSynthAChannel, 50);
}

int main() {
    testChordNotesHoldTogetherAndReleaseIndividually();
    testChordAfterSlideSwitchesReceiverToPoly();
    testOneNoteOnsetEndsTheChordFirst();
    testPatternWithoutSlideSendsNoControlChange();
    testSlideSwitchesReceiverAndTogglesPortamento();
    testStopReleasesPortamento();
    testPerformancePolyForcesMonoAgainForNextSlide();
    testNonSeqtrakProfileNeverSendsVendorControlChanges();
    return 0;
}
