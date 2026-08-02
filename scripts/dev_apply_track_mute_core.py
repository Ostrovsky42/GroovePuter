#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def update(path: str, old: str, new: str, label: str) -> None:
    target = ROOT / path
    text = target.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one match, found {count}")
    target.write_text(text.replace(old, new, 1), encoding="utf-8")


update(
    "src/midi/smf_stream.cpp",
    '#include <limits>\n\nnamespace GroovePuterMidi {',
    '#include <limits>\n\n#include "smf_track_mute.h"\n\nnamespace GroovePuterMidi {',
    "track mute include",
)

update(
    "src/midi/smf_stream.cpp",
    '    source_ = &source;\n    index_ = index;\n    for (std::size_t i = 0; i < kSmfMaxTracks; ++i) hasNext_[i] = false;',
    '    source_ = &source;\n    index_ = index;\n    smfTrackMuteState().reset(index.trackCount);\n    for (std::size_t i = 0; i < kSmfMaxTracks; ++i) hasNext_[i] = false;',
    "track mute reset",
)

update(
    "src/midi/smf_stream.cpp",
    '''bool SmfEventStreamMerger::next(SmfStreamEvent& out) {
    const int selected = selectedTrack();
    if (selected < 0) return false;
    const std::size_t track = static_cast<std::size_t>(selected);
    out = next_[track];
    prime(track);
    return true;
}
''',
    '''bool SmfEventStreamMerger::next(SmfStreamEvent& out) {
    while (true) {
        const int selected = selectedTrack();
        if (selected < 0) return false;
        const std::size_t track = static_cast<std::size_t>(selected);
        out = next_[track];
        prime(track);

        const bool noteOn = out.event.kind == SmfEventKind::NoteOn;
        if (shouldEmitSmfTrackEvent(noteOn, out.trackIndex)) return true;
    }
}
''',
    "track mute filtering",
)

update(
    "tests/test_smf_stream.cpp",
    '#include "src/midi/smf_stream.h"\n',
    '#include "src/midi/smf_stream.h"\n#include "src/midi/smf_track_mute.h"\n',
    "track mute test include",
)

update(
    "tests/test_smf_stream.cpp",
    '    SmfEventStreamMerger stream;\n    assert(stream.open(source, indexed.index));\n',
    '    SmfEventStreamMerger stream;\n    assert(stream.open(source, indexed.index));\n'
    '    assert(smfTrackMuteState().snapshot().trackCount == 2);\n'
    '    assert(smfTrackMuteState().snapshot().mutedMask == 0);\n',
    "track mute initial state test",
)

update(
    "tests/test_smf_stream.cpp",
    '    std::vector<uint8_t> tooMany = fixture();\n',
    '''    // Muting one source track removes only its future NoteOn events.
    // NoteOff remains cleanup-critical for notes queued before the mute.
    smfTrackMuteState().selectRelative(1);
    assert(smfTrackMuteState().snapshot().selectedTrack == 1);
    assert(smfTrackMuteState().toggleSelected());
    assert(smfTrackMuteState().snapshot().selectedMuted());
    stream.reset();
    events.clear();
    while (stream.next(event)) events.push_back(event);
    assert(events.size() == 5);
    assert(events[0].event.kind == SmfEventKind::Tempo);
    assert(events[1].event.kind == SmfEventKind::TimeSignature);
    assert(events[2].event.kind == SmfEventKind::NoteOff);
    assert(events[3].event.kind == SmfEventKind::NoteOff);
    assert(events[4].event.kind == SmfEventKind::ProgramChange);
    smfTrackMuteState().clear();

    std::vector<uint8_t> tooMany = fixture();
''',
    "track mute stream behavior test",
)

print("SMF track mute core integration applied")
