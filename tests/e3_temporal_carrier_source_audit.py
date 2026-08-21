#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def production_files():
    roots = [ROOT / "src", ROOT / "scenes.h", ROOT / "scenes.cpp"]
    files = []
    for root in roots:
        if root.is_file():
            files.append(root)
            continue
        for path in root.rglob("*"):
            if path.suffix in {".h", ".hpp", ".cpp", ".cc", ".c"}:
                files.append(path)
    return sorted(set(files))


def print_hits(title, paths, pattern):
    rx = re.compile(pattern)
    print(f"E3_AUDIT {title}")
    count = 0
    for path in paths:
        lines = path.read_text(encoding="utf-8").splitlines()
        for lineno, line in enumerate(lines, 1):
            if rx.search(line):
                rel = path.relative_to(ROOT)
                print(f"{rel}:{lineno}: {line.strip()}")
                count += 1
    print(f"E3_AUDIT {title}_COUNT={count}")
    return count


miniacid = ROOT / "src/dsp/miniacid_engine.cpp"
miniacid_h = ROOT / "src/dsp/miniacid_engine.h"
song_boundary_h = ROOT / "src/dsp/song_cycle_boundary.h"
all_prod = production_files()

song_hits = print_hits(
    "songBarIndex_all_sites", [miniacid_h, miniacid], r"\bsongBarIndex_\b"
)
write_hits = print_hits(
    "songBarIndex_direct_writes", [miniacid], r"\bsongBarIndex_\s*(?:=|\+\+|--|\+=|-=)"
)
pattern_bars_hits = print_hits(
    "feel_patternBars_production_consumers", all_prod, r"\bpatternBars\b"
)
boundary_hits = print_hits(
    "song_row_duration_boundary",
    [song_boundary_h, miniacid],
    r"nextSongCycleBoundary|normalizedSongPatternBars|cycleBarCount|advanceSongBar_",
)

role_paths = [
    ROOT / "src/generation/roles/bass_rhythm.cpp",
    ROOT / "src/generation/roles/chord_rhythm.cpp",
    ROOT / "src/generation/roles/melodic_motif.cpp",
]
role_hits = print_hits(
    "semantic_bar_sensitive_roles",
    role_paths,
    r"SparseAnchor|SustainAndDrop|DubChordSpace|SparseCall|RestHeavy|barOrdinal",
)

if song_hits == 0 or write_hits == 0:
    raise SystemExit("E3 audit failed: songBarIndex_ sites were not found")
if pattern_bars_hits == 0:
    raise SystemExit("E3 audit failed: patternBars production consumers were not found")
if boundary_hits == 0:
    raise SystemExit("E3 audit failed: Song row duration boundary sites were not found")
if role_hits == 0:
    raise SystemExit("E3 audit failed: semantic bar-sensitive roles were not found")
