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
if fail:
    for m in fail: print("FAIL:", m)
    print(f"0.9.14 P0-B1 source regressions: FAIL ({len(fail)})"); sys.exit(1)
print("0.9.14 P0-B1 source regressions: PASS")
