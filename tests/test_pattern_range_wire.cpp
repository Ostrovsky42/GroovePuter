// Pattern range above B4 on the MIDI wire (PATTERN_RANGE_MIDI): a Pattern
// NoteOn for C#5 (73), E5 (76) or C7 (96) leaves UsbMidiOutput with the same
// note number on the Synth A lane (USB and DIN share this sink), both for the
// default profile and SEQTRAK. The step editor clamp reaches C7, not B4.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "src/midi/midi_companion_settings.h"
#include "src/midi/midi_pattern_startup_routes.h"
#include "src/midi/usb_midi_output.h"
#include "src/state/synth_pattern_edit.h"

namespace {

struct Packet {
    bool on;
    uint8_t channel;
    uint8_t note;
    uint8_t velocity;
};

class FakeUsbMidiTransport final : public IUsbMidiTransport {
public:
    bool begin() override { return true; }
    bool mounted() const override { return true; }
    bool sendNoteOn(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({true, channel, note, velocity});
        return true;
    }
    bool sendNoteOff(uint8_t channel, uint8_t note, uint8_t velocity) override {
        packets.push_back({false, channel, note, velocity});
        return true;
    }
    bool sendControlChange(uint8_t, uint8_t, uint8_t) override { return true; }
    void flush() override {}
    std::vector<Packet> packets;
};

MusicalEvent patternEvent(MusicalEventType type, uint8_t note) {
    return MusicalEvent{type,
                        MusicalEventSource::PatternPlayer,
                        MusicalEventTarget::SynthA,
                        0,
                        note,
                        static_cast<uint8_t>(type == MusicalEventType::NoteOn ? 100 : 0),
                        0};
}

void expectWireNotes(GroovePuterMidi::MidiDeviceProfile profile) {
    GroovePuterMidi::publishMidiPatternStartupRoutes(
        GroovePuterMidi::makeDefaultMidiOutputSettings(profile));
    FakeUsbMidiTransport transport;
    UsbMidiOutput output(transport);
    assert(output.begin());
    output.pollConnection();

    const uint8_t notes[] = {71, 73, 76, 84, 96, 127};
    for (uint8_t note : notes) {
        transport.packets.clear();
        output.handleMusicalEvent(patternEvent(MusicalEventType::NoteOn, note));
        bool sawOn = false;
        for (const Packet& p : transport.packets) {
            if (p.on) {
                assert(p.note == note);  // never clipped to 71
                sawOn = true;
            }
        }
        assert(sawOn);
        transport.packets.clear();
        output.handleMusicalEvent(patternEvent(MusicalEventType::NoteOff, note));
        bool sawOff = false;
        for (const Packet& p : transport.packets) {
            if (!p.on) {
                assert(p.note == note);
                sawOff = true;
            }
        }
        assert(sawOff);
    }
}

void expectEditorCeiling() {
    using namespace GroovePuterUndo::PatternEdit;
    static_assert(kMaxPatternNote == 96, "steps hold up to C7");
    SynthPattern pattern{};
    pattern.steps[0].note = 64;
    adjustOctave(pattern, 0, 1);
    assert(pattern.steps[0].note == 76);   // E4 -> E5
    pattern.steps[1].note = 72;
    adjustNote(pattern, 1, 1);
    assert(pattern.steps[1].note == 73);   // C5 -> C#5
    adjustOctave(pattern, 1, 3);
    assert(pattern.steps[1].note == 96);   // saturates at C7
    adjustOctave(pattern, 1, -6);
    assert(pattern.steps[1].note == 24);   // 96-72: C1 is still a note
    adjustNote(pattern, 1, -1);
    assert(pattern.steps[1].note == -1);   // below C1 is a rest, as before
}

}  // namespace

int main() {
    expectWireNotes(GroovePuterMidi::MidiDeviceProfile::GenericMidi);
    expectWireNotes(GroovePuterMidi::MidiDeviceProfile::SeqtrakNative);
    expectEditorCeiling();
    std::puts("Pattern range wire (73/76/96/127 leave as written, editor ceiling C7): PASS");
    return 0;
}
