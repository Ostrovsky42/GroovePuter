#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SELECTION_H = (ROOT / "src/generation/composition/rhythm_selection.h").read_text()
SELECTION_CPP = (ROOT / "src/generation/composition/rhythm_selection.cpp").read_text()
SCENES_H = (ROOT / "scenes.h").read_text()
SCENES_CPP = (ROOT / "scenes.cpp").read_text()
GENRE_PAGE = (ROOT / "src/ui/pages/genre_page.cpp").read_text()
REFERENCE = (ROOT / "src/generation/rhythm/reference_vocabulary.cpp").read_text()
REALIZER = (ROOT / "src/generation/rhythm/rhythm_realizer.cpp").read_text()
RUNNER = (ROOT / "tests/run_rhythm_stage7c_tests.sh").read_text()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


for token in (
    "RhythmSelectionMode",
    "RhythmSelectionIntent",
    "RhythmSelectionResult",
    "rhythmCompatibilityFor",
    "resolveRhythmSelectionFromView",
    "compatibleRhythmId",
):
    require(token in SELECTION_H, f"missing Stage 7C selection contract: {token}")

for token in (
    "kAcidBase",
    "kSynthwaveBase",
    "kDarksynthBase",
    "kElectroBase",
    "kRaveBase",
    "kReggaeBase",
    "kTripHopBase",
    "kBrokenBase",
    "kChipBase",
    "kHouseBase",
    "kTechnoBase",
    "kHipHopBase",
    "kFunkSoulBase",
    "kUkGarageBase",
    "kDrumAndBassBase",
    "kLoFiBase",
    "kUkGarage",
    "kDrumAndBass",
    "kFootwork",
    "kPsytrance",
    "kDubTechno",
    "kChicagoJack",
    "kRollingAcid",
    "kClassicTwoStep",
    "kDarkSkippy",
    "kDeepChord",
    "kMinimalSpace",
    "Archetype::StackedQuarters",
    "Archetype::ElectroBackskip",
    "Archetype::FunkHouseBridge",
    "Archetype::ElectroGapPush",
):
    require(token in SELECTION_CPP, f"missing production compatibility data: {token}")

# Stage 14 renamed the old source-local pools so the new top-level Techno name
# could be unambiguous. The persisted pre-Stage14 modes must still resolve to
# the same musical compatibility pools after that rename.
require("case GenerativeMode::Outrun: return view(kSynthwaveBase);" in SELECTION_CPP,
        "persisted Outrun must keep its pre-Stage14 compatibility pool")
require("case GenerativeMode::Darksynth: return view(kDarksynthBase);" in SELECTION_CPP,
        "persisted Darksynth must keep its pre-Stage14 compatibility pool")

require("canonicalize(" in SELECTION_CPP,
        "AUTO selection lacks canonical candidate ordering")
require("GenerationDomain::ArchetypeSelection" in SELECTION_CPP,
        "AUTO selection bypasses the established RNG domain")
for forbidden in ("std::vector", "std::string", "new ", "malloc(", "rand("):
    require(forbidden not in SELECTION_CPP,
            f"selection layer leaked heap/global RNG dependency: {forbidden}")

for forbidden in ("GenreSettings", "GenerativeMode", "GenreManager"):
    require(forbidden not in REFERENCE + REALIZER,
            f"Genre ownership leaked into vocabulary/realizer: {forbidden}")

for token in (
    "rhythmSelectionMode",
    "rhythmArchetypeId",
):
    require(token in SCENES_H, f"Scene lacks persisted user intent: {token}")
for token in ('genreObj["rsm"]', 'genreObj["rid"]', 'lastKey_ == "rsm"',
              'lastKey_ == "rid"'):
    require(token in SCENES_CPP, f"Scene codec lacks rhythm field: {token}")
for token in ('writeLiteral(",\\"rsm\\":")',
              'writeLiteral(",\\"rid\\":")'):
    require(token in SCENES_H, f"streaming Scene writer lacks rhythm field: {token}")
for forbidden in ("generationBackend", "selectionPlan", "phraseOrdinal"):
    require(forbidden not in SCENES_H,
            f"Scene persisted derived composition state: {forbidden}")

for token in (
    '"RHYTHM"',
    '"AUTO"',
    "cycleRhythmSelection",
    "normalizePendingRhythm",
    'UI::showToast("RHYTHM RESET TO AUTO"',
    "settings.rhythmSelectionMode",
    "settings.rhythmArchetypeId",
):
    require(token in GENRE_PAGE, f"GENRE UI reachability missing: {token}")
for forbidden in ("Stage7AAudition", "stage7a_catalog", "AUDITION"):
    require(forbidden not in GENRE_PAGE,
            f"temporary audition UI leaked into production: {forbidden}")

require("test_rhythm_stage7c_selection.cpp" in RUNNER,
        "Stage 7C runner omits reachability/property tests")
require("test_rhythm_stage7c_source_regressions.py" in RUNNER,
        "Stage 7C runner omits source ownership tests")

print("Generation Stage 7C source ownership: OK")
