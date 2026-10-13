#include <cassert>
#include <cstdint>
#include <vector>

#include "src/input/performance_keyboard.h"

using namespace GroovePuterPerformance;

namespace {
class CaptureSink final : public IMusicalEventSink {
public:
    void handleMusicalEvent(const MusicalEvent& event) override { events.push_back(event); }
    std::vector<MusicalEvent> events;
};
}

int main() {
    MusicalEventRouter router;
    PerformanceKeyboard keyboard(router);
    uint8_t notes[PerformanceKeyboard::kMaxPolyChordNotes]{};
    uint8_t velocities[PerformanceKeyboard::kMaxPolyChordNotes]{};

    PerformanceContext context{};
    context.rootMidi = 60;
    context.hasHarmony = true;
    context.harmony = GroovePuterRhythm::HarmonicEvent{
        0, GroovePuterRhythm::ChordQuality::Major7, 0};
    keyboard.setPerformanceContext(context);
    assert(keyboard.setArpPitchSource(ArpPitchSource::Harmony));
    keyboard.setArpOctaves(1);
    std::size_t count = keyboard.collectArpeggioPool(notes, velocities, 16);
    assert(count == 4);
    assert(notes[0] == 60 && notes[1] == 64 && notes[2] == 67 && notes[3] == 71);

    context.harmony.quality = GroovePuterRhythm::ChordQuality::Dominant7;
    context.rootMidi = 60;
    context.scalePitchClasses = 0x0ab5u; // C major; Bb is not allowed here.
    context.allowChromatic = false;
    keyboard.setPerformanceContext(context);
    count = keyboard.collectArpeggioPool(notes, velocities, 16);
    assert(count == 3 && notes[0] == 60 && notes[1] == 64 && notes[2] == 67);
    context.allowChromatic = true;
    keyboard.setPerformanceContext(context);
    count = keyboard.collectArpeggioPool(notes, velocities, 16);
    assert(count == 4 && notes[3] == 70);

    assert(keyboard.setHarmonyChangePolicy(HarmonyChangePolicy::Hold));
    keyboard.setArpeggiatorEnabled(true);
    keyboard.service(1000); // establish a live arp cycle before HOLD is requested
    context.rootMidi = 62;
    context.sourceOrdinal = 1;
    keyboard.setPerformanceContext(context);
    count = keyboard.collectArpeggioPool(notes, velocities, 16);
    assert(count == 4 && notes[0] == 60 && notes[1] == 64); // held old chord snapshot

    assert(keyboard.setHarmonyChangePolicy(HarmonyChangePolicy::Follow));
    count = keyboard.collectArpeggioPool(notes, velocities, 16);
    assert(count == 4 && notes[0] == 62 && notes[1] == 66);

    assert(keyboard.setArpPitchSource(ArpPitchSource::Held));
    assert(keyboard.externalNoteOn(60, 90));
    assert(keyboard.externalNoteOn(64, 100));
    count = keyboard.collectArpeggioPool(notes, velocities, 16);
    assert(count == 2 && notes[0] == 60 && notes[1] == 64);

    assert(keyboard.captureChordMemory());
    assert(keyboard.setArpPitchSource(ArpPitchSource::Memory));
    count = keyboard.collectArpeggioPool(notes, velocities, 16);
    assert(count == 2 && notes[0] == 60 && notes[1] == 64);

    keyboard.releaseAllExternalNotes();
    keyboard.setPerformanceRole(PerformanceRole::Lead);
    context.scalePitchClasses = 0x0ab5u;
    context.minNote = 60;
    context.maxNote = 72;
    keyboard.setPerformanceContext(context);
    assert(keyboard.setArpPitchSource(ArpPitchSource::Role));
    count = keyboard.collectArpeggioPool(notes, velocities, 16);
    assert(count == 8);
    assert(notes[0] == 60 && notes[1] == 62 && notes[2] == 64 && notes[3] == 65 &&
           notes[4] == 67 && notes[5] == 69 && notes[6] == 71 && notes[7] == 72);

    PerformanceKeyboard bassRole(router);
    bassRole.setPerformanceRole(PerformanceRole::Bass);
    context.rootMidi = 36;
    context.minNote = 24;
    context.maxNote = 48;
    context.harmony.quality = GroovePuterRhythm::ChordQuality::Major7;
    context.allowChromatic = true;
    bassRole.setPerformanceContext(context);
    assert(bassRole.setArpPitchSource(ArpPitchSource::Role));
    count = bassRole.collectArpeggioPool(notes, velocities, 16);
    assert(count == 4 && notes[0] == 36 && notes[1] == 40 &&
           notes[2] == 43 && notes[3] == 47);

    context.harmony.quality = GroovePuterRhythm::ChordQuality::Dominant7;
    context.scalePitchClasses = 0x0ab5u;
    context.allowChromatic = false;
    bassRole.setPerformanceContext(context);
    count = bassRole.collectArpeggioPool(notes, velocities, 16);
    assert(count == 3 && notes[0] == 36 && notes[1] == 40 && notes[2] == 43);

    PerformanceKeyboard contextExpiry(router);
    contextExpiry.setPerformanceRole(PerformanceRole::Lead);
    assert(contextExpiry.setArpPitchSource(ArpPitchSource::Harmony));
    contextExpiry.setArpeggiatorEnabled(true);
    assert(contextExpiry.setHarmonyChangePolicy(HarmonyChangePolicy::Hold));
    PerformanceContext oldHarmony{};
    oldHarmony.hasHarmony = true;
    oldHarmony.rootMidi = 60;
    oldHarmony.harmony.quality = GroovePuterRhythm::ChordQuality::Major7;
    contextExpiry.setPerformanceContext(oldHarmony);
    PerformanceContext nextHarmony = oldHarmony;
    nextHarmony.rootMidi = 62;
    nextHarmony.sourceOrdinal = 1;
    contextExpiry.setPerformanceContext(nextHarmony);
    assert(contextExpiry.collectArpeggioPool(notes, velocities, 16) == 4);
    contextExpiry.clearPerformanceHarmonyContext();
    assert(contextExpiry.collectArpeggioPool(notes, velocities, 16) == 0);

    // A missing harmony provider may be reported every main-loop pass. Clearing it must not
    // reset the held-note arp phase on every pass.
    CaptureSink sink;
    assert(router.addSink(sink));
    PerformanceKeyboard standalone(router);
    standalone.setArpeggiatorEnabled(true);
    assert(standalone.setArpPitchSource(ArpPitchSource::Held));
    assert(standalone.externalNoteOn(60, 90));
    assert(standalone.externalNoteOn(64, 90));
    assert(standalone.externalNoteOn(67, 90));
    sink.events.clear();
    for (uint32_t now = 1000; now <= 1500000; now += 1000) {
        standalone.clearPerformanceHarmonyContext();
        standalone.service(now);
    }
    uint8_t arpNotes[3]{};
    std::size_t arpCount = 0;
    for (const MusicalEvent& event : sink.events) {
        if (event.type == MusicalEventType::NoteOn && arpCount < 3) {
            arpNotes[arpCount++] = event.note;
        }
    }
    assert(arpCount >= 3 && arpNotes[0] == 60 && arpNotes[1] == 64 && arpNotes[2] == 67);

    CaptureSink harmonySink;
    assert(router.addSink(harmonySink));
    PerformanceKeyboard harmonyOnly(router);
    assert(harmonyOnly.setArpPitchSource(ArpPitchSource::Harmony));
    assert(harmonyOnly.setHarmonyChangePolicy(HarmonyChangePolicy::Hold));
    harmonyOnly.setPerformanceContext(oldHarmony);
    harmonyOnly.setArpeggiatorEnabled(true);
    harmonyOnly.service(1000);
    harmonyOnly.setPerformanceContext(nextHarmony);
    for (uint32_t now = 1000; now <= 2500000; now += 1000) harmonyOnly.service(now);
    bool heardOldChord = false;
    bool heardFollowedChord = false;
    for (const MusicalEvent& event : harmonySink.events) {
        if (event.type != MusicalEventType::NoteOn) continue;
        if (event.note == 60) heardOldChord = true;
        if (event.note == 62) heardFollowedChord = true;
    }
    assert(heardOldChord && heardFollowedChord); // HOLD lasts one arp cycle, then follows.
    return 0;
}
