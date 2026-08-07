#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    edges = (ROOT / "src/input/cardputer_input_edges.h").read_text(encoding="utf-8")
    song = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
    hub = (ROOT / "src/ui/pages/sequencer_hub_page.cpp").read_text(encoding="utf-8")
    midi_hub = (ROOT / "src/ui/pages/sequencer_hub_page_midi.cpp").read_text(encoding="utf-8")
    queue = (ROOT / "src/midi/scheduled_smf_midi_event_queue.h").read_text(encoding="utf-8")
    scenes_h = (ROOT / "scenes.h").read_text(encoding="utf-8")
    scenes_cpp = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    drum = (ROOT / "src/ui/components/drum_sequencer_grid.cpp").read_text(encoding="utf-8")

    require("uint32_t dispatchedLetterMask = 0;" in sketch,
            "Cardputer input must deduplicate physical HID and word letters")
    require("!ks.ctrl && !ks.alt && !ks.fn" in sketch
            and "dispatchedLetterMask |= GroovePuterInput::letterDispatchMask(evt.key);" in sketch,
            "plain physical HID letters, including W, must reach the UI")
    require("wordLetterAlreadyDispatched" in edges and "wordLetterAlreadyDispatched" in sketch,
            "word copies of physical HID letters must not double-dispatch")

    require("int SongPage::maxPatternTrackColumn() const" in song,
            "Song needs a data-only lane boundary separate from the mode button")
    require(song.count("int maxCol = maxPatternTrackColumn();") >= 6,
            "Song area operations must clamp to real pattern lanes")
    require("g_song_pattern_clipboard.pattern_index = mini_acid_.songPatternAt(row, track);" in song,
            "single-cell copy must retain the exact composite pattern reference")
    require("mini_acid_.setSongPattern(row, track, patternIndex);" in song,
            "single-cell paste must restore the exact composite pattern reference")

    require("sceneManager_.setTrackVolume((int)id, volume);" in engine,
            "internal Hub volume must mutate scene-owned track volume state")
    require("trackVolumes[(int)VoiceId::Count]" in scenes_h,
            "scene schema must retain all internal Hub track levels")
    require('lastKey_ == "trackVolumes"' in scenes_cpp
            and 'state["trackVolumes"]' in scenes_cpp
            and 'obj["trackVolumes"]' in scenes_cpp,
            "scene codecs must read and write internal Hub track levels")
    require("e.alt || e.ctrl || e.meta" in hub,
            "internal Hub must accept Cardputer Fn+Left/Right volume input")
    require(hub.count("int volPct = (int)(vol * 100.0f + 0.5f);") == 4,
            "all internal Hub styles must display real 0..120 percent volume")

    require('#include "src/midi/smf_track_level.h"' in midi_hub,
            "MIDI Hub must use physical-track level state")
    require("event.meta && !event.alt && !event.ctrl" in midi_hub
            and "smfTrackLevelState().adjustLevel" in midi_hub,
            "MIDI Hub Fn+Left/Right must adjust only the selected physical track")
    require("applySmfTrackLevelVelocity" in queue
            and "smfTrackLevelState().levelFor" in queue,
            "MIDI level scaling must happen in the queue consumer before USB ownership")

    require('{"KIK", "SNR", "HH1", "HH2", "PR1", "PR2", "RIM", "CLP"}' in drum,
            "drum lanes must use equal three-glyph labels")
    require("((step + 1) % 10)" in drum,
            "drum step headers must stay one glyph wide after step 9")

    print("Hub/Song/drum UI source regressions: OK")


if __name__ == "__main__":
    main()
