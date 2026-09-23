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
    # One physical Cardputer arrow may appear as HID + punctuation word.
    # The punctuation shadow must never become a second musical command.
    for punctuation, hid_name in (
        (";", "kCardputerArrowUpHid"),
        (",", "kCardputerArrowLeftHid"),
        (".", "kCardputerArrowDownHid"),
        ("/", "kCardputerArrowRightHid"),
    ):
        require(
            f"rawValue == static_cast<WordChar>('{punctuation}')" in EDGES
            and f"shadowArrowHid = {hid_name};" in EDGES,
            f"missing Cardputer arrow shadow mapping for {punctuation}",
        )
    require(
        "shadowArrowHid != 0 && containsHid(current, shadowArrowHid)" in EDGES,
        "arrow punctuation must be suppressed only when the matching HID arrow exists",
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
    require("value < 1" in adjust and "value > kMaxRetrigCount" in adjust,
            "synth retrig count must remain in 1..8")
    require('"F           Retrig on/off"' in HELP
            and '"Alt+Up/Dn   Retrig count 1..8"' in HELP,
            "on-device help must describe the actual retrig contract")

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
        and 'return "9CLP";' in GRID
        and 'currentDrumEngineName() == "SP12"' in SKETCH
        and "toggleMuteClap()" in SKETCH
        and "toggleMuteRim()" in SKETCH,
        "SP12 grid labels must mirror its swapped 9/0 global mute bindings",
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

    print("Instrument interaction closure source regressions: OK")


if __name__ == "__main__":
    main()
