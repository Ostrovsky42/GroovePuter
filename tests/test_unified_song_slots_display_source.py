#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE_H = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
SONG = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
NOTES = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(encoding="utf-8")
UX = (ROOT / "src/ui/material_development_ux.h").read_text(encoding="utf-8")
PROJECT = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

def require(cond: bool, msg: str) -> None:
    if not cond:
        raise AssertionError(msg)

for token in (
    "SongCellMaterialKind",
    "SongVoiceDisplayState",
    "activeMelodyForDisplay",
    "songNeedsNextBuffer",
):
    require(token in ENGINE_H, f"missing read-model contract: {token}")

for token in (
    "SongCellMaterialKind::Pattern",
    "SongCellMaterialKind::Melody",
    "SongCellMaterialKind::Unknown",
    '"PAT"',
    '"MEL"',
    '"HOLD"',
    '"WAIT"',
    '"FAIL"',
):
    require(token in SONG, f"Song display mapping missing {token}")

require("activeMelodyForDisplay(voiceIndex)" in NOTES,
        "NOTES must use the same active Melody payload as playback")
require("currentPhrasePlayTick(voiceIndex)" in NOTES,
        "Melody playhead must use phrase phase rather than Pattern step clock")
require("NO ACTIVE NOTE DATA" in NOTES,
        "Awaiting/load failure must not render stale Pattern note data")
require(NOTES.index("activeMelodyForDisplay(voiceIndex)") <
        NOTES.index("const int8_t* notes = mini_acid_.pattern303Steps(voice_index_)"),
        "Melody/awaiting branch must resolve before Pattern note source access")

require('"NEXT BUSY: SONG"' in UX,
        "Song NEXT reservation needs an explicit user-visible refusal")
for toast in (
    "PROJECT LOADED; NEXT CLEARED",
    "BLANK PROJECT; NEXT CLEARED",
    "PROJECT CLEARED; NEXT CLEARED",
):
    require(toast in PROJECT, f"project NEXT invalidation is not visible: {toast}")

load = ENGINE.index("bool MiniAcid::loadSongMelodyIntoNext_")
service = ENGINE.index("void MiniAcid::serviceSongMaterial", load)
load_block = ENGINE[load:service]
require("userOwnsNext_(idx) || goQueued_[idx]" in load_block,
        "Song must not overwrite user NEXT/GO")
require("goQueued_[idx] = false" not in load_block,
        "Song load path must not disarm user GO")
require("songVoiceState_[0] == SongVoiceState::Awaiting" in ENGINE and
        "songVoiceState_[1] == SongVoiceState::Awaiting" in ENGINE,
        "sequencer must explicitly suppress Awaiting voices")

print("USS-DISPLAY PASS: ownership/display source contract is deterministic")
