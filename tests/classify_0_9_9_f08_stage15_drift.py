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


def row_key(row: dict[str, str]) -> tuple[str, str, str]:
    return row["mode"], row["ordinal"], row["voice"]


def changed_columns(expected: dict[str, str], actual: dict[str, str]) -> tuple[str, ...]:
    return tuple(
        column
        for column in ("topology", "pitch", "articulation", "full")
        if expected[column] != actual[column]
    )


def grouped(rows: list[dict[str, str]], changed_keys: set[tuple[str, str, str]], field: str):
    totals: dict[str, int] = defaultdict(int)
    changes: dict[str, int] = defaultdict(int)
    for row in rows:
        value = row[field]
        totals[value] += 1
        if row_key(row) in changed_keys:
            changes[value] += 1
    return [(value, changes[value], totals[value]) for value in sorted(totals)]


def append_group(report: list[str], title: str, values) -> None:
    report.append(f"## {title}")
    report.append("")
    report.append("| value | changed | total |")
    report.append("| --- | ---: | ---: |")
    for value, changed, total in values:
        report.append(f"| {value} | {changed} | {total} |")
    report.append("")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Classify the intentional 0.9.9-F08 Stage15 tonal corpus drift."
    )
    parser.add_argument("expected", type=Path)
    parser.add_argument("actual_review", type=Path)
    parser.add_argument("--expect-changed", type=int, default=93)
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()

    expected_rows = read_tsv(args.expected, BASE_COLUMNS)
    actual_rows = read_tsv(args.actual_review, REVIEW_COLUMNS)
    if len(expected_rows) != len(actual_rows):
        raise AssertionError(
            f"row count changed: expected {len(expected_rows)}, actual {len(actual_rows)}"
        )

    expected_by_key = {row_key(row): row for row in expected_rows}
    actual_by_key = {row_key(row): row for row in actual_rows}
    if expected_by_key.keys() != actual_by_key.keys():
        missing = sorted(expected_by_key.keys() - actual_by_key.keys())
        extra = sorted(actual_by_key.keys() - expected_by_key.keys())
        raise AssertionError(f"row identity changed: missing={missing}, extra={extra}")

    changed: list[tuple[dict[str, str], tuple[str, ...]]] = []
    changed_keys: set[tuple[str, str, str]] = set()
    column_counts = defaultdict(int)
    for key in expected_by_key:
        actual = actual_by_key[key]
        columns = changed_columns(expected_by_key[key], actual)
        if not columns:
            continue
        changed.append((actual, columns))
        changed_keys.add(key)
        for column in columns:
            column_counts[column] += 1

    if len(changed) != args.expect_changed:
        raise AssertionError(
            f"semantic drift moved: {len(changed)} rows changed, expected {args.expect_changed}"
        )
    if column_counts["topology"] != 0:
        raise AssertionError(f"F08 changed topology in {column_counts['topology']} rows")
    if column_counts["articulation"] != 0:
        raise AssertionError(
            f"F08 changed articulation in {column_counts['articulation']} rows"
        )
    if column_counts["pitch"] != len(changed) or column_counts["full"] != len(changed):
        raise AssertionError(
            "F08 drift is no longer pitch-only: "
            f"pitch={column_counts['pitch']} full={column_counts['full']} changed={len(changed)}"
        )

    static_changed = [
        row for row, _ in changed if row["harmonic_class"] == "static"
    ]
    if static_changed:
        identities = ", ".join(
            f"{row['mode']}/{row['ordinal']}/{row['voice']}/{row['progression']}"
            for row in static_changed[:12]
        )
        raise AssertionError(
            f"static harmony changed in {len(static_changed)} rows: {identities}"
        )

    same_clock_changed = [
        row for row, _ in changed if row["clock_relation"] == "same"
    ]
    if same_clock_changed:
        identities = ", ".join(
            f"{row['mode']}/{row['ordinal']}/{row['voice']}"
            for row in same_clock_changed[:12]
        )
        raise AssertionError(
            "pitch changed even though old chord clock and new harmonic clock are identical: "
            + identities
        )

    count_equal_clock_different = sum(
        1
        for row, _ in changed
        if row["old_chord_event_count"] == row["harmonic_event_count"]
        and row["clock_relation"] == "different"
    )
    count_different = sum(
        1
        for row, _ in changed
        if row["old_chord_event_count"] != row["harmonic_event_count"]
    )

    report: list[str] = [
        "# 0.9.9-F08 Stage15 semantic drift classification",
        "",
        f"- Corpus rows: **{len(actual_rows)}**",
        f"- Changed rows: **{len(changed)}**",
        f"- Topology changes: **{column_counts['topology']}**",
        f"- Articulation changes: **{column_counts['articulation']}**",
        f"- Pitch changes: **{column_counts['pitch']}**",
        f"- Full-fingerprint changes: **{column_counts['full']}**",
        f"- Static-harmony changed rows: **{len(static_changed)}**",
        f"- Changed rows with identical old/new clock masks: **{len(same_clock_changed)}**",
        f"- Changed rows with different event counts: **{count_different}**",
        "- Changed rows with equal event counts but different clock positions: "
        f"**{count_equal_clock_different}**",
        "",
    ]

    append_group(report, "By genre", grouped(actual_rows, changed_keys, "mode"))
    append_group(report, "By voice", grouped(actual_rows, changed_keys, "voice"))
    append_group(
        report,
        "By progression",
        grouped(actual_rows, changed_keys, "progression"),
    )
    append_group(
        report,
        "By harmonic class",
        grouped(actual_rows, changed_keys, "harmonic_class"),
    )
    append_group(
        report,
        "By old/new clock relation",
        grouped(actual_rows, changed_keys, "clock_relation"),
    )

    report.append("## Changed rows")
    report.append("")
    report.append(
        "| genre | ordinal | voice | progression | class | old chord clock | "
        "new harmonic clock | old count | new count | changed fields |"
    )
    report.append(
        "| --- | ---: | --- | --- | --- | --- | --- | ---: | ---: | --- |"
    )
    for row, columns in changed:
        report.append(
            f"| {row['mode']} | {row['ordinal']} | {row['voice']} | "
            f"{row['progression']} | {row['harmonic_class']} | "
            f"{row['old_chord_onsets']} | {row['harmonic_onsets']} | "
            f"{row['old_chord_event_count']} | {row['harmonic_event_count']} | "
            f"{', '.join(columns)} |"
        )
    report.append("")

    text = "\n".join(report)
    print(text)
    if args.report is not None:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(text + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
