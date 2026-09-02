#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    ux = (ROOT / "src/ui/undo_ux.h").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    pattern = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(encoding="utf-8")
    synth_parent = (ROOT / "src/ui/pages/synth_sequencer_page.cpp").read_text(encoding="utf-8")
    song = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
    phrase = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
    sound = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
    help_content = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")
    keys = (ROOT / "src/ui/docs/keys.md").read_text(encoding="utf-8")
    owner = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")

    # R9 keeps the R6 public chord but makes wrong-context Ctrl+Z a retained
    # no-op. Navigation/back belongs to Esc; the global display never restores
    # another page on behalf of the active page.
    require("event.ctrl" in ux and "GROOVEPUTER_Z" in ux and "key == 26" in ux,
            "current global Ctrl+Z recognition is incomplete")
    require("event.alt || event.meta || event.shift" in ux,
            "Undo shortcut must reject modified Ctrl+Z variants")
    require("GROOVEPUTER_APP_EVENT_UNDO" in ux and
            "GROOVEPUTER_APPLICATION_EVENT" in ux,
            "Ctrl+Z must reuse the existing application Undo event")
    require("GROOVEPUTER_U" not in ux and "key == 21" not in ux,
            "legacy Ctrl+U must not remain a second global Undo chord")
    require('"UNDO: NOT HERE"' in ux and '"REDO: NOT HERE"' in ux and
            '"UNDO: EMPTY"' in ux and "RETURN PAGE" not in ux,
            "R9 fallback must retain history without return-page navigation")
    for forbidden in ("SceneManager", "AudioGuard", "std::vector", "new "):
        require(forbidden not in ux,
                f"Undo UX helper acquired restore/allocation dependency: {forbidden}")

    handle_start = display.index("bool MiniAcidDisplay::handleEvent(UIEvent event)")
    promotion = display.index("GroovePuterUndoUx::promoteUndoShortcut(event);", handle_start)
    page_dispatch = display.index("IPage* currentPage = getPage_(page_index_);", promotion)
    fallback = display.index("GroovePuterUndoUx::isUndoEvent(event)", page_dispatch)
    app_fallback = display.index(
        "if (event.event_type == GROOVEPUTER_APPLICATION_EVENT)", fallback)
    require(promotion < page_dispatch < fallback < app_fallback,
            "Undo must be promoted before page dispatch and fall back only after page refusal")
    require("undoOwner().hasUndo()" in display[fallback:app_fallback] and
            "fallbackToast(hasReceipt)" in display[fallback:app_fallback],
            "central fallback must distinguish EMPTY from retained wrong-context receipt")
    require("undoPrepared" not in display[fallback:app_fallback] and
            "restore" not in display[fallback:app_fallback].lower(),
            "MiniAcidDisplay must not become a second Undo restore owner")

    # Pattern's retained R2 handler is still the bounded exchange primitive. The
    # SYNTH parent only admits Pattern Undo while NOTES is already visible and
    # does not call the Pattern child directly across tabs. The Pattern child
    # remains the authority for decoding/exchanging a retained receipt during
    # Ctrl+Z. R9's hardware follow-up separately lets the parent PREPARE a new
    # bounded Pattern receipt for STOP-state Synth G before legacy dispatch.
    require('"UNDO: PATTERN"' in pattern and '"UNDO: EMPTY"' in pattern,
            "Pattern retained Undo handler disappeared")
    require("synth_tab_ == SynthTab::Notes" in synth_parent and
            "owner.kind() == GroovePuterUndo::UndoKind::Pattern" in synth_parent and
            "MultiPage::handleEvent(ui_event)" in synth_parent and
            "pattern_page_->handleEvent(ui_event)" not in synth_parent and
            "SynthPatternUndoPayload retained" not in synth_parent and
            "SynthPatternUndoPayload before" in synth_parent and
            "captureCurrentSynthPatternUndo" in synth_parent and
            "synthPatternUndoTargetAvailable" in synth_parent,
            "Pattern Undo routing must stay local while STOP Synth G gains bounded prepare ownership")
    require('"REDO: PATTERN"' in synth_parent and '"UNDO: PATTERN"' in synth_parent,
            "Pattern one-slot feedback must distinguish Undo from Redo")
    require('"REDO: SONG"' in song and '"UNDO: SONG"' in song,
            "Song one-slot feedback must distinguish Undo from Redo")
    require('"REDO: PHRASE"' in phrase and '"UNDO: PHRASE"' in phrase and
            '"REDO: SONG"' in phrase and '"UNDO: SONG"' in phrase,
            "Phrase one-slot feedback must distinguish Undo from Redo")

    # Ctrl+Z is globally reserved. The former Cutoff reset moves to Ctrl+A;
    # X/C/V retain their existing reset functions.
    reset_start = sound.index("if (isTb303Engine() && ui_event.ctrl")
    reset_end = sound.index("if (ui_event.ctrl && !ui_event.alt && key >= '1'", reset_start)
    reset_block = sound[reset_start:reset_end]
    require("lowerKey == 'a'" in reset_block and
            "set303Parameter(TB303ParamId::Cutoff, 800.0f" in reset_block,
            "Synth Sound Cutoff reset was not preserved on Ctrl+A")
    require("lowerKey == 'z'" not in reset_block,
            "Synth Sound must not consume the global Ctrl+Z chord")
    for key, param in (("x", "Resonance"), ("c", "EnvAmount"), ("v", "EnvDecay")):
        require(f"lowerKey == '{key}'" in reset_block and f"TB303ParamId::{param}" in reset_block,
                f"Synth Sound Ctrl+{key.upper()} reset compatibility disappeared")

    require('"Ctrl+Z      Undo last edit"' in help_content,
            "on-device global help must expose Ctrl+Z Undo")
    require('"Ctrl+A/X/C/V Reset parameter"' in help_content,
            "on-device Synth help must expose relocated reset chord")
    require("`Ctrl+Z` | Undo last retained Pattern / Song / Phrase edit" in keys,
            "canonical external key map must expose the same Undo chord")
    require("`Ctrl+A/X/C/V` | Reset Cutoff / Resonance / Env Amount / Env Decay" in keys,
            "canonical external key map must expose relocated Synth reset")

    require("class UndoOwner" in owner and "kUndoPayloadBytes = 1536" in owner,
            "R9 must retain the accepted bounded owner")
    require("static UndoOwner owner" in owner,
            "R9 must keep one authoritative retained owner")
    require("ContextUnavailable" in owner,
            "wrong-context one-slot toggle must fall through without consuming history")

    print("0.9.8 R6/R8/R9 Undo UX source regressions: OK")


if __name__ == "__main__":
    main()
