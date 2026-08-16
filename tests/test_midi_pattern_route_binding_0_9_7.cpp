#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_pattern_startup_routes.h"
#include "src/midi/usb_midi_output.h"

namespace {

enum class PacketType : uint8_t { NoteOn, NoteOff };

struct Packet {
    PacketType type;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

class FakeUsbMidiTransport final : public IUsbMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return mounted_; }

    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (!mounted_) return false;
        packets.push_back({PacketType::NoteOn, channel, note, velocity});
        return true;
    }

    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (!mounted_) return false;
        packets.push_back({PacketType::NoteOff, channel, note, velocity});
        return true;
    }

    void flush() override {}

    bool mounted_{true};
    std::vector<Packet> packets;
};

MusicalEvent patternEvent(MusicalEventType type,
                          MusicalEventTarget target,
                          uint8_t logicalChannel,
                          uint8_t note,
                          uint8_t velocity = 100) {
    return MusicalEvent{
        type,
        MusicalEventSource::PatternPlayer,
        target,
        logicalChannel,
        note,
        velocity,
    };
}

void expectPacket(const Packet& packet,
                  PacketType type,
                  uint8_t channel,
                  uint8_t note) {
    assert(packet.type == type);
    assert(packet.channel == channel);
    assert(packet.note == note);
}

GroovePuterMidi::MidiDeviceProfile parseProfile(const char* value) {
    if (std::strcmp(value, "seqtrak") == 0) {
        return GroovePuterMidi::MidiDeviceProfile::SeqtrakNative;
    }
    if (std::strcmp(value, "gm") == 0) {
        return GroovePuterMidi::MidiDeviceProfile::GeneralMidi;
    }
    if (std::strcmp(value, "generic") == 0) {
        return GroovePuterMidi::MidiDeviceProfile::GenericMidi;
    }
    assert(false && "unknown profile");
    return GroovePuterMidi::MidiDeviceProfile::SeqtrakNative;
}

void testProfile(GroovePuterMidi::MidiDeviceProfile profile) {
    const auto settings = GroovePuterMidi::makeDefaultMidiOutputSettings(profile);
    GroovePuterMidi::publishMidiPatternStartupRoutes(settings);

    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    const bool generic = profile == GroovePuterMidi::MidiDeviceProfile::GenericMidi;
    const bool gm = profile == GroovePuterMidi::MidiDeviceProfile::GeneralMidi;

    const uint8_t expectedSynthA = gm || generic ? 0 : 7;
    const uint8_t expectedSynthB = gm || generic ? 1 : 8;
    assert(output.channelFor(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == expectedSynthA);
    assert(output.channelFor(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == expectedSynthB);

    output.handleMusicalEvent(patternEvent(
        MusicalEventType::NoteOn, MusicalEventTarget::SynthA, 0, 64));
    assert(transport.packets.size() == 1);
    expectPacket(transport.packets.back(), PacketType::NoteOn, expectedSynthA, 64);
    output.handleMusicalEvent(patternEvent(
        MusicalEventType::NoteOff, MusicalEventTarget::SynthA, 0, 64, 0));
    assert(transport.packets.size() == 2);
    expectPacket(transport.packets.back(), PacketType::NoteOff, expectedSynthA, 64);

    transport.packets.clear();

    constexpr uint8_t kSeqtrakChannels[8] = {0, 1, 3, 4, 5, 6, 5, 2};
    constexpr uint8_t kGmNotes[8] = {36, 38, 42, 46, 43, 47, 37, 39};

    for (uint8_t voice = 0; voice < 8; ++voice) {
        output.handleMusicalEvent(patternEvent(
            MusicalEventType::NoteOn,
            MusicalEventTarget::Drums,
            voice,
            60,
            static_cast<uint8_t>(90 + voice)));

        if (generic) {
            assert(transport.packets.empty());
            continue;
        }

        assert(transport.packets.size() == 1);
        const uint8_t expectedChannel = gm ? 9 : kSeqtrakChannels[voice];
        const uint8_t expectedNote = gm ? kGmNotes[voice] : 60;
        expectPacket(transport.packets.back(),
                     PacketType::NoteOn,
                     expectedChannel,
                     expectedNote);
        assert(output.wireOwnerCount(expectedChannel, expectedNote) == 1);

        // Producer-side NoteOff still carries the historical logical N60. R5
        // must release the physical profile note captured at startup.
        output.handleMusicalEvent(patternEvent(
            MusicalEventType::NoteOff,
            MusicalEventTarget::Drums,
            voice,
            60,
            0));
        assert(transport.packets.size() == 2);
        expectPacket(transport.packets.back(),
                     PacketType::NoteOff,
                     expectedChannel,
                     expectedNote);
        assert(output.wireOwnerCount(expectedChannel, expectedNote) == 0);
        transport.packets.clear();
    }

    if (!gm) return;

    // A shared SMF owner on GM CH10/N36 must survive scoped Pattern cleanup.
    assert(output.handleSmfNoteOn(9, 36, 70));
    output.handleMusicalEvent(patternEvent(
        MusicalEventType::NoteOn,
        MusicalEventTarget::Drums,
        0,
        60,
        100));
    assert(output.wireOwnerCount(9, 36) == 2);

    const std::size_t beforePatternOff = transport.packets.size();
    output.handleMusicalEvent(patternEvent(
        MusicalEventType::NoteOff,
        MusicalEventTarget::Drums,
        0,
        60,
        0));
    assert(transport.packets.size() == beforePatternOff);
    assert(output.wireOwnerCount(9, 36) == 1);

    assert(output.handleSmfNoteOff(9, 36));
    assert(output.wireOwnerCount(9, 36) == 0);
}

}  // namespace

int main(int argc, char** argv) {
    assert(argc == 2);
    testProfile(parseProfile(argv[1]));
    return 0;
}
