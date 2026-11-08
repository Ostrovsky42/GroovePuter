#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

EDGES = (ROOT / "src/input/cardputer_input_edges.h").read_text(encoding="utf-8")
PREPARE = (ROOT / "src/state/synth_pattern_edit.h").read_text(encoding="utf-8")
ENGINE = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
GRID_H = (ROOT / "src/ui/components/drum_sequencer_grid.h").read_text(encoding="utf-8")
GRID = (ROOT / "src/ui/components/drum_sequencer_grid.cpp").read_text(encoding="utf-8")
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page_legacy.h").read_text(encoding="utf-8")
HELP = (ROOT / "src/ui/global_help_content.h").read_text(encoding="utf-8")
PATTERN_PAGE = (ROOT / "src/ui/pages/pattern_edit_page.cpp").read_text(encoding="utf-8")
HUB = (ROOT / "src/ui/pages/sequencer_hub_page.cpp").read_text(encoding="utf-8")
SONG = (ROOT / "src/ui/pages/song_page.cpp").read_text(encoding="utf-8")
SKETCH = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")


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
    # Cardputer ADV reserves four punctuation positions as physical arrows.
    # Both plain and Shift word glyphs from those keys are shadows of the HID
    # navigation event and must never become a second command.
    for plain, shifted, hid_name, hid_hex, scan in (
        (";", ":", "kCardputerArrowUpHid", "0x33", "GROOVEPUTER_UP"),
        (",", "<", "kCardputerArrowLeftHid", "0x36", "GROOVEPUTER_LEFT"),
        (".", ">", "kCardputerArrowDownHid", "0x37", "GROOVEPUTER_DOWN"),
        ("/", "?", "kCardputerArrowRightHid", "0x38", "GROOVEPUTER_RIGHT"),
    ):
        require(
            f"rawValue == static_cast<WordChar>('{plain}')" in EDGES
            and f"rawValue == static_cast<WordChar>('{shifted}')" in EDGES
            and f"shadowArrowHid = {hid_name};" in EDGES,
            f"missing Cardputer arrow-only mapping for {plain}/{shifted}",
        )
        require(
            f"hid == {hid_hex}" in SKETCH and f"evt.scancode = {scan};" in SKETCH,
            f"physical Cardputer HID {hid_hex} must remain canonical {scan}",
        )
    require(
        "shadowArrowHid != 0 && containsHid(current, shadowArrowHid)" in EDGES,
        "word glyphs from Cardputer arrow positions must be suppressed beside their HID",
    )
    require(
        "ui_event.ctrl && ui_event.alt" in SONG
        and "nav == GROOVEPUTER_UP" in SONG
        and "nav == GROOVEPUTER_DOWN" in SONG
        and "moveCursorToRow(0)" in SONG,
        "Song Top/End must use reachable Ctrl+Alt+Up/Down instead of arrow punctuation",
    )

    # Synth F is now one musician-facing effect: audible retrigger. Reverse is
    # still a valid StepFx for sampled drums, but must not be offered by synth F.
    cycle = between(PREPARE, "inline void cycleFx", "inline void adjustFxParam")
    require("kDefaultRetrigCount = 2" in PREPARE,
            "new synth retrig must start from an audible non-zero value")
    require("kMaxRetrigCount = 8" in PREPARE,
            "synth retrig edit range must stay bounded")
    require("StepFx::Retrig" in cycle and "StepFx::Reverse" not in cycle,
            "synth F must toggle Retrig only, not expose unsupported Reverse")
    adjust = between(PREPARE, "inline void adjustFxParam", "inline void rotate")
    require("clampRetrigCount(step.fxParam)" in adjust
            and "value < 1" in adjust and "value > kMaxRetrigCount" in adjust,
            "synth retrig adjustment must start from effective R1..R8 value")
    require('"F           Retrig on/off"' in HELP
            and '"Alt+Up/Dn   Retrig count 1..8"' in HELP,
            "on-device help must describe the actual retrig contract")
    retrig_param_owner = between(
        PATTERN_PAGE,
        "// On Cardputer the physical punctuation/arrow keys can carry meta=true",
        "// Global navigation, pattern rotation and meta note editing keep their",
    )
    require(
        "commitPatternMutation" in retrig_param_owner
        and "PatternEdit::adjustFxParam(" in retrig_param_owner
        and "pattern, step, delta" in retrig_param_owner
        and "mini_acid_.adjust303StepFxParam" not in retrig_param_owner,
        "Alt+Up/Down retrig count must use the same Pattern owner/runtime refresh as F",
    )

    # Prove Retrig is not merely a label: projected SynthStep FX reaches the
    # current audio runtime and arms the existing retrigger state.
    trigger = between(
        ENGINE,
        "void MiniAcid::triggerSynthStep_(",
        "void MiniAcid::triggerDrumVoice_",
    )
    require(
        "event.fx == static_cast<uint8_t>(StepFx::Retrig)" in trigger
        and "event.fxParam > 0" in trigger
        and "retrig.active = true" in trigger,
        "synth Retrig must arm an audible runtime retrigger",
    )
    require(
        "patternPlaybackState_[0].acceptRetrigger(patternRetrigEvent_[0])" in ENGINE
        and "patternPlaybackState_[1].acceptRetrigger(patternRetrigEvent_[1])" in ENGINE,
        "both synth voices must consume prepared retriggers",
    )
    require(
        "event.durationSubticks" in trigger
        and "kSubticksPerStep" in trigger
        and "retrigSpanSamples" in trigger,
        "Rn retriggers must be scheduled inside the active event gate",
    )
    require(
        "PatternEdit::clampRetrigCount(event.fxParam)" in trigger
        and "retrig.countRemaining = retrigCount;" in trigger,
        "legacy persisted retrig values must use the shared R1..R8 execution bound",
    )

    # Drum grid must fit all eight lanes and map visible names to global mute
    # digits. Accent is an individual hit property, not an aggregate ACC row.
    require(
        '{"3KIK", "4SNR", "5HH1", "6HH2", "7PR1", "8PR2", "9RIM", "0CLP"}'
        in GRID,
        "default drum lanes must expose direct 3..0 mute mapping",
    )
    require(
        'engine == "SP12"' in GRID
        and 'return "0RIM";' in GRID
        and 'return "9CLP";' in GRID,
        "SP12 grid labels must expose the swapped 9/0 mute bindings",
    )
    mute9 = between(SKETCH, "} else if (c == '9') {", "} else if (c == '0') {")
    mute0 = between(SKETCH, "} else if (c == '0') {", "} else if (c == 'k' || c == 'K') {")
    require(
        'currentDrumEngineName() == "SP12"' in mute9
        and "toggleMuteClap()" in mute9
        and "toggleMuteRim()" in mute9,
        "global key 9 must map SP12->Clap and non-SP12->Rim",
    )
    require(
        'currentDrumEngineName() == "SP12"' in mute0
        and "toggleMuteRim()" in mute0
        and "toggleMuteClap()" in mute0,
        "global key 0 must map SP12->Rim and non-SP12->Clap",
    )
    require("drawAccentLabel" not in GRID and "onToggleAccent" not in GRID_H,
            "obsolete aggregate ACC-row UI must stay removed")
    require("hit && stepData.accent" in GRID,
            "accent state must be visible on the actual hit cell")
    require("layout.grid_y = bounds.y + kStepHeaderHeight;" in GRID,
            "all height below the step header must belong to the eight drum lanes")

    accent = between(DRUM, "if (key_a) {", "if (key_b")
    require("activeDrumVoice()" in accent and "activeDrumStep()" in accent,
            "drum A must target the cursor voice and step")
    require("if (!current.voices[voice].steps[step].hit)" in accent,
            "drum A must fail closed on an empty cell")
    require("pattern.voices[voice].steps[step].accent" in accent,
            "drum A must toggle only the selected hit accent")
    require("for (int v" not in accent,
            "drum A must not accent the entire time column")

    require(
        DRUM.count("value.hit = !value.hit;") == 2
        and DRUM.count("value.accent = false;") >= 2,
        "drum hit toggle must clear accent so remove/re-add cannot resurrect hidden accent",
    )
    engine_toggle = between(
        ENGINE, "void MiniAcid::toggleDrumStep(", "void MiniAcid::setDrumAccentStep")
    require(
        "value.hit = !value.hit;" in engine_toggle
        and "value.accent = false;" in engine_toggle,
        "engine-level Drum hit toggle must clear accent for Hub and every other caller",
    )
    require(
        "onToggleAccent" not in HUB
        and "toggleDrumAccentStep" not in HUB
        and "toggleDrumAccentStep" not in ENGINE
        and HUB.count('UI::showToast("ACCENT: ADD HIT", 900);') >= 2
        and HUB.count("setDrumAccentStep(") >= 2,
        "Hub must use per-hit accent with empty-cell fail-closed semantics",
    )
    require(
        "cb.onToggle = [this](int step, int voice)" in HUB
        and "toggleDrumStep(voice, step)" in HUB,
        "Hub must honor Drum grid callback order (step, voice)",
    )
    require(
        '"RETRIG OFF"' in PATTERN_PAGE
        and '"RETRIG R%u"' in PATTERN_PAGE,
        "Retrig edits need theme-independent musician feedback",
    )

    print("Instrument interaction closure source regressions: OK")


if __name__ == "__main__":
    main()
