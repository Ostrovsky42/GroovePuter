#include <cassert>

#include "src/midi/smf_channel_inspector.h"

using namespace GroovePuterMidi;

int main() {
    SmfChannelInspectorBuilder builder;
    builder.reset(1, 480, 3);

    builder.observe(SmfEvent{0, 0, SmfEventKind::ProgramChange, 0, 4, 0, 0});
    builder.observe(SmfEvent{0, 1, SmfEventKind::NoteOn, 0, 60, 40, 0});
    builder.observe(SmfEvent{0, 2, SmfEventKind::NoteOn, 0, 64, 80, 0});
    builder.observe(SmfEvent{120, 3, SmfEventKind::NoteOff, 0, 60, 0, 0});
    builder.observe(SmfEvent{240, 4, SmfEventKind::NoteOff, 0, 64, 0, 0});

    builder.observe(SmfEvent{0, 5, SmfEventKind::NoteOn, 9, 36, 100, 0});
    builder.observe(SmfEvent{60, 6, SmfEventKind::NoteOff, 9, 36, 0, 0});
    builder.observe(SmfEvent{0, 7, SmfEventKind::ProgramChange, 3, 48, 0, 0});
    builder.observe(SmfEvent{0, 8, SmfEventKind::ProgramChange, 3, 49, 0, 0});
    builder.observe(SmfEvent{0, 9, SmfEventKind::NoteOff, 3, 60, 0, 0});

    const SmfChannelInspectorSnapshot& snapshot = builder.snapshot();
    assert(snapshot.format == 1);
    assert(snapshot.division == 480);
    assert(snapshot.trackCount == 3);
    assert(snapshot.usedChannelCount() == 3);
    assert(snapshot.usedChannelMask == ((1u << 0) | (1u << 3) | (1u << 9)));

    const SmfChannelInfo& synth = snapshot.channels[0];
    assert(synth.used);
    assert(!synth.likelyDrums);
    assert(synth.noteCount == 2);
    assert(synth.minNote == 60);
    assert(synth.maxNote == 64);
    assert(synth.averageVelocity() == 60);
    assert(synth.maxPolyphony == 2);
    assert(synth.hasProgramChange);
    assert(synth.firstProgram == 4);

    const SmfChannelInfo& drums = snapshot.channels[9];
    assert(drums.used);
    assert(drums.likelyDrums);
    assert(drums.noteCount == 1);
    assert(drums.averageVelocity() == 100);

    const SmfChannelInfo& programOnly = snapshot.channels[3];
    assert(programOnly.used);
    assert(!programOnly.hasNotes());
    assert(programOnly.hasProgramChange);
    assert(programOnly.firstProgram == 48);
    assert(programOnly.maxPolyphony == 0);
    return 0;
}
