from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/dsp/advanced_pattern_generator.cpp").read_text()
SCENES = (ROOT / "scenes.h").read_text()

assert "scale % 7" not in SOURCE
assert "static const int intervals[][12]" in SOURCE
assert "static const uint8_t intervalCounts[] = {7, 7, 7, 7, 7, 7, 7, 5, 5, 12};" in SOURCE
assert "{0, 2, 4, 7, 9, 0, 0, 0, 0, 0, 0, 0}" in SOURCE
assert "{0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0, 0}" in SOURCE
assert "{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}" in SOURCE
assert "PENTATONIC_MJ == 7" in SOURCE
assert "PENTATONIC_MN == 8" in SOURCE
assert "CHROMATIC == 9" in SOURCE
assert "for (uint8_t i = 0; i < intervalCount; i++)" in SOURCE

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
