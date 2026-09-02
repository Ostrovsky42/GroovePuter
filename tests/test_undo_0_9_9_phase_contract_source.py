#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(ok: bool, message: str) -> None:
    if not ok:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    begin = text.find(start)
    require(begin >= 0, f"missing source anchor: {start}")
    finish = text.find(end, begin + len(start))
    require(finish >= 0, f"missing source end anchor: {end}")
    return text[begin:finish]


generation = read("src/generation/migration/quantized_generation_undo_owner_impl.h")
live_song = read("src/generation/migration/live_song_arrangement_activation.h")
song = read("src/ui/pages/song_page_r4_owner.inc")
receipts = read("src/state/undo_receipts.h")
generated_phrase = read("src/dsp/generated_phrase_song.h")

# Redo after Undo-before-boundary is not an inverse operation. During PLAY it
# must be refused before the retained pair is exchanged; no re-arm is allowed.
# Because refusal returns before togglePrepared(), it also leaves the direction
# bit unchanged: second/third/etc Ctrl+Z during PLAY produce the same refusal.
gen_toggle = between(
    generation,
    "inline GroovePuterUndo::UndoResult toggleLastQuantizedGeneration",
    "inline std::size_t quantizedGenerationUndoPayloadSize")
require("const bool redo = owner.nextIsRedo();" in gen_toggle,
        "Generation phase gate does not read retained history direction")
require("if (redo || !matchingPending)" in gen_toggle,
        "Generation PLAY Redo/late Undo gate missing")
require(gen_toggle.index("if (redo || !matchingPending)") <
        gen_toggle.index("togglePrepared<GenerationUndoPayload>"),
        "Generation refusal occurs after history exchange")
require("ContextUnavailable" in gen_toggle,
        "Generation PLAY refusal has no explicit status")
require("setNextIsRedo" not in gen_toggle,
        "Generation handler must not mutate retained direction outside UndoOwner toggle")
for forbidden in ("armActivationSlot", "completeArmedActivation", "publishWriteSlot"):
    require(forbidden not in gen_toggle,
            f"Generation history path re-arms activation via {forbidden}")

song_toggle = between(song, "bool SongPage::undoPreparedSongState()", "bool SongPage::handleEvent")
require("const bool redo = owner.nextIsRedo();" in song_toggle,
        "D3 Song phase gate does not read retained history direction")
require("mini_acid_.isPlaying() && affectsAudible && (redo || !pending)" in song_toggle,
        "D3 Song phase gate missing")
require("REDO: STOP OR WAIT" in song_toggle,
        "D3 Song PLAY Redo refusal is silent")
require(song_toggle.index("(redo || !pending)") <
        song_toggle.index("togglePrepared<SongUndoPayload>"),
        "D3 Song refusal occurs after history exchange")
require("setNextIsRedo" not in song_toggle,
        "D3 Song handler must not mutate retained direction outside UndoOwner toggle")
for forbidden in ("prepareSongMutationActivation", "completeSongMutationActivation",
                  "requestSongPlaybackSwitch"):
    require(forbidden not in song_toggle,
            f"D3 Song history path re-arms activation via {forbidden}")

# Matching means exact committed revision. Undo Y must never cancel pending X.
gen_cancel = between(
    generation,
    "inline bool cancelPendingGenerationActivationForRevision",
    "inline bool cancelPendingGenerationActivation(MiniAcid& engine)")
require("pending.committedRevision != committedRevision" in gen_cancel,
        "Generation pending cancellation is not revision-exact")

song_cancel = between(
    live_song,
    "inline bool cancelPendingSongActivationForRevision",
    "inline bool songUndoWouldAffectAudibleTruth")
require("pending.committedRevision != revision" in song_cancel,
        "D3 Song pending cancellation is not revision-exact")
require("const uint32_t receiptRevision = owner.committedRevision();" in song_toggle,
        "D3 Song does not cancel against retained receipt revision")
require("const uint32_t committedRevision = owner.committedRevision();" in gen_toggle,
        "Generation does not cancel against retained receipt revision")

# Every current retained payload must fail compilation if it outgrows 1536 B.
for payload in ("SynthPatternUndoPayload", "SongUndoPayload",
                "PhraseUndoPayload", "DrumPatternUndoPayload"):
    require(f"sizeof({payload}) <= kUndoPayloadBytes" in receipts,
            f"missing capacity guard for {payload}")
require("sizeof(GenerationUndoPayload) <= GroovePuterUndo::kUndoPayloadBytes" in generation,
        "missing Generation payload capacity guard")
phrase_guard = between(
    generated_phrase,
    "static_assert(std::is_trivially_copyable<GeneratedPhraseUndoPayload>::value",
    "static_assert(std::is_trivially_copyable<PreparedPhraseArrangement>::value")
require("GroovePuterUndo::kUndoPayloadBytes" in phrase_guard,
        "missing GeneratedPhrase payload capacity guard")

print("0.9.9 phase-dependent Undo/Redo contract: PASS")
