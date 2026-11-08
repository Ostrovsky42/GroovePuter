#!/usr/bin/env python3
"""P0-B1 source audit: bass/chord follow the bar function without changing frozen paths."""
from pathlib import Path
import re, sys

ROOT = Path(__file__).resolve().parents[1]
read = lambda p: (ROOT / p).read_text(encoding="utf-8")
MIG = read("src/generation/migration/strong_rhythm_migration.cpp")
ROLES = read("src/generation/roles/bar_function_roles.h")
fail = []
req = lambda c, m: None if c else fail.append(m)

# Effective plan is the one exported as evidence and used to build Synth A.
req("applyToBassPlan(\n      result.phraseBarFunction, bassBase.plan" in MIG, "bass plan must be transformed from the base plan by the bar function")
req("const BassRhythmResult& bass = bassEffective;" in MIG, "downstream must use the effective bass plan")
req("result.bassRhythmPlan = bass.plan;" in MIG, "exported evidence must be the effective plan")
# Other roles must not be handed positions freed by the function.
req("chordRequest.bassOnsets = bassBase.plan.onsets;" in MIG, "chord must be built from the ORIGINAL bass attacks")
req("melodicRequest.bassOnsets = bassBase.plan.onsets;" in MIG, "melodic must be built from the ORIGINAL bass attacks")
req(": chordBase.plan.onsets;" in MIG, "melodic must be built from the ORIGINAL chord attacks")
req("applyToChordPlan(\n      result.phraseBarFunction, chordBase.plan, bassBase.plan.onsets" in MIG, "chord function must use the base bass attacks")
# Frozen functions have no case in the role transforms (they fall to default: unchanged).
body = ROLES[ROLES.index("inline BassRhythmPlan applyToBassPlan"):]
for frozen in ("Statement", "Repeat", "Return", "Response", "RepeatWithGhosts"):
    req(f"case BarFunction::{frozen}" not in body, f"{frozen} must stay a no-op (no case in the transforms)")
for used in ("Break", "Reduction", "Build", "Turnaround"):
    req(f"case BarFunction::{used}" in body, f"{used} transform missing")
# Purity / boundedness: no engine, no allocation, no randomness source.
for token in ("std::vector", "new ", "malloc", "rand(", "std::random", "deterministicValue"):
    req(token not in ROLES, f"bar_function_roles.h must not use {token!r}")
# The role realizers themselves are untouched.
for f in ("bass_rhythm.cpp", "chord_rhythm.cpp"):
    req("BarFunction" not in read("src/generation/roles/" + f), f"{f} must not learn about BarFunction")

# ---- B2: admission is per archetype AND per scenario, in one shared function ----
ADM = read("src/generation/composition/phrase_evolution_admission.h")
EXEC = read("src/generation/migration/phrase_execution.cpp")
BRIDGE = read("src/generation/migration/strong_rhythm_live_bridge.cpp")
CAT = read("src/generation/rhythm/reference_phrase_catalog_data.h")
req("GenerativeMode::Acid" in ADM and "GenerativeMode::House" in ADM, "Acid and House must be excluded at scenario level")
req("phraseEvolutionAdmitted(genre, definition->key)" in EXEC, "phrase execution must use the scenario-level admission")
req("phraseEvolutionAdmitted(auditionSettings, selection.archetype)" in BRIDGE, "live audition must use the scenario-level admission")
req("applyPhraseLawToExecution(" in read("tools/m0/m0a_corpus.cpp"),
    "the M0 tool must apply phrase laws through the same product helper")
req("applyPhraseLawToExecution" in EXEC and "= admittedPhraseTrajectory(\n      execution.settings" in EXEC,
    "the product law helper must go through the shared admission")
# Archetypes whose hard relationship makes the multi-bar catalog INVALID must never be whitelisted
# (probe, P0-B2: 401 straight_drive, 402 offbeat_open_hat, 409 one_drop_space, 419 shuffled_4x4).
for bad in (401, 402, 409, 419):
    req(not re.search(rf"case {bad}:", CAT), f"archetype {bad} makes the phrase catalog invalid and must not be admitted")
req(re.search(r"case 410:", CAT) is not None, "steppers (410) is the first B2 archetype")
req(re.search(r"case 713:", CAT) is not None, "funk_house_bridge (713) is the second B2 archetype")

if fail:
    for m in fail: print("FAIL:", m)
    print(f"0.9.14 P0-B1 source regressions: FAIL ({len(fail)})"); sys.exit(1)
print("0.9.14 P0-B1 source regressions: PASS")
