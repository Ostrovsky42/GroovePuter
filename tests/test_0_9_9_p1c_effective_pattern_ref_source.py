#!/usr/bin/env python3

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
CPP = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def function_body(source: str, signature: str) -> str:
    start = source.find(signature)
    require(start >= 0, f"missing function: {signature}")
    brace = source.find("{", start)
    require(brace >= 0, f"missing opening brace: {signature}")
    depth = 0
    for index in range(brace, len(source)):
        char = source[index]
        if char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[brace + 1 : index]
    raise AssertionError(f"unterminated function: {signature}")


def has_call(source: str, callee: str, first_argument: str) -> bool:
    pattern = (
        rf"\b{re.escape(callee)}\s*\(\s*"
        rf"{re.escape(first_argument)}(?:\s*,|\s*\))"
    )
    return re.search(pattern, source, flags=re.DOTALL) is not None


require(
    "int effectivePatternRef(int position, SongTrack track) const;" in HEADER,
    "MiniAcid must declare one private effective Pattern ref resolver",
)
require(
    CPP.count("int MiniAcid::effectivePatternRef(") == 1,
    "MiniAcid must define exactly one effective Pattern ref boundary",
)

resolver = function_body(CPP, "int MiniAcid::effectivePatternRef(")
require(
    "sceneManager_.songPatternAtSlot(songPlaybackSlot_, pos, track)" in resolver,
    "resolver must start from the canonical Song ref for playback slot/row/track",
)
require(
    "overrideRef.songSlot == songPlaybackSlot_" in resolver
    and "overrideRef.position == pos" in resolver
    and "overrideRef.track == track" in resolver,
    "test seam must key the effective view by playback slot + row + SongTrack",
)
for decomposition in (
    "songPatternPage(",
    "songPatternBank(",
    "songPatternIndexInBank(",
):
    require(
        decomposition not in resolver,
        "effective ref must be resolved before PAGE/BANK/SLOT decomposition",
    )
for forbidden in (
    "PatternLeaseOwner",
    "patternLeaseOwner",
    ".acquire(",
    ".discard(",
    "preparePersistentTransfer",
    "completePersistentTransfer",
    "findReusableLocalSlot",
):
    require(
        forbidden not in resolver,
        f"resolver must not own allocation/lifecycle: found {forbidden}",
    )

song_index = function_body(CPP, "int MiniAcid::songPatternIndexForTrack(")
require(
    "effectivePatternRef(pos, track)" in song_index,
    "songPatternIndexForTrack must consume the canonical effective ref",
)
require(
    song_index.find("effectivePatternRef(pos, track)")
    < song_index.find("songPatternIndexInBank(combined)"),
    "songPatternIndexForTrack must resolve before local-index decomposition",
)

selection = function_body(CPP, "void MiniAcid::applySongPositionSelection(")
for track in ("SongTrack::SynthA", "SongTrack::SynthB", "SongTrack::Drums"):
    require(
        f"effectivePatternRef(pos, {track})" in selection,
        f"applySongPositionSelection must resolve {track} through the same boundary",
    )
require(
    selection.count("sceneManager_.songPatternAtSlot(") == 1
    and "SongTrack::Voice" in selection,
    "only the non-Pattern Voice lane may keep a direct canonical Song lookup",
)
for variable, track in (
    ("patA", "SongTrack::SynthA"),
    ("patB", "SongTrack::SynthB"),
    ("patD", "SongTrack::Drums"),
):
    require(
        selection.find(f"effectivePatternRef(pos, {track})")
        < selection.find(f"songPatternBank({variable})"),
        f"{variable} must be resolved through {track} before bank decomposition",
    )

display_synth = function_body(CPP, "int16_t MiniAcid::display303PatternIndex(")
require(
    has_call(display_synth, "effectivePatternRef", "pos"),
    "Song synth display ref must use the same effective boundary",
)
display_drums = function_body(CPP, "int16_t MiniAcid::displayDrumPatternIndex(")
require(
    has_call(display_drums, "effectivePatternRef", "pos")
    and "SongTrack::Drums" in display_drums,
    "Song drum display ref must use the same effective boundary",
)

# Canonical model access and rehearsal control semantics intentionally remain
# outside the effective view.
canonical_getter = function_body(CPP, "int16_t MiniAcid::songPatternAtSlot(")
require(
    "sceneManager_.songPatternAtSlot(slot, position, track)" in canonical_getter,
    "public Song ref getter must remain canonical",
)
advance = function_body(CPP, "void MiniAcid::advanceSongPlayhead(")
require(
    "sceneManager_.songPatternAtSlot(songPlaybackSlot_, nextPos" in advance
    and "== -2" in advance,
    "rehearsal pause sentinel must remain canonical Song control state",
)

require(
    '#include "../phrase/pattern_lease_owner.h"' not in CPP
    and "PatternLeaseOwner" not in CPP,
    "P1c MiniAcid production path must not introduce a second lease/allocator owner",
)
require(
    "GROOVEPUTER_P1C_TEST_SEAM" in CPP,
    "runtime override seam must remain explicitly test-only at this checkpoint",
)

print("0.9.9-P1c effective Pattern ref source contracts passed")
