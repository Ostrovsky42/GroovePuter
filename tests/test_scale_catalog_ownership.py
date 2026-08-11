from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = (ROOT / "src/generation/tonal/scale_catalog.h").read_text()
PROJECTOR = (ROOT / "src/generation/tonal/tonal_projector.cpp").read_text()
ADVANCED = (ROOT / "src/dsp/advanced_pattern_generator.cpp").read_text()
KEYBOARD = (ROOT / "src/input/performance_keyboard.cpp").read_text()

# Stage 15 owns generated pitch only. All generation scale data must come from
# this single data-only catalog.
for name in (
    "kScaleIntervalsMinor",
    "kScaleIntervalsMajor",
    "kScaleIntervalsDorian",
    "kScaleIntervalsPhrygian",
    "kScaleIntervalsLydian",
    "kScaleIntervalsMixolydian",
    "kScaleIntervalsLocrian",
    "kScaleIntervalsPentatonicMajor",
    "kScaleIntervalsPentatonicMinor",
    "kScaleIntervalsChromatic",
):
    assert name in CATALOG, name

assert "scaleDefinitionFor(" in CATALOG
assert "scaleDegreeToSemitone(" in CATALOG
assert "kScaleIntervals" not in PROJECTOR
assert "static const int intervals[][" not in ADVANCED
assert "static const uint8_t intervalCounts[]" not in ADVANCED
assert "scaleDefinitionFor(" in ADVANCED

# PerformanceKeyboard remains an explicitly separate live-input compatibility
# context in this Stage 15 PR. It is not a generated-pitch owner and is kept out
# of the production tonal path rather than accepting a broad input-controller
# rewrite during the musical integration gate.
assert "PerformanceKeyboard::intervalForDegree" in KEYBOARD
assert "materializeTonalIntent(" not in KEYBOARD
assert "projectTonalIntent(" not in KEYBOARD

print("Stage 15 generation ScaleCatalog ownership: OK")
