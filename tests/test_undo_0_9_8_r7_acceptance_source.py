#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def read(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def require(text: str, needle: str, context: str) -> None:
    if needle not in text:
        raise AssertionError(f"{context}: missing {needle!r}")


undo_ux = read("src/ui/undo_ux.h")
help_content = read("src/ui/global_help_content.h")
core = read(".github/workflows/core-regressions.yml")
dram = read("scripts/check_cardputer_dram_budget.sh")
acceptance = read("docs/releases/0_9_8_R7_ADV_ACCEPTANCE.md")

# R6 compatibility contract remains frozen into the release candidate.
require(undo_ux, "event.event_type != GROOVEPUTER_KEY_DOWN || !event.ctrl", "Ctrl+U modifier contract")
require(undo_ux, "event.alt || event.meta || event.shift", "Ctrl+U modifier contract")
require(undo_ux, "event.scancode == GROOVEPUTER_U", "Ctrl+U key contract")
require(help_content, '"Ctrl+U      Undo last edit"', "global help")
require(help_content, '"Ctrl+Z/X/C/V Reset parameter"', "Synth Sound compatibility")

# Exact software acceptance must continue to exercise both Cardputer profiles
# and the repository DRAM policy on every R7 PR.
require(core, "Compile Cardputer ADV firmware", "Core ADV gate")
require(core, "Check fixed DRAM budget", "Core ADV gate")
require(core, "scripts/check_cardputer_dram_budget.sh", "Core DRAM gate")
require(core, "Compile and check SEQTRAK MIDI-only firmware", "Core SEQTRAK gate")

# Do not silently promote the current provisional static ceiling into a fully
# derived hardware-safety claim. R7 must preserve this evidence gap explicitly.
require(dram, 'MAX_BYTES="${2:-191488}"', "DRAM policy ceiling")
require(dram, "provisional exception", "DRAM policy state")
require(dram, "items 5-7 remain pending", "DRAM evidence state")

# Release acceptance is intentionally fail-closed with respect to physical ADV
# evidence. CI may make the PR merge-ready, but cannot manufacture device data.
require(acceptance, "Hardware state: PENDING DEVICE", "R7 hardware state")
require(acceptance, "DO NOT TAG / DO NOT CLAIM RELEASE ACCEPTED", "R7 release rule")
require(acceptance, "Ctrl+U", "R7 shortcut exercise")
require(acceptance, "Ctrl+Z", "R7 collision exercise")
require(acceptance, "30-minute soak", "R7 soak exercise")
require(acceptance, "Generation/activation Undo remains a 0.9.9 boundary", "R7 scope boundary")

print("0.9.8 R7 acceptance source regressions: PASS")
