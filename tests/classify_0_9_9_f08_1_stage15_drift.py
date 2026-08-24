#!/usr/bin/env python3
import argparse
from collections import defaultdict
from pathlib import Path

BASE_COLUMNS = (
    "mode",
    "ordinal",
    "voice",
    "status",
    "secondary_role",
    "topology",
    "pitch",
    "articulation",
    "full",
)
REVIEW_COLUMNS = BASE_COLUMNS + (
    "progression_id",
    "progression",
    "harmonic_class",
    "old_chord_onsets",
    "harmonic_onsets",
    "old_chord_event_count",
    "harmonic_event_count",
    "clock_relation",
)

EXPECTED_POLICY = {
    "STATIC MODAL": ("8000", 1, "static"),
    "PEDAL DRONE": ("8000", 1, "static"),
    "POP CYCLE": ("8888", 4, "moving"),
    "II-V-I": ("8220", 3, "moving"),
    "PARALLEL SHIFT": ("8080", 2, "moving"),
    "MINOR FALL": ("8888", 4, "moving"),
    "BORROWED LIFT": ("8008", 2, "moving"),
}


def read_tsv(path: Path, expected_header: tuple[str, ...]) -> list[dict[str, str]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines:
        raise AssertionError(f"empty TSV: {path}")
    header = tuple(lines[0].split("\t"))
    if header != expected_header:
        raise AssertionError(
            f"unexpected header in {path}: {header!r} != {expected_header!r}"
        )
    rows: list[dict[str, str]] = []
    for line_number, line in enumerate(lines[1:], start=2):
        fields = line.split("\t")
        if len(fields) != len(header):
            raise AssertionError(
                f"{path}:{line_number}: {len(fields)} fields, expected {len(header)}"
            )
        rows.append(dict(zip(header, fields)))
    return rows


def key(row: dict[str, str]) -> tuple[str, str, str]:
    return row["mode"], row["ordinal"], row["voice"]


def bootstrap_clock(row: dict[str, str]) -> str:
    return "8000" if row["harmonic_class"] == "static" else "8080"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Classify F08.1 drift relative to the accepted F08 {0}/{0,8} baseline."
    )
    parser.add_argument("accepted_f08", type=Path)
    parser.add_argument("current_review", type=Path)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    baseline_rows = read_tsv(args.accepted_f08, BASE_COLUMNS)
    current_rows = read_tsv(args.current_review, REVIEW_COLUMNS)
    if len(baseline_rows) != len(current_rows):
        raise AssertionError(
            f"row count changed: {len(baseline_rows)} != {len(current_rows)}"
        )

    baseline = {key(row): row for row in baseline_rows}
    current = {key(row): row for row in current_rows}
    if baseline.keys() != current.keys():
        raise AssertionError("fixed corpus row identity changed")

    represented = set()
    progression_totals = defaultdict(int)
    progression_clock_changes = defaultdict(int)
    progression_pitch_changes = defaultdict(int)
    changed_rows = 0
    pitch_changes = 0
    full_changes = 0
    clock_changed_rows = 0

    for row_key, accepted in baseline.items():
        row = current[row_key]
        if accepted["status"] != row["status"]:
            raise AssertionError(f"status changed for {row_key}")
        if accepted["secondary_role"] != row["secondary_role"]:
            raise AssertionError(f"secondary role changed for {row_key}")

        progression = row["progression"]
        if progression not in EXPECTED_POLICY:
            raise AssertionError(f"unknown progression in corpus: {progression}")
        expected_mask, expected_count, expected_class = EXPECTED_POLICY[progression]
        represented.add(progression)
        progression_totals[progression] += 1

        if row["harmonic_onsets"] != expected_mask:
            raise AssertionError(
                f"unexpected F08.1 clock for {row_key}/{progression}: "
                f"{row['harmonic_onsets']} != {expected_mask}"
            )
        if int(row["harmonic_event_count"]) != expected_count:
            raise AssertionError(
                f"unexpected F08.1 event count for {row_key}/{progression}: "
                f"{row['harmonic_event_count']} != {expected_count}"
            )
        if row["harmonic_class"] != expected_class:
            raise AssertionError(
                f"unexpected harmonic class for {row_key}/{progression}: "
                f"{row['harmonic_class']} != {expected_class}"
            )

        topology_changed = accepted["topology"] != row["topology"]
        pitch_changed = accepted["pitch"] != row["pitch"]
        articulation_changed = accepted["articulation"] != row["articulation"]
        full_changed = accepted["full"] != row["full"]
        any_changed = topology_changed or pitch_changed or articulation_changed or full_changed
        clock_changed = row["harmonic_onsets"] != bootstrap_clock(row)

        if topology_changed:
            raise AssertionError(
                f"F08.1 changed physical topology for {row_key}/{progression}"
            )
        if articulation_changed:
            raise AssertionError(
                f"F08.1 changed physical articulation for {row_key}/{progression}"
            )
        if pitch_changed != full_changed:
            raise AssertionError(
                f"F08.1 drift is not pitch-only for {row_key}/{progression}: "
                f"pitch={pitch_changed} full={full_changed}"
            )
        if any_changed and not clock_changed:
            raise AssertionError(
                f"output changed without a harmonic-clock change for {row_key}/{progression}"
            )

        if any_changed:
            changed_rows += 1
        if pitch_changed:
            pitch_changes += 1
            progression_pitch_changes[progression] += 1
        if full_changed:
            full_changes += 1
        if clock_changed:
            clock_changed_rows += 1
            progression_clock_changes[progression] += 1

    missing = set(EXPECTED_POLICY) - represented
    if missing:
        raise AssertionError(f"fixed corpus lost progression coverage: {sorted(missing)}")

    moving_clocks = {
        mask
        for progression, (mask, _count, harmonic_class) in EXPECTED_POLICY.items()
        if harmonic_class == "moving"
    }
    if moving_clocks != {"8080", "8888", "8220", "8008"}:
        raise AssertionError(f"unexpected bounded moving vocabulary: {sorted(moving_clocks)}")
    if EXPECTED_POLICY["POP CYCLE"][0] != EXPECTED_POLICY["MINOR FALL"][0]:
        raise AssertionError("semantic clock sharing was lost")
    if changed_rows == 0 or pitch_changes == 0 or clock_changed_rows == 0:
        raise AssertionError(
            "F08.1 corpus did not exercise the new harmonic-clock policy"
        )

    report = [
        "# 0.9.9-F08.1 Stage15 causal classification",
        "",
        f"- Corpus rows: **{len(current_rows)}**",
        f"- Harmonic-clock-changed rows vs accepted F08 bootstrap: **{clock_changed_rows}**",
        f"- Output-changed rows: **{changed_rows}**",
        "- Physical topology changes: **0**",
        "- Physical articulation changes: **0**",
        f"- Pitch changes: **{pitch_changes}**",
        f"- Full-fingerprint changes: **{full_changes}**",
        "",
        "Clock changes are progression-semantic. Genre, BPM, and ChordRhythm "
        "articulation are not HarmonicRhythm selectors.",
        "",
        "## Progression classes",
        "",
        "| progression | clock | events | corpus rows | clock changed | pitch changed |",
        "| --- | --- | ---: | ---: | ---: | ---: |",
    ]
    for progression in EXPECTED_POLICY:
        mask, event_count, _ = EXPECTED_POLICY[progression]
        report.append(
            f"| {progression} | `{mask}` | {event_count} | "
            f"{progression_totals[progression]} | "
            f"{progression_clock_changes[progression]} | "
            f"{progression_pitch_changes[progression]} |"
        )
    report.extend(
        [
            "",
            "## Causal gate",
            "",
            "A clock change may alter harmonic timing and therefore pitch, but this "
            "checkpoint rejects any change to physical synth topology or articulation. "
            "Rows whose F08.1 clock is identical to the accepted F08 bootstrap must "
            "remain bit-identical in all four fingerprints.",
            "",
        ]
    )

    text = "\n".join(report)
    print(text)
    if args.report is not None:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(text + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
