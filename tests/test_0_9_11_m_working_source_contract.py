#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin)
    return text[begin:finish]


# Song transport may alter what is displayed/sounded, but the existing cursor
# synchronization hook must not itself mutate material/source ownership.
sync = between(
    header,
    "  void syncSongPatternContext() {",
    "\n  }\n\n private:",
)
for token in (
    "current303PatternIndex",
    "current303BankIndex",
    "pattern_row_cursor_",
    "bank_index_",
):
    require(token in sync, f"Song cursor synchronization lost expected presentation token: {token}")

for forbidden in (
    "setSequencedSource",
    "makePhrase",
    "currentPhraseBuffer",
    "publishActiveMaterial",
    "stagePendingMaterial",
    "sceneManager().edit",
    "workingMaterial",
):
    require(forbidden not in sync,
            f"STOP: Song playback cursor sync rebinds material ownership via {forbidden}")

print("M-WORKING GATE [SONG_RETARGET]: PLAYBACK RETARGET DOES NOT REBIND WORKING OWNER")

# Playback-only source switching currently changes the published kind and applies
# the lifetime barrier, but does not rewrite the retained Melody payload. This is
# the compatibility fact the future single-storage implementation must preserve:
# source selection alone is not an edit-target rebind.
source_switch = between(
    engine,
    "void MiniAcid::setSequencedSource(int voiceIndex, SequencedSource source) {",
    "\n}\n\nMiniAcid::SequencedSource MiniAcid::currentSequencedSource",
)
require("hardBarrierPatternPlayback_" in source_switch,
        "source switch lost the existing playback lifetime barrier")
require("activeMaterial_[voice].kind = nextKind" in source_switch,
        "source switch no longer publishes the authoritative source kind")
for forbidden in (
    "currentPhrase_[",
    "projectPatternToRuntimeEvents",
    "editSynthPattern",
    "restoreSynthPatternUndo",
):
    require(forbidden not in source_switch,
            f"STOP: playback-only source switch rewrites material through {forbidden}")

print("M-WORKING GATE [SOURCE_SWITCH]: playback source change does not itself overwrite retained material")
