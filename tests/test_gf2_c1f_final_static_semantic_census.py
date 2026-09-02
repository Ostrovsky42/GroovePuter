#!/usr/bin/env python3
import csv
import hashlib
import subprocess
from collections import defaultdict
from pathlib import Path

from gf2_frozen_git_boundary import resolve_frozen_commit


ROOT = Path(__file__).resolve().parents[1]
PROFILE_PATH = ROOT / "docs/research/GF2_C1F_FINAL_STATIC_SEMANTIC_CENSUS.tsv"
ARCHETYPE_PATH = ROOT / "docs/research/GF2_C1F_RHYTHM_ARCHETYPE_PAYLOADS.tsv"
PAIR_PATH = ROOT / "docs/research/GF2_C1F_BASE_GENRE_PAIRS.tsv"
BASE_SHA = "0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d"
C1F_HEAD = "d24ebf42ba48c50d2057af055807dd2c1ec6f096"
C1F_TREE = "e81d84b5fa23315f287cc308cb0df6e6d3fbce62"
C1F_SUBJECT = "research(0.9.10-gf2): revalidate Gate A on v0.9.9"
HISTORICAL_NORMALIZED_HASHES = {
    PROFILE_PATH: "32a601025718bf6a4768aa706620e668febc2508a36c73d952f779539433a700",
    PAIR_PATH: "3e82efd8e4a9d26e419e9dfeeec907acbfb45d3a0d1adb41df24e71dbbfe8667",
    ARCHETYPE_PATH: "0615f246c26d8a46a2b840002d40974cc3116c05e56b205f94e5b903ed4e1367",
}


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


profiles = rows(PROFILE_PATH)
archetypes = rows(ARCHETYPE_PATH)
pairs = rows(PAIR_PATH)
C1F_BOUNDARY = resolve_frozen_commit(
    ROOT,
    exact_sha=C1F_HEAD,
    expected_tree=C1F_TREE,
    expected_subject=C1F_SUBJECT,
)

subprocess.run(
    ["git", "merge-base", "--is-ancestor", C1F_BOUNDARY, "HEAD"],
    cwd=ROOT,
    check=True,
)
changed_paths = subprocess.run(
    ["git", "diff", "--name-only", BASE_SHA, C1F_BOUNDARY, "--"],
    cwd=ROOT,
    check=True,
    text=True,
    capture_output=True,
).stdout.splitlines()
assert changed_paths
assert all(
    path.startswith(("docs/research/", "tests/", "tools/gf2/"))
    for path in changed_paths
), changed_paths

for artifact_path, expected_hash in HISTORICAL_NORMALIZED_HASHES.items():
    payload = artifact_path.read_text(encoding="utf-8").replace(
        BASE_SHA, "<BASE>"
    )
    assert hashlib.sha256(payload.encode("utf-8")).hexdigest() == expected_hash

assert len(profiles) == 33
assert len(archetypes) == 24
assert len(pairs) == 120
assert all(row["exact_base"] == BASE_SHA for row in profiles)
assert all(row["exact_base"] == BASE_SHA for row in pairs)

bases = [row for row in profiles if row["is_base"] == "1"]
recipes = [row for row in profiles if row["is_base"] == "0"]
assert len(bases) == 16
assert len(recipes) == 17
assert all(row["recipe_id"] == "0" for row in bases)
assert all(row["classification_vs_base"] == "BASE" for row in bases)
assert all(row["strong_rhythm_route"] not in {"Legacy", "INVALID"}
           for row in profiles)
assert all("MULTI-DOMAIN" in row["classification_vs_base"]
           for row in recipes)
assert sum("MEMBERSHIP-CHANGE" in row["classification_vs_base"]
           for row in recipes) == 16
assert all("DRUM" in row["changed_domains_vs_base"] for row in recipes)

atlas_rows = [row for row in profiles if row["atlas_backed"] == "1"]
assert {int(row["recipe_id"]) for row in atlas_rows} == {6, 7, 8, 9, 10, 11}
assert all(row["runtime_trace_changes_vs_base"] == "ATLAS_METADATA"
           for row in atlas_rows)
assert all(row["atlas_display_name"] == row["recipe_name"]
           for row in atlas_rows)
outside_corridor = [row for row in atlas_rows
                    if row["atlas_corridor_relation"] == "OUTSIDE_CORRIDOR"]
assert len(outside_corridor) == 1
assert outside_corridor[0]["genre_key"] == "Reggae"
assert outside_corridor[0]["recipe_id"] == "11"
assert outside_corridor[0]["atlas_bpm"] == "116"

rows_by_genre: dict[str, list[dict[str, str]]] = defaultdict(list)
for row in profiles:
    rows_by_genre[row["genre_key"]].append(row)
for genre_rows in rows_by_genre.values():
    ids = [int(row["recipe_id"]) for row in genre_rows]
    assert ids == sorted(ids)
    assert ids[0] == 0

assert all(row["tonal_source"] == "kStaticProfile" for row in bases
           if row["genre_key"] in {"Rave", "House", "Techno"})
assert all(row["tonal_source"] == "kSlowProfile" for row in bases
           if row["genre_key"] in
           {"TripHop", "HipHop", "FunkSoul", "LoFi"})

for row in profiles:
    assert "tonal.bass_articulation=PLAIN" in row["single_option_axes"]
    assert "tonal.melodic_rhythm_operation=PRESERVE" in row[
        "single_option_axes"
    ]
    assert "tonal.melodic_motif_operation=NONE" in row[
        "single_option_axes"
    ]

assert not any(
    row["classification"] == "EXACT_DECLARATIVE_COLLISION" for row in pairs
)
assert any(row["classification"] == "PARTIAL_COLLISION" for row in pairs)
assert any(row["classification"] == "STRONG_DISTINCTNESS" for row in pairs)
assert sum(row["classification"] == "PARTIAL_COLLISION"
           for row in pairs) == 19
assert sum(row["classification"] == "STRONG_DISTINCTNESS"
           for row in pairs) == 101

assert sum("composition.rhythms=" in row["single_option_axes"]
           for row in profiles) == 1
deep_chord = next(row for row in profiles
                  if row["genre_key"] == "Reggae" and
                  row["recipe_id"] == "10")
assert "composition.rhythms=chord_response" in deep_chord[
    "single_option_axes"
]

assert (ROOT / "tests/data/stage15_tonal_enabled_f13_baseline.tsv.gz.b64").is_file()

print("GF2-C1F final-release census invariants passed")
