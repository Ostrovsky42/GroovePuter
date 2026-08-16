#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SONG = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
CORE = (ROOT / "src/ui/ui_core.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    require("GROOVEPUTER_APP_EVENT_UNDO" in CORE,
            "shared UI event contract must retain Undo")
    require("bool key_z" in SONG and "GROOVEPUTER_Z" in SONG,
            "Song must normalize the Z shortcut across key/scancode paths")
    require("(ui_event.ctrl || ui_event.meta) && !ui_event.alt && key_z" in SONG,
            "Song Undo must be reachable via Ctrl/Meta+Z without stealing Alt+Z")
    require("app_evt.app_event_type = GROOVEPUTER_APP_EVENT_UNDO" in SONG,
            "Song shortcut must route through the existing application Undo event")
    require("case GROOVEPUTER_APP_EVENT_UNDO" in SONG,
            "existing Song Undo handler must remain reachable")
    require("withAudioGuard([&]()" in SONG and "g_undo_history.clear();" in SONG,
            "Undo restore must remain audio-guarded and one-step")
    require('showToast("Nothing to undo"' in SONG,
            "empty Undo must be a safe handled no-op with feedback")
    require('showToast("Undo: restored"' in SONG,
            "successful Undo must provide explicit feedback")
    require("GROOVEPUTER_APP_EVENT_REDO" not in CORE and
            "GROOVEPUTER_APP_EVENT_REDO" not in SONG,
            "0.9.8-B1 must not introduce Redo")
    print("Song Undo source regressions: OK")


if __name__ == "__main__":
    main()
