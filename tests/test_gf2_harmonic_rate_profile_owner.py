#!/usr/bin/env python3
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
PROFILE_H = ROOT / "src/generation/composition/generation_profile.h"
PROFILE_CPP = ROOT / "src/generation/composition/generation_profile.cpp"
LAWS_H = ROOT / "src/generation/composition/genre_structural_laws.h"
PHRASE_EXECUTION_CPP = ROOT / "src/generation/migration/phrase_execution.cpp"


def fail(message: str) -> None:
    print(f"GF2 HARMONIC PROFILE OWNER FAIL: {message}", file=sys.stderr)
    raise SystemExit(1)


profile_h = PROFILE_H.read_text(encoding="utf-8")
profile_cpp = PROFILE_CPP.read_text(encoding="utf-8")
laws_h = LAWS_H.read_text(encoding="utf-8")
phrase_execution_cpp = PHRASE_EXECUTION_CPP.read_text(encoding="utf-8")

# Harmonic rhythm is a musician-facing structural decision, so the authoritative
# profile definition must own it beside corridor, phrase law and role vocabulary.
# A second switch on genre/recipe identity is exactly the duplication this guard
# is intended to prevent.
for token in (
    "HarmonicChangeRateId harmonicChangeRate = HarmonicChangeRateId::Every2Beats;",
    "struct GenerationProfileView",
    "struct GenerationCompositionResult",
):
    if token not in profile_h:
        fail(f"profile-facing harmonic rate contract missing: {token!r}")

for token in (
    "HarmonicChangeRateId harmonicChangeRate;",
    "result.harmonicChangeRate = definition->harmonicChangeRate;",
    "result.harmonicChangeRate = profile.harmonicChangeRate;",
    "isValidHarmonicChangeRate(profile.harmonicChangeRate)",
):
    if token not in profile_cpp:
        fail(f"profile owner wiring missing: {token!r}")

for forbidden in (
    "GenerativeMode::LoFi",
    "kLoFiHouseRecipeId",
):
    if forbidden in laws_h:
        fail(f"parallel genre/recipe switch remains in structural-law accessor: {forbidden}")
if "return profile.harmonicChangeRate;" not in laws_h:
    fail("structural-law accessor does not consume the authoritative profile field")

lofi_profiles = [
    line.strip()
    for line in profile_cpp.splitlines()
    if "profile(GenerativeMode::LoFi," in line
]
if len(lofi_profiles) != 5:
    fail(f"expected five Lo-Fi profile definitions, found {len(lofi_profiles)}")

for line in lofi_profiles:
    if "kLoFiHouseRecipeId" in line:
        if "HarmonicChangeRateId::Every2Beats" not in line:
            fail("Lo-Fi House must explicitly own its two-beat harmonic rate")
    elif "HarmonicChangeRateId::Every4Beats" not in line:
        fail(f"slow/base Lo-Fi profile lacks explicit four-beat harmonic rate: {line}")

# Phrase preparation already owns a frozen composition snapshot. Harmonic rate
# must be consumed from that snapshot, not re-derived from mutable settings after
# selection has completed.
if "destination.selection.composition.harmonicChangeRate" not in phrase_execution_cpp:
    fail("phrase execution does not consume frozen composition harmonic rate")
if "const GenerationProfileView structuralProfile = generationProfileFor(settings);" in phrase_execution_cpp:
    fail("phrase execution still re-reads profile identity after frozen selection")

print("GF2 harmonic-rate profile ownership: OK")
print("profile table is the single genre/recipe owner: YES")
print("phrase composition freezes the selected harmonic rate: YES")
print("parallel Lo-Fi switch in structural-law accessor: NO")
