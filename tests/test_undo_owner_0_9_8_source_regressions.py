#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OWNER = (ROOT / "src/state/undo_owner.h").read_text(encoding="utf-8")
RECEIPTS = (ROOT / "src/state/undo_receipts.h").read_text(encoding="utf-8")
REVISION = (ROOT / "src/state/scene_revision.h").read_text(encoding="utf-8")
PATTERN_CPP = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
PATTERN = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def between(text: str, start: str, end: str) -> str:
    begin = text.find(start)
    require(begin >= 0, f"missing source anchor: {start}")
    finish = text.find(end, begin + len(start))
    require(finish >= 0, f"missing source end anchor: {end}")
    return text[begin:finish]


def main() -> None:
    require("constexpr std::size_t kUndoPayloadBytes = 1536" in OWNER,
            "authoritative R2 Undo payload capacity must stay measured and fixed")
    require("BoundedUndoSlot<kUndoPayloadBytes> slot_" in OWNER,
            "authoritative owner must use the bounded R1 receipt primitive")
    require("uint32_t committed_revision_" in OWNER,
            "owner must distinguish receipt publication from later persistent edits")
    require("current.currentRevision != committed_revision_" in OWNER,
            "later persistent mutation must expire stale Undo")
    require("current.persistedRevision == committed_revision_" in OWNER,
            "Save-after-edit Undo dirty semantics must be explicit")
    require("GroovePuterState::markSceneMutated();" in OWNER,
            "one successful owner commit must advance Scene revision")
    require(OWNER.count("GroovePuterState::markSceneMutated();") == 1,
            "owner implementation must have one canonical revision advance point")
    require("inline UndoOwner& undoOwner()" in OWNER,
            "R2 must establish one authoritative retained Undo owner")
    require("static UndoOwner owner{}" in OWNER,
            "authoritative owner must have one fixed-lifetime instance")
    require("static_assert(sizeof(UndoOwner) <= 1552" in OWNER,
            "authoritative Undo owner DRAM ceiling must be compile-time guarded")

    for forbidden in ("std::vector", "std::deque", " malloc(", " new ",
                      "ArduinoJson", "SD.", "AudioGuard", "MusicalEvent"):
        require(forbidden not in OWNER,
                f"authoritative Undo owner leaked forbidden dependency/token: {forbidden}")

    require("struct SynthPatternUndoPayload" in RECEIPTS and
            "static_assert(sizeof(SynthPatternUndoPayload) == 116" in RECEIPTS,
            "Pattern receipt must retain its measured fixed shape")
    require("pageIndex" in RECEIPTS and "bankIndex" in RECEIPTS and
            "patternIndex" in RECEIPTS and "synthIndex" in RECEIPTS,
            "Pattern Undo must capture stable persistent address identity")
    require("manager.currentPageIndex() == receipt.pageIndex" in RECEIPTS,
            "paged Pattern Undo must fail target validation instead of loading files")
    require("restoreSynthPatternUndo" in RECEIPTS and
            "exchangeSynthPatternUndo" in RECEIPTS,
            "Pattern receipt must expose bounded in-memory restore/toggle primitives")
    require("isCanonicalClearedSynthPattern" in RECEIPTS and
            "value.velocity != 100" in RECEIPTS and
            "value.probability != 100" in RECEIPTS,
            "Pattern no-op detection must check canonical full step state")

    # R2 first vertical slice: one existing destructive Pattern edit only.
    require('#include "../../state/undo_owner.h"' in PATTERN_CPP and
            '#include "../../state/undo_receipts.h"' in PATTERN_CPP,
            "Pattern editor must consume the canonical owner/receipt boundary")
    reset_block = between(
        PATTERN,
        "// Alt + Backspace = Reset Pattern. R2 routes this one destructive edit",
        "if (is_backspace && has_selection_)")
    require("captureCurrentSynthPatternUndo" in reset_block,
            "Pattern reset must PREPARE a stable before-state receipt")
    require("isCanonicalClearedSynthPattern" in reset_block,
            "Pattern reset no-op must not replace Undo or revision")
    require("undoOwner().commitPrepared" in reset_block and
            "UndoKind::Pattern" in reset_block,
            "Pattern reset COMMIT must route through the authoritative owner")
    require("audio_guard_(clear_pattern)" in reset_block,
            "Pattern data write must remain audio-guarded")
    require("withAudioGuard" not in reset_block and
            "markSceneMutated" not in reset_block,
            "Pattern page must not double-own revision advancement")

    undo_case = between(
        PATTERN,
        "case GROOVEPUTER_APP_EVENT_UNDO:",
        "default:\n        return false;")
    require("undoOwner().togglePrepared<SynthPatternUndoPayload>" in undo_case,
            "Pattern application Undo/redo toggle must route through the authoritative owner")
    require("synthPatternUndoTargetAvailable" in undo_case and
            "exchangeSynthPatternUndo" in undo_case,
            "Pattern toggle must validate resident target before bounded exchange")
    require("audio_guard_(restore)" in undo_case,
            "Pattern exchange must remain audio-guarded")
    require("UndoResult::TargetUnavailable" in undo_case and
            'UI::showToast("UNDO: EXPIRED"' in undo_case,
            "Pattern Undo must remain fail-closed for unavailable/expired state")

    # R2 does not own shortcut rollout. It only makes the existing application
    # event semantically correct; global/local Ctrl+Z is deferred to the UI stage.
    require("app_evt.app_event_type = GROOVEPUTER_APP_EVENT_UNDO" not in PATTERN,
            "R2 must not add a local Ctrl+Z fallback before UI ownership migration")

    require("static_assert(sizeof(SceneRevisionState) == 8" in REVISION,
            "R2 relies on the frozen 8-byte Scene revision contract")
    print("0.9.8 R2 Undo owner + Pattern slice source regressions: OK")


if __name__ == "__main__":
    main()