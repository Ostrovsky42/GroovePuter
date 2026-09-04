#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
HEADER = (ROOT / "src/dsp/miniacid_engine.h").read_text(encoding="utf-8")
GENERATION = (ROOT / "src/generation/migration/quantized_generation_undo_owner_impl.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    begin = text.index(start)
    finish = text.index(end, begin + len(start))
    return text[begin:finish]


# Task 7 lifecycle closure: source-specific transitions may trigger a barrier,
# but the RuntimeSynthPlaybackState remains the only Pattern lifetime decider.
require(
    "void hardBarrierPatternPlayback_(int synthIdx);" in HEADER,
    "P2 lifecycle RED: MiniAcid has no target-scoped RuntimeSynthPlaybackState barrier",
)
require(
    "void barrierPatternRuntimeSourceTransition();" in HEADER,
    "P2 lifecycle RED: generation/source owner has no narrow Pattern source-transition seam",
)

barrier_one = between(
    ENGINE,
    "void MiniAcid::hardBarrierPatternPlayback_(int synthIdx)",
    "void MiniAcid::hardBarrierPatternPlayback_()",
)
require(
    "patternPlaybackState_[synthIdx].hardBarrier()" in barrier_one,
    "P2 lifecycle RED: target barrier bypasses RuntimeSynthPlaybackState",
)
require(
    "consumePatternPlaybackActions_(synthIdx" in barrier_one,
    "P2 lifecycle RED: target barrier bypasses common backend translation",
)
for forbidden in (
    "publishPatternNoteOff_",
    "publishPatternAllNotesOff_",
    "synthVoices_[synthIdx]->release()",
    "patternOwnedMask_",
):
    require(forbidden not in barrier_one,
            f"P2 lifecycle RED: target barrier retained parallel lifetime/backend logic: {forbidden}")

barrier_all = between(
    ENGINE,
    "void MiniAcid::hardBarrierPatternPlayback_()",
    "void MiniAcid::cleanupLiveNotesForTransportBarrier_(",
)
require(
    "hardBarrierPatternPlayback_(synth)" in barrier_all,
    "P2 lifecycle RED: all-target barrier does not delegate to target barrier",
)

source_seam = between(
    ENGINE,
    "void MiniAcid::barrierPatternRuntimeSourceTransition()",
    "void MiniAcid::",
)
require(
    "hardBarrierPatternPlayback_();" in source_seam,
    "P2 lifecycle RED: source-transition seam does not delegate to runtime owner",
)

# MUTE must release exactly the selected Pattern runtime target through the owner.
toggle_mute = between(ENGINE, "void MiniAcid::toggleMute303", "void MiniAcid::setMute303")
set_mute = between(ENGINE, "void MiniAcid::setMute303", "void MiniAcid::toggleMuteKick")
for name, body in (("toggleMute303", toggle_mute), ("setMute303", set_mute)):
    require("hardBarrierPatternPlayback_(idx)" in body,
            f"P2 lifecycle RED: {name} does not use target runtime hard barrier")
    require("publishPatternNoteOff_(idx)" not in body,
            f"P2 lifecycle RED: {name} still makes a direct Pattern NoteOff decision")

# Pattern identity/replacement transitions must use the same target barrier.
source_methods = (
    ("set303PatternIndex", "void MiniAcid::set303PatternIndex", "void MiniAcid::shift303PatternIndex"),
    ("shift303PatternIndex", "void MiniAcid::shift303PatternIndex", "void MiniAcid::set303BankIndex"),
    ("set303BankIndex", "void MiniAcid::set303BankIndex", "void MiniAcid::setDrumPatternIndex"),
    ("randomize303Pattern", "void MiniAcid::randomize303Pattern", "void MiniAcid::randomizeDrumPattern"),
)
for name, start, end in source_methods:
    body = between(ENGINE, start, end)
    require("hardBarrierPatternPlayback_(idx)" in body,
            f"P2 lifecycle RED: {name} does not terminate old Pattern lifetime through runtime owner")
    require("publishPatternNoteOff_(idx)" not in body,
            f"P2 lifecycle RED: {name} retains direct Pattern NoteOff ownership")

# Swapping the physical synth is a target source transition. Capture authority
# before RuntimeSynthPlaybackState mutates the Pattern mask so legacy/live cleanup
# cannot double-release a Pattern-owned physical voice.
engine_swap = between(ENGINE, "void MiniAcid::setSynthEngine", "std::vector<std::string> MiniAcid::getAvailableDrumEngines")
require("patternAuthorityAtEntry" in engine_swap,
        "P2 lifecycle RED: synth-engine swap lacks pre-barrier Pattern authority snapshot")
require("hardBarrierPatternPlayback_(idx)" in engine_swap,
        "P2 lifecycle RED: synth-engine swap bypasses target runtime barrier")
require("publishPatternNoteOff_(idx)" not in engine_swap,
        "P2 lifecycle RED: synth-engine swap still owns Pattern NoteOff directly")
require(engine_swap.index("patternAuthorityAtEntry") < engine_swap.index("hardBarrierPatternPlayback_(idx)"),
        "P2 lifecycle RED: synth-engine swap captures authority after barrier mutation")

# Song physical source transitions terminate active Pattern lifetimes through the
# same executor, never via global Pattern panic.
set_song_mode = between(ENGINE, "void MiniAcid::setSongMode", "void MiniAcid::toggleSongMode")
require("hardBarrierPatternPlayback_();" in set_song_mode,
        "P2 lifecycle RED: SONG mode source transition lacks common hard barrier")
require("publishPatternAllNotesOff_()" not in set_song_mode,
        "P2 lifecycle RED: SONG mode source transition still uses global Pattern panic")

song_selection = between(ENGINE, "void MiniAcid::applySongPositionSelection", "void MiniAcid::")
require("hardBarrierPatternPlayback_();" in song_selection,
        "P2 lifecycle RED: SONG position source transition lacks common hard barrier")
require("publishPatternAllNotesOff_()" not in song_selection,
        "P2 lifecycle RED: SONG position source transition still uses global Pattern panic")

# Page identity is part of the prepared source identity. Publishing a different
# page while a Pattern lifetime is active must first end that lifetime once.
require(
    "void setCurrentPage(int8_t page);" in HEADER,
    "P2 lifecycle RED: setCurrentPage remains an inline identity store with no barrier seam",
)
page = between(ENGINE, "void MiniAcid::setCurrentPage(int8_t page)", "void MiniAcid::")
require("hardBarrierPatternPlayback_();" in page,
        "P2 lifecycle RED: page identity transition lacks common runtime barrier")
require(page.index("hardBarrierPatternPlayback_();") < page.index("currentPage_.store"),
        "P2 lifecycle RED: page identity is published before old lifetime is ended")

# Pending-generation old-audible overlay removal/activation is another Pattern
# source transition. The generation owner may request the transition but may not
# emit backend lifetime operations itself.
for name, start, end in (
    ("cancel-for-revision", "inline bool cancelPendingGenerationActivationForRevision", "inline bool cancelPendingGenerationActivation("),
    ("cancel", "inline bool cancelPendingGenerationActivation(MiniAcid& engine)", "inline int armCompactSynthActivation"),
):
    body = between(GENERATION, start, end)
    require("engine.barrierPatternRuntimeSourceTransition();" in body,
            f"P2 lifecycle RED: generation {name} removes old-audible source without runtime barrier")

commit = GENERATION[GENERATION.index("inline bool commitQuantizedGenerationAtBarStart(SceneManager& scenes)"):]
require("barrierPatternRuntimeSourceTransition();" in commit,
        "P2 lifecycle RED: BAR_START generation activation lacks Pattern runtime source barrier")

print("P2 lifecycle single-owner barrier contract: OK")
