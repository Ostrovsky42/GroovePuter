#!/usr/bin/env python3
"""G4-C0R5 deterministic role-coherence census.

Research-only analyzer. It owns no musical policy: it classifies exact observed
planning/audible onset masks and aggregates production-native bass membership.
"""

from __future__ import annotations

import argparse
import csv
from collections import Counter, defaultdict
from pathlib import Path
from typing import Dict, Iterable, List, Mapping, Sequence, Tuple


EXPECTED_PROFILES = {
    "0": "Acid/BASE",
    "9": "Dub/Reggae/Dub Techno",
    "20": "House/BASE",
    "27": "Drum&Bass/BASE",
}

REQUIRED_FIELDS = {
    "profile_ordinal",
    "profile_id",
    "depth",
    "identity_ordinal",
    "generation_attempt_ordinal",
    "pattern_address",
    "migration_status",
    "selected_archetype",
    "selected_bass_rhythm",
    "bass_attack_mask",
    "rhythm_family",
    "bass_native_candidate_mask",
    "bass_native_membership",
    "planning_bass_onset_mask",
}

TOPOLOGY_CLASSES = (
    "EXACT_MATCH",
    "PARTIAL_OVERLAP",
    "DISJOINT_NONEMPTY",
    "PLANNING_EMPTY_AUDIBLE_NONEMPTY",
    "AUDIBLE_EMPTY_PLANNING_NONEMPTY",
    "BOTH_EMPTY",
)

SUMMARY_FIELDS = (
    "profile_ordinal",
    "profile_id",
    "rows",
    "native_count",
    "outside_native_count",
    "exact_match_count",
    "partial_overlap_count",
    "disjoint_nonempty_count",
    "planning_empty_audible_nonempty_count",
    "audible_empty_planning_nonempty_count",
    "both_empty_count",
    "unique_planning_masks",
    "unique_audible_attack_masks",
    "unique_archetypes",
    "unique_rhythm_families",
)


class CensusError(RuntimeError):
    pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument("--rows-output", required=True, type=Path)
    parser.add_argument("--summary-output", required=True, type=Path)
    return parser.parse_args()


def parse_mask(text: str, field: str, row_number: int) -> int:
    if text in ("", "NOT_OBSERVED", "INVALID"):
        raise CensusError(f"row {row_number}: {field} is not observed: {text!r}")
    try:
        value = int(text, 16)
    except ValueError as exc:
        raise CensusError(
            f"row {row_number}: invalid hexadecimal {field}: {text!r}"
        ) from exc
    if not 0 <= value <= 0xFFFF:
        raise CensusError(f"row {row_number}: {field} outside 16-bit mask: {text!r}")
    return value


def classify_topology(planning: int, audible: int) -> str:
    if planning == 0 and audible == 0:
        return "BOTH_EMPTY"
    if planning == audible:
        return "EXACT_MATCH"
    if planning == 0:
        return "PLANNING_EMPTY_AUDIBLE_NONEMPTY"
    if audible == 0:
        return "AUDIBLE_EMPTY_PLANNING_NONEMPTY"
    if (planning & audible) == 0:
        return "DISJOINT_NONEMPTY"
    return "PARTIAL_OVERLAP"


def canonical_mask(value: int) -> str:
    return f"0x{value:04x}"


def load_rows(path: Path) -> Tuple[List[Dict[str, str]], List[str]]:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.DictReader(handle, delimiter="\t")
            if reader.fieldnames is None:
                raise CensusError("input has no header")
            missing = REQUIRED_FIELDS - set(reader.fieldnames)
            if missing:
                raise CensusError(f"input missing fields: {sorted(missing)}")
            rows = [dict(row) for row in reader]
            return rows, list(reader.fieldnames)
    except OSError as exc:
        raise CensusError(f"cannot read {path}: {exc}") from exc


def validate_and_classify(rows: List[Dict[str, str]]) -> None:
    if len(rows) != 512:
        raise CensusError(f"expected 512 rows, got {len(rows)}")

    seen = set()
    expected = {
        (profile, str(identity))
        for profile in EXPECTED_PROFILES
        for identity in range(128)
    }

    for row_number, row in enumerate(rows, start=2):
        profile = row["profile_ordinal"]
        identity = row["identity_ordinal"]
        coordinate = (profile, identity)
        if coordinate in seen:
            raise CensusError(f"row {row_number}: duplicate coordinate {coordinate}")
        seen.add(coordinate)

        if profile not in EXPECTED_PROFILES:
            raise CensusError(f"row {row_number}: unexpected profile {profile!r}")
        if row["profile_id"] != EXPECTED_PROFILES[profile]:
            raise CensusError(
                f"row {row_number}: profile binding mismatch: "
                f"{profile!r} -> {row['profile_id']!r}"
            )
        try:
            identity_number = int(identity, 10)
        except ValueError as exc:
            raise CensusError(
                f"row {row_number}: invalid identity ordinal {identity!r}"
            ) from exc
        if not 0 <= identity_number < 128:
            raise CensusError(
                f"row {row_number}: identity outside 0..127: {identity_number}"
            )
        if row["depth"] != "P1":
            raise CensusError(f"row {row_number}: depth must be P1")
        if row["generation_attempt_ordinal"] != "0":
            raise CensusError(f"row {row_number}: attempt must be 0")
        if row["pattern_address"] != "23":
            raise CensusError(f"row {row_number}: pattern address must be 23")
        if row["migration_status"] != "APPLIED":
            raise CensusError(
                f"row {row_number}: migration not APPLIED: {row['migration_status']!r}"
            )
        if row["bass_native_membership"] not in {
            "NATIVE",
            "OUTSIDE_NATIVE_SET",
        }:
            raise CensusError(
                f"row {row_number}: invalid native membership "
                f"{row['bass_native_membership']!r}"
            )
        if row["rhythm_family"] in ("", "NOT_OBSERVED", "INVALID"):
            raise CensusError(f"row {row_number}: rhythm family not observed")
        if row["selected_archetype"] in ("", "NOT_OBSERVED", "INVALID"):
            raise CensusError(f"row {row_number}: archetype not observed")
        if row["selected_bass_rhythm"] in ("", "NOT_OBSERVED", "INVALID"):
            raise CensusError(f"row {row_number}: bass rhythm not observed")

        native_mask = parse_mask(
            row["bass_native_candidate_mask"],
            "bass_native_candidate_mask",
            row_number,
        )
        if native_mask == 0:
            raise CensusError(f"row {row_number}: empty native candidate mask")
        planning = parse_mask(
            row["planning_bass_onset_mask"], "planning_bass_onset_mask", row_number
        )
        audible = parse_mask(row["bass_attack_mask"], "bass_attack_mask", row_number)

        row["planning_bass_onset_mask"] = canonical_mask(planning)
        row["bass_attack_mask"] = canonical_mask(audible)
        row["bass_native_candidate_mask"] = canonical_mask(native_mask)
        row["planning_vs_audible"] = classify_topology(planning, audible)
        row["planning_audible_overlap_mask"] = canonical_mask(planning & audible)

    if seen != expected:
        missing = sorted(expected - seen)
        extra = sorted(seen - expected)
        raise CensusError(
            f"coordinate set mismatch missing={missing[:8]} extra={extra[:8]}"
        )


def sorted_rows(rows: Iterable[Dict[str, str]]) -> List[Dict[str, str]]:
    return sorted(
        rows,
        key=lambda row: (
            int(row["profile_ordinal"], 10),
            int(row["identity_ordinal"], 10),
        ),
    )


def summarize(rows: Sequence[Dict[str, str]]) -> List[Dict[str, str]]:
    groups: Dict[Tuple[int, str], List[Dict[str, str]]] = defaultdict(list)
    for row in rows:
        groups[(int(row["profile_ordinal"], 10), row["profile_id"])].append(row)

    summaries: List[Dict[str, str]] = []
    for (profile_ordinal, profile_id), group in sorted(groups.items()):
        if len(group) != 128:
            raise CensusError(
                f"{profile_id}: expected 128 rows, got {len(group)}"
            )
        native = Counter(row["bass_native_membership"] for row in group)
        topology = Counter(row["planning_vs_audible"] for row in group)
        unknown = set(topology) - set(TOPOLOGY_CLASSES)
        if unknown:
            raise CensusError(f"{profile_id}: unknown topology classes {sorted(unknown)}")

        summaries.append(
            {
                "profile_ordinal": str(profile_ordinal),
                "profile_id": profile_id,
                "rows": str(len(group)),
                "native_count": str(native["NATIVE"]),
                "outside_native_count": str(native["OUTSIDE_NATIVE_SET"]),
                "exact_match_count": str(topology["EXACT_MATCH"]),
                "partial_overlap_count": str(topology["PARTIAL_OVERLAP"]),
                "disjoint_nonempty_count": str(topology["DISJOINT_NONEMPTY"]),
                "planning_empty_audible_nonempty_count": str(
                    topology["PLANNING_EMPTY_AUDIBLE_NONEMPTY"]
                ),
                "audible_empty_planning_nonempty_count": str(
                    topology["AUDIBLE_EMPTY_PLANNING_NONEMPTY"]
                ),
                "both_empty_count": str(topology["BOTH_EMPTY"]),
                "unique_planning_masks": str(
                    len({row["planning_bass_onset_mask"] for row in group})
                ),
                "unique_audible_attack_masks": str(
                    len({row["bass_attack_mask"] for row in group})
                ),
                "unique_archetypes": str(
                    len({row["selected_archetype"] for row in group})
                ),
                "unique_rhythm_families": str(
                    len({row["rhythm_family"] for row in group})
                ),
            }
        )
    return summaries


def write_tsv(path: Path, rows: Sequence[Mapping[str, str]], fields: Sequence[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        with path.open("w", newline="", encoding="utf-8") as handle:
            writer = csv.DictWriter(
                handle,
                fieldnames=list(fields),
                delimiter="\t",
                lineterminator="\n",
                extrasaction="ignore",
            )
            writer.writeheader()
            writer.writerows(rows)
    except OSError as exc:
        raise CensusError(f"cannot write {path}: {exc}") from exc


def main() -> int:
    args = parse_args()
    try:
        rows, input_fields = load_rows(args.input)
        validate_and_classify(rows)
        rows = sorted_rows(rows)
        summaries = summarize(rows)

        detail_fields = input_fields + [
            "planning_vs_audible",
            "planning_audible_overlap_mask",
        ]
        write_tsv(args.rows_output, rows, detail_fields)
        write_tsv(args.summary_output, summaries, SUMMARY_FIELDS)

        for row in summaries:
            print(
                "C0R5",
                row["profile_id"],
                f"native={row['native_count']}/128",
                f"outside={row['outside_native_count']}/128",
                f"exact={row['exact_match_count']}",
                f"partial={row['partial_overlap_count']}",
                f"disjoint={row['disjoint_nonempty_count']}",
                "planning_empty="
                f"{row['planning_empty_audible_nonempty_count']}",
                "audible_empty="
                f"{row['audible_empty_planning_nonempty_count']}",
                f"both_empty={row['both_empty_count']}",
            )
        return 0
    except CensusError as exc:
        print(f"G4_C0R5_ANALYSIS_FAIL reason={exc}", file=__import__("sys").stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
