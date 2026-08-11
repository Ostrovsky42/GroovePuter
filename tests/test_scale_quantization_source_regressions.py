from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/dsp/advanced_pattern_generator.cpp").read_text()
CATALOG = (ROOT / "src/generation/tonal/scale_catalog.h").read_text()
SCENES = (ROOT / "scenes.h").read_text()

assert "scale % 7" not in SOURCE
assert '#include "src/generation/tonal/scale_catalog.h"' in SOURCE
assert "static const int intervals[][12]" not in SOURCE
assert "static const uint8_t intervalCounts[]" not in SOURCE
assert "GroovePuterRhythm::scaleDefinitionFor(" in SOURCE
assert "definition.count" in SOURCE
assert "definition.intervals[i]" in SOURCE
assert "PENTATONIC_MJ == 7" in SOURCE
assert "PENTATONIC_MN == 8" in SOURCE
assert "CHROMATIC == 9" in SOURCE

assert "kScaleIntervalsPentatonicMajor[] = {0, 2, 4, 7, 9}" in CATALOG
assert "kScaleIntervalsPentatonicMinor[] = {0, 3, 5, 7, 10}" in CATALOG
assert "kScaleIntervalsChromatic[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}" in CATALOG
assert "case kScalePentatonicMajor: return {kScaleIntervalsPentatonicMajor, 5};" in CATALOG
assert "case kScalePentatonicMinor: return {kScaleIntervalsPentatonicMinor, 5};" in CATALOG
assert "case kScaleChromatic: return {kScaleIntervalsChromatic, 12};" in CATALOG

expected_enum = """enum ScaleType {
    MINOR,
    MAJOR,
    DORIAN,
    PHRYGIAN,
    LYDIAN,
    MIXOLYDIAN,
    LOCRIAN,
    PENTATONIC_MJ,   // Major pentatonic
    PENTATONIC_MN,   // Minor pentatonic
    CHROMATIC        // All 12 notes
};"""
assert expected_enum in SCENES

print("Scale quantization source regressions: OK")
