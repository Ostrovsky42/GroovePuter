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
    song_owner = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
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
    require("ui_event.ctrl && ui_event.alt" in song
            and "nav == GROOVEPUTER_UP" in song
            and "nav == GROOVEPUTER_DOWN" in song
            and 'showToast("Top", 500)' in song
            and 'showToast("End", 500)' in song,
            "Song top/end must use reachable Ctrl+Alt+Up/Down on Cardputer")
    require("ui_event.key == '<'" not in song
            and "ui_event.key == '>'" not in song,
            "Song must not retain unreachable Alt punctuation top/end bindings")
    require("classifyAltVerticalRoute" in song_owner
            and "AltVerticalRoute::DelegateLegacy" in song_owner
            and "return handleEventLegacyUnowned(ui_event);" in song_owner,
            "R4 owner must delegate Ctrl+Alt vertical chords before Song mutation")
    require("!cursorOnPlayheadLabel()" not in song_owner[
                song_owner.index("const auto altVerticalRoute"):
                song_owner.index("const char lowerKey")
            ],
            "Song vertical routing must not reintroduce the old owner-only precedence test")

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

    require('{"3KIK", "4SNR", "5HH1", "6HH2", "7PR1", "8PR2", "9RIM", "0CLP"}' in drum,
            "drum lanes must show the mute digit next to the semantic label")
    require("((step + 1) % 10)" in drum,
            "drum step headers must stay one glyph wide after step 9")
    require("cb.onToggle = [this](int step, int voice)" in hub
            and "toggleDrumStep(voice, step)" in hub,
            "Hub must honor Drum grid callback order (step, voice)")
    require("toggleDrumAccentStep" not in hub
            and hub.count('UI::showToast("ACCENT: ADD HIT", 900);') >= 2
            and hub.count("setDrumAccentStep(") >= 2,
            "Hub Drum accent must be per-hit and fail closed on empty cells")
    require("value.hit = !value.hit;" in engine
            and "value.accent = false;" in engine,
            "engine Drum topology toggle must clear hidden accent")

    print("Hub/Song/drum UI source regressions: OK")


if __name__ == "__main__":
    main()
