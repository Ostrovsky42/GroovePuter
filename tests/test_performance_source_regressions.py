#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def test_blocked_note_mode_keys_are_consumed() -> None:
    keyboard = (ROOT / "src/input/performance_keyboard.cpp").read_text(
        encoding="utf-8"
    )
    start = keyboard.index("bool PerformanceKeyboard::keyDown")
    end = keyboard.index("bool PerformanceKeyboard::keyUp", start)
    block = keyboard[start:end]

    layout_pos = block.index("if (!isPerformanceKey(physicalKey)) return false;")
    note_mode_pos = block.index("if (!noteModeEnabled_) return false;")
    blocked_pos = block.index("if (!enabled_ || transportPlaying_) return true;")
    note_pos = block.index("noteForKey", blocked_pos)

    require(layout_pos < note_mode_pos < blocked_pos < note_pos,
            "layout membership must be decided before transport blocks NoteOn")
    require("return true;" in block[blocked_pos:note_pos],
            "transport-blocked performance keys must remain consumed")

    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")
    route_pos = display.index("performance_keyboard_.keyDown(event.key)")
    fallback_pos = display.index("// 3) Global navigation fallback", route_pos)
    require(route_pos < fallback_pos,
            "NOTE-mode routing must run before legacy global fallback")


def test_performance_all_notes_off_is_target_scoped() -> None:
    sink = (ROOT / "src/input/internal_synth_output.cpp").read_text(
        encoding="utf-8"
    )
    start = sink.index("case MusicalEventType::AllNotesOff")
    end = sink.index("\n        }\n    }", start)
    block = sink[start:end]

    require("engine_.liveNote(voice)" in block,
            "AllNotesOff must inspect only the event target voice")
    require("engine_.liveNoteOff(voice" in block,
            "AllNotesOff must release only the live-owned target voice")
    require("engine_.allLiveNotesOff()" not in block,
            "performance AllNotesOff must not become a global voice release")


def test_note_mode_is_explicit_and_runtime_only() -> None:
    page = (ROOT / "src/ui/pages/perform_page.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/input/performance_keyboard.h").read_text(
        encoding="utf-8"
    )
    scenes = (ROOT / "scenes.h").read_text(encoding="utf-8")
    storage = (ROOT / "scenes.cpp").read_text(encoding="utf-8")

    require("keyboard_.toggleNoteMode();" in page,
            "PERFORM must expose an explicit NOTE-mode toggle")
    require("NOTE MODE: %s" in page,
            "PERFORM must display NOTE-mode state")
    require("bool noteModeEnabled_{true};" in header,
            "PERFORM must remain immediately playable by default")
    require("noteModeEnabled" not in scenes and "noteModeEnabled" not in storage,
            "NOTE mode must remain runtime-only in this PR")


def main() -> None:
    test_blocked_note_mode_keys_are_consumed()
    test_performance_all_notes_off_is_target_scoped()
    test_note_mode_is_explicit_and_runtime_only()
    print("performance source regressions: OK")


if __name__ == "__main__":
    main()
