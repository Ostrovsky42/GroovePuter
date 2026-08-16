#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = (ROOT / "src/ui/pages/pattern_edit_page.h").read_text(encoding="utf-8")
CPP = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
LEGACY = (ROOT / "src/ui/pages/pattern_edit_page_legacy.h").read_text(encoding="utf-8")
PREPARE = (ROOT / "src/state/synth_pattern_edit.h").read_text(encoding="utf-8")
PATTERN_BAR = (ROOT / "src/ui/components/pattern_selection_bar.h").read_text(encoding="utf-8")
BANK_BAR = (ROOT / "src/ui/components/bank_selection_bar.h").read_text(encoding="utf-8")


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
    # Pure PREPARE semantics must remain independent from MiniAcid/live state.
    require("namespace PatternEdit" in PREPARE,
            "R3 must expose a pure prepared Pattern edit layer")
    for name in ("samePattern", "clearStep", "adjustNote", "adjustOctave",
                 "toggleAccent", "toggleSlide", "cycleFx", "adjustFxParam",
                 "rotate"):
        require(f"{name}(" in PREPARE, f"missing prepared Pattern primitive: {name}")
    for forbidden in ("MiniAcid", "SceneManager", "UndoOwner", "markSceneMutated",
                      "SD.", "ArduinoJson", "std::vector"):
        require(forbidden not in PREPARE,
                f"pure Pattern PREPARE layer leaked runtime dependency: {forbidden}")
    require("step.velocity =" not in between(PREPARE, "inline void clearStep", "inline void adjustNote") and
            "step.timing =" not in between(PREPARE, "inline void clearStep", "inline void adjustNote"),
            "R3 clearStep must preserve legacy velocity/timing semantics")

    # One canonical page helper must perform capture -> prepare -> full no-op ->
    # bounded commit. The owner, not the page, advances Scene revision.
    helper = between(HEADER, "template <typename PrepareFn>", "template <typename F>")
    require("captureCurrentSynthPatternUndo" in helper,
            "Pattern mutation helper must capture a stable before receipt")
    require("SynthPattern after = before.before" in helper,
            "Pattern mutation helper must build a detached after-state")
    require("PatternEdit::samePattern" in helper,
            "Pattern mutation helper must reject full-state no-ops before COMMIT")
    require(helper.find("PatternEdit::samePattern") < helper.find("undoOwner().commitPrepared"),
            "Pattern no-op detection must happen before Undo publication")
    require("synthPatternUndoTargetAvailable" in helper,
            "Pattern target residency must be validated before COMMIT")
    require("undoOwner().commitPrepared" in helper and "UndoKind::Pattern" in helper,
            "manual Pattern COMMIT must use the authoritative R2 owner")
    require("restoreSynthPatternUndo(manager, prepared)" in helper,
            "COMMIT must be one bounded resident Pattern assignment")
    require("audio_guard_(apply)" in helper,
            "Pattern COMMIT must keep the existing audio exclusion boundary")
    require("markSceneMutated" not in helper,
            "Pattern page must not double-own revision advancement")

    # Audio guard is no longer synonymous with persistence. Runtime selectors
    # may use it without dirtying the Scene or expiring a valid Undo receipt.
    guard = between(HEADER, "template <typename F>", "IGfx& gfx_")
    require("audio_guard_" in guard,
            "Pattern page must retain audio exclusion for runtime selectors")
    require("markSceneMutated" not in guard,
            "audio guard must not implicitly own persistent revision in R3")

    # The large legacy implementation remains retained behind an unowned entry;
    # the new wrapper is the only route from the public handler to manual edits.
    require("bool handleEventLegacyUnowned(UIEvent& ui_event);" in HEADER,
            "R3 must declare the retained unowned legacy entry")
    require("#define handleEvent handleEventLegacyUnowned" in CPP,
            "legacy include must compile under the unowned entry name")
    wrapper = between(
        CPP,
        "bool PatternEditPage::handleEventLegacy(UIEvent& ui_event)",
        "int PatternEditPage::noteForEntryKey")
    require(wrapper.count("commitPatternMutation") >= 10,
            "R3 wrapper does not cover the expected manual Pattern edit surface")

    # The include-level alias also rewrites child component handleEvent calls.
    # Keep those forwarding shims explicit and side-effect free so the retained
    # legacy body compiles without widening its behavior.
    shim = "bool handleEventLegacyUnowned(UIEvent& ui_event) { return handleEvent(ui_event); }"
    require(shim in PATTERN_BAR,
            "Pattern selection bar must preserve R3 legacy-macro compatibility")
    require(shim in BANK_BAR,
            "Bank selection bar must preserve R3 legacy-macro compatibility")

    for forbidden in ("mini_acid_.clear303Step", "mini_acid_.adjust303Step",
                      "mini_acid_.toggle303", "mini_acid_.rotatePattern",
                      "mini_acid_.cycle303StepFx", "editCurrentSynthPattern"):
        require(forbidden not in wrapper,
                f"manual Pattern wrapper bypasses PREPARE/COMMIT via {forbidden}")

    require("GROOVEPUTER_APP_EVENT_PASTE" in wrapper and
            "g_pattern_step_clipboard" in wrapper and
            "g_pattern_clipboard.pattern" in wrapper,
            "Paste must be prepared as one complete Pattern mutation")
    require("return handleEventLegacy(appEvent);" in wrapper,
            "Ctrl+V must re-enter the owned Paste path instead of unowned recursion")
    require("Selection Cleared" in wrapper,
            "selection clear UX must stay reachable through the owned mutation path")

    # R2 Reset/Undo remains exactly in the retained legacy body.
    require("// Alt + Backspace = Reset Pattern. R2 routes this one destructive edit" in LEGACY and
            "undoOwner().commitPrepared" in LEGACY,
            "R3 must preserve the accepted R2 Reset Pattern vertical slice")
    require("case GROOVEPUTER_APP_EVENT_UNDO:" in LEGACY and
            "undoOwner().undoPrepared<SynthPatternUndoPayload>" in LEGACY,
            "R3 must preserve the accepted R2 Pattern Undo path")
    require("if (ui_event.alt && isBackspace)" in wrapper and
            "handleEventLegacyUnowned(ui_event)" in wrapper,
            "R3 wrapper must delegate Alt+Backspace to the R2 owner path")

    # Direct note entry is also persistent Pattern editing and may not bypass R3.
    note_write = between(
        CPP,
        "void PatternEditPage::writeNoteEntryStep",
        "bool PatternEditPage::handleNoteEntryKey")
    require("commitPatternMutation" in note_write and
            "editCurrentSynthPattern" not in note_write and
            "withAudioGuard" not in note_write,
            "NOTE ENTRY writes must publish one canonical Pattern receipt")

    # Generation remains outside the manual R3 Pattern helper. Plain G remains
    # the quantized generation entry and Generation UndoOwner remains the sole
    # persistent revision owner. C changes audible publication, not R3 ownership.
    public_generate = between(
        CPP,
        "// Outside NOTE ENTRY, plain G rerolls only this physical synth voice",
        "// Global navigation, pattern rotation/FX editing")
    require("regenerateSynthWithQuantizedCommit" in public_generate and
            "PendingNextBar" in public_generate,
            "R3 must preserve quantized generation / next-bar activation behavior")
    require("GroovePuterState::markSceneMutated();" not in public_generate,
            "plain G must not double-own revision after Generation commit ownership")
    require("commitPatternMutation" not in public_generate,
            "manual Pattern owner must not claim quantized generation")

    # B2's fallback generator remains outside the manual R3 Pattern owner. C
    # keeps its dedicated compact Generation receipt and changes only audible
    # publication: PLAY must join the bounded activation owner instead of doing
    # a same-index immediate Pattern replacement.
    generated_legacy = between(
        wrapper,
        "// C keeps B2's legacy/fallback musical generator",
        "if (keyF)")
    require("preparePatternEditorGeneration" in generated_legacy and
            "undoOwner().commitPrepared" in generated_legacy and
            "UndoKind::Generation" in generated_legacy and
            "armCompactSynthActivation" in generated_legacy and
            "completeArmedActivation" in generated_legacy and
            "set303PatternIndex(voice_index_, currentPattern)" not in generated_legacy and
            "handleEventLegacyUnowned(ui_event)" not in generated_legacy and
            "markSceneMutated" not in generated_legacy and
            "commitPatternMutation" not in generated_legacy,
            "fallback generation must stay a dedicated Generation receipt with C activation ownership")

    # Pattern/bank selection is runtime-only. Song chaining is a separate
    # persistent domain and retains a revision advance only when it really writes.
    selector = between(
        CPP,
        "// Q-I is the only keyboard path for slots 1-8 outside NOTE ENTRY.",
        "// Bank selection has one unambiguous binding.")
    require("commitSongMutation" in selector and
            "SongEdit::setPattern" in selector and
            "markSceneMutated" not in selector,
            "Song chaining must route through canonical R4 Song ownership while selector navigation stays runtime-only")

    print("0.9.8 R3 Pattern mutation ownership source regressions: OK")


if __name__ == "__main__":
    main()
