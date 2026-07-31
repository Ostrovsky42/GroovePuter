#include <cassert>
#include <cstdint>
#include <vector>

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
    bool begin() override {
        ++beginCalls;
        return beginResult;
    }

    bool mounted() const override { return mountedState; }

    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (!mountedState || !sendResult) return false;
        packets.push_back({PacketType::NoteOn, channel, note, velocity});
        return true;
    }

    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        if (noteOffFailuresRemaining > 0) {
            --noteOffFailuresRemaining;
            return false;
        }
        if (!mountedState || !sendResult) return false;
        packets.push_back({PacketType::NoteOff, channel, note, velocity});
        return true;
    }

    void flush() override { ++flushCalls; }

    void clear() {
        packets.clear();
        flushCalls = 0;
    }

    bool beginResult{true};
    bool mountedState{false};
    bool sendResult{true};
    int noteOffFailuresRemaining{0};
    int beginCalls{0};
    int flushCalls{0};
    std::vector<Packet> packets;
};

class CountingSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent&) override { ++count; }
    int count{0};
};

MusicalEvent event(
    MusicalEventType type,
    uint8_t note,
    uint8_t velocity = 0,
    MusicalEventSource source = MusicalEventSource::PerformanceKeyboard,
    MusicalEventTarget target = MusicalEventTarget::SynthA) {
    return MusicalEvent{type, source, target, 0, note, velocity};
}

void expectPacket(const Packet& packet,
                  PacketType type,
                  uint8_t channel,
                  uint8_t note,
                  uint8_t velocity) {
    assert(packet.type == type);
    assert(packet.channel == channel);
    assert(packet.note == note);
    assert(packet.velocity == velocity);
}
}  // namespace

int main() {
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);

    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 100));
    assert(transport.packets.empty());
    assert(output.status() == UsbMidiStatus::Off);

    assert(output.begin());
    assert(output.status() == UsbMidiStatus::Wait);
    transport.mountedState = true;
    output.pollConnection();
    assert(output.status() == UsbMidiStatus::Ready);

    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 7);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthB) == 8);
    assert(output.channelFor(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::Drums) == 9);
    assert(output.channelFor(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 7);
    assert(output.channelFor(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 8);

    // Live A/B/Drums and Pattern A/B are independent logical lanes.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 100));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 40, 95,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthB));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 42, 90,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::Drums));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 48, 85,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 52, 80,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    assert(transport.packets.size() == 5);
    expectPacket(transport.packets[0], PacketType::NoteOn, 7, 36, 100);
    expectPacket(transport.packets[1], PacketType::NoteOn, 8, 40, 95);
    expectPacket(transport.packets[2], PacketType::NoteOn, 9, 42, 90);
    expectPacket(transport.packets[3], PacketType::NoteOn, 7, 48, 85);
    expectPacket(transport.packets[4], PacketType::NoteOn, 8, 52, 80);

    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::Drums));
    assert(transport.packets.size() == 6);
    expectPacket(transport.packets.back(), PacketType::NoteOff, 9, 42, 0);
    assert(output.activeNote(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthA) == 36);
    assert(output.activeNote(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::SynthB) == 40);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 48);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthB) == 52);

    // Pattern A replacement does not cancel any other lane.
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 50, 91,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 8);
    expectPacket(transport.packets[6], PacketType::NoteOff, 7, 48, 0);
    expectPacket(transport.packets[7], PacketType::NoteOn, 7, 50, 91);

    // Live and Pattern Synth A share channel 8. Equal pitches are reference
    // counted at wire level, so a live panic cannot silence PatternPlayer.
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0));
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthB));
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthB));
    transport.clear();

    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60, 90,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60, 110));
    assert(transport.packets.size() == 1);
    expectPacket(transport.packets[0], PacketType::NoteOn, 7, 60, 90);
    assert(output.wireOwnerCount(7, 60) == 2);

    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0));
    assert(transport.packets.size() == 1);
    assert(output.wireOwnerCount(7, 60) == 1);
    output.handleMusicalEvent(event(MusicalEventType::AllNotesOff, 0, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 2);
    expectPacket(transport.packets[1], PacketType::NoteOff, 7, 60, 0);
    assert(output.wireOwnerCount(7, 60) == 0);

    // Failed old-note release is retried before a later mismatched NoteOff.
    transport.clear();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 55, 88,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    transport.noteOffFailuresRemaining = 1;
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 57, 77,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 1);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == 55);
    output.handleMusicalEvent(event(MusicalEventType::NoteOff, 57, 0,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::SynthA));
    assert(transport.packets.size() == 2);
    expectPacket(transport.packets[1], PacketType::NoteOff, 7, 55, 0);
    assert(output.activeNote(MusicalEventSource::PatternPlayer,
                             MusicalEventTarget::SynthA) == -1);

    // Unsupported producers remain outside the fixed Stage 1 routes.
    transport.clear();
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 60, 100,
                                    MusicalEventSource::Arpeggiator));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 61, 100,
                                    MusicalEventSource::MidiInput));
    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 62, 100,
                                    MusicalEventSource::PatternPlayer,
                                    MusicalEventTarget::Drums));
    assert(transport.packets.empty());

    // Router fan-out remains independent.
    MusicalEventRouter router;
    CountingSink internalLikeSink;
    assert(router.addSink(internalLikeSink));
    assert(router.addSink(output));
    router.route(event(MusicalEventType::NoteOn, 64, 70,
                       MusicalEventSource::PerformanceKeyboard,
                       MusicalEventTarget::Drums));
    assert(internalLikeSink.count == 1);
    assert(transport.packets.size() == 1);
    expectPacket(transport.packets[0], PacketType::NoteOn, 9, 64, 70);

    // Disconnect clears every lane and does not replay stale notes.
    transport.mountedState = false;
    output.pollConnection();
    assert(output.status() == UsbMidiStatus::Wait);
    assert(output.activeNote(MusicalEventSource::PerformanceKeyboard,
                             MusicalEventTarget::Drums) == -1);
    assert(output.wireOwnerCount(9, 64) == 0);
    transport.mountedState = true;
    output.pollConnection();

    output.handleMusicalEvent(event(MusicalEventType::NoteOn, 65, 71,
                                    MusicalEventSource::PerformanceKeyboard,
                                    MusicalEventTarget::SynthB));
    const std::size_t beforeDisable = transport.packets.size();
    output.setEnabled(false);
    assert(output.status() == UsbMidiStatus::Off);
    assert(transport.packets.size() == beforeDisable + 1);
    expectPacket(transport.packets.back(), PacketType::NoteOff, 8, 65, 0);

    // Original five-field aggregate initializers retain their old meaning.
    FakeUsbMidiTransport clampedTransport;
    clampedTransport.mountedState = true;
    UsbMidiOutput clampedOutput(
        clampedTransport,
        UsbMidiRouteConfig{99, 98, 97, true, true});
    assert(clampedOutput.begin());
    clampedOutput.pollConnection();
    clampedOutput.handleMusicalEvent(event(MusicalEventType::NoteOn, 62, 64,
                                            MusicalEventSource::PatternPlayer,
                                            MusicalEventTarget::SynthB));
    assert(clampedTransport.packets.size() == 1);
    expectPacket(clampedTransport.packets[0], PacketType::NoteOn, 15, 62, 64);

    FakeUsbMidiTransport liveOnlyTransport;
    liveOnlyTransport.mountedState = true;
    UsbMidiOutput liveOnly(
        liveOnlyTransport,
        UsbMidiRouteConfig{7, 7, 8, true, false});
    assert(liveOnly.begin());
    liveOnly.pollConnection();
    liveOnly.handleMusicalEvent(event(MusicalEventType::NoteOn, 60, 100,
                                      MusicalEventSource::PatternPlayer,
                                      MusicalEventTarget::SynthA));
    assert(liveOnlyTransport.packets.empty());
    liveOnly.handleMusicalEvent(event(MusicalEventType::NoteOn, 36, 100,
                                      MusicalEventSource::PerformanceKeyboard,
                                      MusicalEventTarget::Drums));
    assert(liveOnlyTransport.packets.size() == 1);
    expectPacket(liveOnlyTransport.packets[0], PacketType::NoteOn, 9, 36, 100);

    return 0;
}
