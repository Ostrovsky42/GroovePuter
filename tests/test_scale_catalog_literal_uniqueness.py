from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CATALOG = ROOT / "src/generation/tonal/scale_catalog.h"

# Canonical interval payloads. Search generated-pitch implementation surfaces,
# not the explicitly separate live PerformanceKeyboard compatibility context.
LITERALS = (
    "0, 2, 3, 5, 7, 8, 10",
    "0, 2, 4, 5, 7, 9, 11",
    "0, 2, 3, 5, 7, 9, 10",
    "0, 1, 3, 5, 7, 8, 10",
    "0, 2, 4, 6, 7, 9, 11",
    "0, 2, 4, 5, 7, 9, 10",
    "0, 1, 3, 5, 6, 8, 10",
    "0, 2, 4, 7, 9",
    "0, 3, 5, 7, 10",
    "0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11",
)

surfaces = [
    ROOT / "src/generation",
    ROOT / "src/dsp/advanced_pattern_generator.cpp",
]

files = []
for surface in surfaces:
    if surface.is_dir():
        files.extend(p for p in surface.rglob("*") if p.suffix in {".h", ".cpp"})
    else:
        files.append(surface)

catalog_text = CATALOG.read_text()
for literal in LITERALS:
    assert literal in catalog_text, literal
    owners = []
    for path in files:
        if literal in path.read_text():
            owners.append(path.relative_to(ROOT).as_posix())
    assert owners == ["src/generation/tonal/scale_catalog.h"], (literal, owners)

print("Stage 15 generation scale interval literals have one owner: OK")
