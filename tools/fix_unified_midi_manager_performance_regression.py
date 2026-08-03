#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
path = ROOT / "tests/test_performance_source_regressions.py"
text = path.read_text(encoding="utf-8")

source_anchor = '''    player_page = (ROOT / "src/ui/pages/smf_player_page.cpp").read_text(
        encoding="utf-8"
    )
'''
source_replacement = source_anchor + '''    midi_manager = (ROOT / "src/ui/midi_file_manager.cpp").read_text(
        encoding="utf-8"
    )
'''
if text.count(source_anchor) != 1:
    raise RuntimeError("SMF player source anchor missing")
text = text.replace(source_anchor, source_replacement, 1)

old_load = '''    require("requestLoad(path.c_str())" in player_page and
            "requestLoadAndPlay" not in player_page,
            "Enter must load an SMF without creating hidden playback intent")
'''
new_load = '''    load_path = player_page[
        player_page.index("bool SmfPlayerPage::loadMidiPath(const char* path)"):
        player_page.index("bool SmfPlayerPage::togglePlayerTransport()")]
    require("player_->requestLoad(path)" in load_path and
            "requestLoadAndPlay" not in player_page and
            "EventResult::FileActivated" in player_page and
            "return loadMidiPath(activatedPath);" in player_page,
            "Enter must load the selected shared-browser path without hidden playback intent")
'''
if text.count(old_load) != 1:
    raise RuntimeError("legacy SMF requestLoad ownership assertion missing")
text = text.replace(old_load, new_load, 1)

old_library = '''    require('"MIDI LIBRARY  %.24s"' in player_page and
            "currentPath_.c_str()" in player_page and
            "requestLoad" in player_page,
            "MIDI Player page must expose selectable SD playback and its path")
'''
new_library = '''    require('"MIDI FILES / %s"' in midi_manager and
            "currentPath_" in midi_manager and
            "EventResult::FileActivated" in midi_manager and
            "midiFileManager().draw" in player_page,
            "MIDI Player page must expose the shared selectable SD library and path")
'''
if text.count(old_library) != 1:
    raise RuntimeError("legacy SMF library rendering assertion missing")
text = text.replace(old_library, new_library, 1)

path.write_text(text, encoding="utf-8")
