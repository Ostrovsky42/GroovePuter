#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import zipfile
from collections import defaultdict
from pathlib import Path

import extract_atlas_pass2 as core

MIN_ACTIVE_STRUCTURAL_GROUPS = 5
MIN_ABSENCE_FRACTION = 0.90
ROLES = core.DRUM_ROLES


def rows(zf: zipfile.ZipFile, root: str, rel: str) -> list[dict[str, str]]:
    return list(csv.DictReader(io.StringIO(zf.read(root + rel).decode("utf-8-sig"))))


def extract_negative_space(atlas_zip: Path, output_csv: Path) -> list[dict[str, object]]:
    digest = hashlib.sha256(atlas_zip.read_bytes()).hexdigest()
    if digest != core.EXPECTED_ATLAS_SHA256:
        raise ValueError(f"unexpected Atlas archive SHA-256: {digest}")

    with zipfile.ZipFile(atlas_zip) as zf:
        roots = {name.split("/", 1)[0] for name in zf.namelist() if "/" in name}
        if len(roots) != 1:
            raise ValueError("Atlas ZIP must have exactly one root")
        root = next(iter(roots)) + "/"
        patterns = rows(zf, root, "core/patterns.csv")
        tracks = rows(zf, root, "core/pattern_tracks.csv")
        events = rows(zf, root, "core/pattern_events.csv")
        direction_links = rows(zf, root, "core/pattern_direction_links.csv")

    track_by_pid = defaultdict(list)
    events_by_key = defaultdict(list)
    directions_by_pid = defaultdict(list)
    for track in tracks:
        track_by_pid[track["pattern_id"]].append(track)
    for event in events:
        events_by_key[(event["pattern_id"], event["track_id"])].append(event)
    for link in direction_links:
        directions_by_pid[link["pattern_id"]].append(link["direction_id"])

    eligible = [
        pattern for pattern in patterns
        if pattern["pattern_kind"] == "SOURCE_OBSERVATION"
        and pattern["bars"] == "1"
        and pattern["steps_per_bar"] == "16"
        and pattern["publication_status"] != "SUPERSEDED"
    ]
    masks = {
        pattern["pattern_id"]: core.pattern_masks(
            pattern["pattern_id"], 1, 16, track_by_pid, events_by_key
        )[0]
        for pattern in eligible
    }

    # One observation per (direction, structural group). A pattern linked to
    # multiple directions can contribute to each direction, but duplicates
    # inside one direction are collapsed before occupancy statistics.
    by_direction_group: dict[tuple[str, str], str] = {}
    for pattern in eligible:
        pid = pattern["pattern_id"]
        for direction in directions_by_pid[pid]:
            key = (direction, pattern["structural_group_id"])
            by_direction_group.setdefault(key, pid)

    grouped = defaultdict(list)
    for (direction, _group_id), pid in by_direction_group.items():
        grouped[direction].append(masks[pid])

    out = []
    for direction, observations in sorted(grouped.items()):
        for role in ROLES:
            # The denominator includes only structural groups where the role is
            # active somewhere. A missing role is not evidence of protected space.
            active = [obs[role] for obs in observations if obs[role]]
            if len(active) < MIN_ACTIVE_STRUCTURAL_GROUPS:
                continue
            for step in range(16):
                absence = sum(step not in onset_set for onset_set in active) / len(active)
                if absence >= MIN_ABSENCE_FRACTION:
                    out.append({
                        "direction": direction,
                        "role": core.ROLE_NAMES[role],
                        "step": step,
                        "active_structural_group_count": len(active),
                        "absence_fraction": round(absence, 6),
                        "evidence_class": "RESEARCH_AGGREGATE",
                    })

    output_csv.parent.mkdir(parents=True, exist_ok=True)
    core.write_csv(
        output_csv,
        out,
        [
            "direction",
            "role",
            "step",
            "active_structural_group_count",
            "absence_fraction",
            "evidence_class",
        ],
    )
    return out


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("atlas_zip", type=Path)
    parser.add_argument("output_csv", type=Path)
    args = parser.parse_args()
    rows_ = extract_negative_space(args.atlas_zip, args.output_csv)
    print(f"Atlas Pass 2 negative-space rows: {len(rows_)}")


if __name__ == "__main__":
    main()
