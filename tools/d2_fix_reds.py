#!/usr/bin/env python3
from pathlib import Path

# Trigger commit: workflow now exists in the branch parent and can apply this
# exact self-cleaning patch deterministically.
ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one exact anchor, found {count}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


generated = ROOT / "src/dsp/generated_phrase_song.h"
replace_once(
    generated,
    "lease.slot, QuantizedGenerationStatus::Busy);",
    "lease.slot, GroovePuterRhythm::QuantizedGenerationStatus::Busy);",
)

ui_test = ROOT / "tests/test_phrase_ui_source_regressions.py"
replace_once(
    ui_test,
    """# Generated Phrase recovery uses plain G, current 1/2/4/8B length and the same
# explicit TO destination that W/Alt+W already expose. It stays STOP-only.
require(HEADER, \"bool generatePhraseToSong();\",
        \"Phrase page must expose the generated Phrase action\")
require(CPP, '#include \"src/dsp/generated_phrase_song.h\"',
        \"Phrase page must use the current generated-Phrase adapter\")
require(CPP, \"bool PhrasePage::generatePhraseToSong()\",
        \"generated Phrase action implementation is missing\")
require(CPP, \"const int songStart = static_cast<int>(destination_row_);\",
        \"Phrase G must start at the visible TO destination\")
require(CPP, \"GeneratedPhraseSong::generate(\",
        \"Phrase G must route through the current generated-Phrase adapter\")
require(CPP, \"mini_acid_.isPlaying()\",
        \"multi-row Phrase generation must guard moving Song ownership\")
require(CPP, '\"STOP PLAYBACK FOR PHRASE\"',
        \"PLAY rejection must be visible to the user\")
require(CPP, \"!ui_event.ctrl && !ui_event.alt && !ui_event.meta && lower == 'g'\",
        \"only plain G may own generated Phrase materialization\")
require(CPP, \"static_cast<int>(songStart) + result.bars\",
        \"successful generated Phrase must advance TO by generated length\")
if \"mini_acid_.stop()\" in CPP or \"mini_acid_.start()\" in CPP:
    raise AssertionError(\"Phrase G must not hide stop/generate/restart transport ownership\")
""",
    """# Generated Phrase recovery uses plain G, current 1/2/4/8B length and the same
# explicit TO destination that W/Alt+W already expose. 0.9.9-D2 deliberately
# extends this command to PLAY through the canonical bounded activation owner;
# the UI must not hide a stop/restart cycle or retain the old STOP-only guard.
require(HEADER, \"bool generatePhraseToSong();\",
        \"Phrase page must expose the generated Phrase action\")
require(CPP, '#include \"src/dsp/generated_phrase_song.h\"',
        \"Phrase page must use the current generated-Phrase adapter\")
require(CPP, \"bool PhrasePage::generatePhraseToSong()\",
        \"generated Phrase action implementation is missing\")
require(CPP, \"const int songStart = static_cast<int>(destination_row_);\",
        \"Phrase G must start at the visible TO destination\")
require(CPP, \"GeneratedPhraseSong::generate(\",
        \"Phrase G must route through the current generated-Phrase adapter\")
require(CPP, \"GeneratedPhraseSong::Result\",
        \"Phrase G must consume the D2 lifecycle result\")
require(CPP, \"GeneratedPhraseSong::LifecycleStatus::PendingNextBar\",
        \"PLAY Phrase G must visibly distinguish pending next-bar activation\")
if '\"STOP PLAYBACK FOR PHRASE\"' in CPP:
    raise AssertionError(\"D2 Phrase G must not retain the old PLAY rejection\")
require(CPP, \"!ui_event.ctrl && !ui_event.alt && !ui_event.meta && lower == 'g'\",
        \"only plain G may own generated Phrase materialization\")
require(CPP, \"static_cast<int>(songStart) + phraseResult.bars\",
        \"successful generated Phrase must advance TO by generated length\")
if \"mini_acid_.stop()\" in CPP or \"mini_acid_.start()\" in CPP:
    raise AssertionError(\"Phrase G must not hide stop/generate/restart transport ownership\")
""",
)

print("D2 focused red fixes applied")
