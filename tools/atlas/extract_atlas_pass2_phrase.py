#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import hashlib
import io
import json
import statistics
import zipfile
from collections import Counter, defaultdict
from pathlib import Path

import extract_atlas_pass2 as core


def rows(zf: zipfile.ZipFile, root: str, rel: str) -> list[dict[str, str]]:
    return list(csv.DictReader(io.StringIO(zf.read(root + rel).decode("utf-8-sig"))))


def phrase_track_role(track: dict[str, str]) -> int | None:
    tid = track["track_id"].upper()
    role = track["track_role"].lower()
    if tid == "KICK":
        return 0
    if tid in {"SNARE", "CLAP", "HAND_CLAP"}:
        return 1
    if tid in {"CLOSED_HAT", "HAT1", "MID_HAT", "HAT_FOOT"}:
        return 2
    if tid in {"OPEN_HAT", "HAT2", "RIDE"}:
        return 3
    if role == "bass":
        return 5
    if role == "harmony":
        return 6
    if role == "melody":
        return 7
    if role in {"annotation", "sample"} or "unknown" in role:
        return None
    if role in {"drums", "drum_or_percussion", "percussion"}:
        return 4
    return None


def phrase_masks(pid: str, bars: int, steps_per_bar: int, track_by_pid, events_by_key):
    if steps_per_bar not in (8, 16):
        raise ValueError(f"unsupported steps_per_bar: {steps_per_bar}")
    result = [{role: set() for role in range(8)} for _ in range(bars)]
    for track in track_by_pid[pid]:
        role = phrase_track_role(track)
        if role is None:
            continue
        for event in events_by_key[(pid, track["track_id"])]:
            bar = int(event.get("bar_index") or "1") - 1
            source_step = int(event["step_index"]) - 1
            step = source_step if steps_per_bar == 16 else source_step * 2
            if 0 <= bar < bars and 0 <= step < 16:
                result[bar][role].add(step)
    return result


def extract_phrase(atlas_zip: Path, output_csv: Path, summary_path: Path) -> dict[str, int]:
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

    track_by_pid = defaultdict(list)
    events_by_key = defaultdict(list)
    for track in tracks:
        track_by_pid[track["pattern_id"]].append(track)
    for event in events:
        events_by_key[(event["pattern_id"], event["track_id"])].append(event)

    measured_patterns = [
        pattern for pattern in patterns
        if pattern["pattern_kind"] == "SOURCE_OBSERVATION"
        and pattern["bars"] in {"2", "4"}
        and pattern["steps_per_bar"] in {"8", "16"}
    ]
    measured = []
    for pattern in measured_patterns:
        bars = phrase_masks(
            pattern["pattern_id"],
            int(pattern["bars"]),
            int(pattern["steps_per_bar"]),
            track_by_pid,
            events_by_key,
        )
        for left, right in zip(bars, bars[1:]):
            measured.append(core.transition_features(left, right))

    derived_patterns = [
        pattern for pattern in patterns
        if pattern["pattern_kind"] == "COMPOSITE_SEQUENCE"
        and pattern["bars"] == "4"
        and pattern["steps_per_bar"] == "16"
    ]
    derived_sequences = Counter()
    for pattern in derived_patterns:
        bars = phrase_masks(pattern["pattern_id"], 4, 16, track_by_pid, events_by_key)
        signature = " > ".join(
            core.transition_features(left, right)["transition_class"]
            for left, right in zip(bars, bars[1:])
        )
        derived_sequences[signature] += 1

    out = []
    class_counts = Counter(row["transition_class"] for row in measured)
    for transition_class in ("EXACT_REPEAT", "ADD_ONLY", "DROP_ONLY", "MIXED"):
        matching = [row for row in measured if row["transition_class"] == transition_class]
        out.append({
            "evidence_class": "MEASURED",
            "scope": "two_bar_source_observation_transition",
            "transition_signature": transition_class,
            "count": len(matching),
            "median_adds": round(statistics.median([row["adds"] for row in matching]), 3) if matching else 0,
            "median_drops": round(statistics.median([row["drops"] for row in matching]), 3) if matching else 0,
        })
    for signature, count in sorted(derived_sequences.items()):
        out.append({
            "evidence_class": "EDITORIAL_CURATED",
            "scope": "four_bar_derived_composite_sequence",
            "transition_signature": signature,
            "count": count,
            "median_adds": "",
            "median_drops": "",
        })

    core.write_csv(
        output_csv,
        out,
        [
            "evidence_class", "scope", "transition_signature", "count",
            "median_adds", "median_drops",
        ],
    )

    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    summary["measured_phrase_patterns"] = len(measured_patterns)
    summary["measured_phrase_transition_counts"] = dict(class_counts)
    summary["derived_four_bar_patterns"] = len(derived_patterns)
    summary_path.write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )
    return dict(class_counts)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("atlas_zip", type=Path)
    parser.add_argument("output_csv", type=Path)
    parser.add_argument("summary_json", type=Path)
    args = parser.parse_args()
    counts = extract_phrase(args.atlas_zip, args.output_csv, args.summary_json)
    print(json.dumps(counts, sort_keys=True))


if __name__ == "__main__":
    main()
