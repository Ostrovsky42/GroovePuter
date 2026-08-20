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
    song = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
    phrase = (ROOT / "src/ui/pages/phrase_page.cpp").read_text(encoding="utf-8")
    sound = (ROOT / "src/ui/pages/tb303_params_page.cpp").read_text(encoding="utf-8")
    help_content = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")
    keys = (ROOT / "src/ui/docs/keys.md").read_text(encoding="utf-8")
    owner = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")

    require("event.ctrl" in ux and "GROOVEPUTER_U" in ux and "key == 21" in ux,
            "R6 global Ctrl+U recognition is incomplete")
    require("event.alt || event.meta || event.shift" in ux,
            "Undo shortcut must reject modified Ctrl+U variants")
    require("GROOVEPUTER_APP_EVENT_UNDO" in ux and
            "GROOVEPUTER_APPLICATION_EVENT" in ux,
            "Ctrl+U must reuse the existing application Undo event")
    for forbidden in ("UndoOwner", "SceneManager", "AudioGuard", "std::vector", "new "):
        require(forbidden not in ux,
                f"stateless Undo UX helper acquired ownership/state: {forbidden}")

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
            "central fallback must distinguish EMPTY from retained other-page receipt")
    require("undoPrepared" not in display[fallback:app_fallback] and
            "restore" not in display[fallback:app_fallback].lower(),
            "MiniAcidDisplay must not become a second Undo restore owner")

    require('"UNDO: PATTERN"' in pattern and '"UNDO: EMPTY"' in pattern and
            '"UNDO: RETURN PAGE"' in pattern,
            "Pattern Undo UX is not normalized")
    require('"UNDO: SONG"' in song and '"UNDO: RETURN PAGE"' in song,
            "Song Undo UX is not normalized")
    require('"UNDO: PHRASE"' in phrase and '"UNDO: SONG"' in phrase and
            phrase.count('"UNDO: RETURN PAGE"') >= 2,
            "Phrase Undo UX is not normalized")

    # Ctrl+Z remains a TB303 parameter reset. R6 must not steal it globally.
    require("if (lowerKey == 'z')" in sound and
            "set303Parameter(TB303ParamId::Cutoff, 800.0f" in sound,
            "Synth Sound Ctrl+Z reset compatibility disappeared")
    require("GROOVEPUTER_Z" not in ux and "'z'" not in ux and '"z"' not in ux,
            "R6 must not bind global Undo to Z")

    require('"Ctrl+U      Undo last edit"' in help_content,
            "on-device global help must expose Ctrl+U Undo")
    require("`Ctrl+U` | Undo last retained Pattern / Song / Phrase edit" in keys,
            "canonical external key map must expose the same Undo chord")

    require("class UndoOwner" in owner and "kUndoPayloadBytes = 1536" in owner,
            "R6 must retain the accepted bounded owner")
    require("static UndoOwner owner" in owner,
            "R6 must keep one authoritative retained owner")

    print("0.9.8 R6 Undo UX source regressions: OK")


if __name__ == "__main__":
    main()
