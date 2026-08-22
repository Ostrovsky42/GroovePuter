#!/usr/bin/env python3
import argparse
from collections import Counter, defaultdict
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

EXPECTED_DIRECTION = {"increased": 46, "decreased": 16, "same-count/different-position": 31}
EXPECTED_OLD_COUNT_HISTOGRAM = {0: 19, 1: 27, 2: 31, 3: 6, 4: 10}
EXPECTED_ZERO_TO_TWO = 19

LISTENING_CASES = (
    ("ADDED HARMONIC ACTIVITY", "DrumAndBass", "5", "B", "MINOR FALL", "0000", "8080"),
    ("ADDED HARMONIC ACTIVITY", "TripHop", "4", "A", "II-V-I", "0000", "8080"),
    ("ADDED HARMONIC ACTIVITY", "House", "4", "A", "POP CYCLE", "0000", "8080"),
    ("REDUCED HARMONIC ACTIVITY", "House", "5", "B", "POP CYCLE", "4904", "8080"),
    ("REDUCED HARMONIC ACTIVITY", "Outrun", "0", "B", "POP CYCLE", "2448", "8080"),
    ("SAME COUNT / DIFFERENT TIMING", "UkGarage", "1", "B", "BORROWED LIFT", "0101", "8080"),
    ("SAME COUNT / DIFFERENT TIMING", "FunkSoul", "6", "B", "BORROWED LIFT", "0802", "8080"),
    ("CHORD-ORIENTED", "TripHop", "2", "B", "PARALLEL SHIFT", "0902", "8080"),
)

CONTROL_CASES = (
    ("STATIC NEGATIVE CONTROL", "Acid", "2", "B", "STATIC MODAL"),
    ("STATIC NEGATIVE CONTROL", "Techno", "4", "B", "PEDAL DRONE"),
    ("MOVING SENSITIVITY CONTROL", "Reggae", "4", "B", "BORROWED LIFT"),
)

SECONDARY_ROLE = {
    "0": "Chord",
    "1": "Melodic",
    "2": "Chord+Melodic fill",
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


def focus_name(row: dict[str, str]) -> str:
    if row["voice"] == "A":
        return "Bass"
    return SECONDARY_ROLE.get(row["secondary_role"], f"Secondary role {row['secondary_role']}")


def require_case(
    actual_by_key: dict[tuple[str, str, str], dict[str, str]],
    changed_keys: set[tuple[str, str, str]],
    mode: str,
    ordinal: str,
    voice: str,
    progression: str,
    old_clock: str | None = None,
    new_clock: str | None = None,
    must_change: bool | None = None,
) -> dict[str, str]:
    key = (mode, ordinal, voice)
    row = actual_by_key.get(key)
    if row is None:
        raise AssertionError(f"listening case disappeared: {mode}/{ordinal}/{voice}")
    if row["progression"] != progression:
        raise AssertionError(
            f"listening case progression moved for {mode}/{ordinal}/{voice}: "
            f"{row['progression']} != {progression}"
        )
    if old_clock is not None and row["old_chord_onsets"] != old_clock:
        raise AssertionError(
            f"listening case old clock moved for {mode}/{ordinal}/{voice}: "
            f"{row['old_chord_onsets']} != {old_clock}"
        )
    if new_clock is not None and row["harmonic_onsets"] != new_clock:
        raise AssertionError(
            f"listening case new clock moved for {mode}/{ordinal}/{voice}: "
            f"{row['harmonic_onsets']} != {new_clock}"
        )
    changed = key in changed_keys
    if must_change is not None and changed != must_change:
        raise AssertionError(
            f"listening case change classification moved for {mode}/{ordinal}/{voice}: "
            f"changed={changed}, expected {must_change}"
        )
    return row


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

    static_rows = [row for row in actual_rows if row["harmonic_class"] == "static"]
    static_changed = [row for row, _ in changed if row["harmonic_class"] == "static"]
    if static_changed:
        identities = ", ".join(
            f"{row['mode']}/{row['ordinal']}/{row['voice']}/{row['progression']}"
            for row in static_changed[:12]
        )
        raise AssertionError(
            f"static harmony changed in {len(static_changed)} rows: {identities}"
        )

    same_clock_changed = [row for row, _ in changed if row["clock_relation"] == "same"]
    if same_clock_changed:
        identities = ", ".join(
            f"{row['mode']}/{row['ordinal']}/{row['voice']}"
            for row in same_clock_changed[:12]
        )
        raise AssertionError(
            "pitch changed even though old chord clock and new harmonic clock are identical: "
            + identities
        )

    direction = Counter()
    old_count_histogram = Counter()
    for row, _ in changed:
        old_count = int(row["old_chord_event_count"])
        new_count = int(row["harmonic_event_count"])
        old_count_histogram[old_count] += 1
        if new_count > old_count:
            direction["increased"] += 1
        elif new_count < old_count:
            direction["decreased"] += 1
        else:
            if row["clock_relation"] != "different":
                raise AssertionError(
                    "same-count changed row no longer has different timing: "
                    f"{row['mode']}/{row['ordinal']}/{row['voice']}"
                )
            direction["same-count/different-position"] += 1

    if dict(direction) != EXPECTED_DIRECTION:
        raise AssertionError(
            f"F08 direction-of-change moved: {dict(direction)} != {EXPECTED_DIRECTION}"
        )
    if dict(sorted(old_count_histogram.items())) != EXPECTED_OLD_COUNT_HISTOGRAM:
        raise AssertionError(
            "F08 old harmonic-event histogram moved: "
            f"{dict(sorted(old_count_histogram.items()))} != {EXPECTED_OLD_COUNT_HISTOGRAM}"
        )

    count_different = direction["increased"] + direction["decreased"]
    zero_to_two = [
        row
        for row, _ in changed
        if row["old_chord_event_count"] == "0" and row["harmonic_event_count"] == "2"
    ]
    if len(zero_to_two) != EXPECTED_ZERO_TO_TWO:
        raise AssertionError(
            f"F08 0->2 risk class moved: {len(zero_to_two)} != {EXPECTED_ZERO_TO_TWO}"
        )

    listening_rows = []
    for group, mode, ordinal, voice, progression, old_clock, new_clock in LISTENING_CASES:
        row = require_case(
            actual_by_key,
            changed_keys,
            mode,
            ordinal,
            voice,
            progression,
            old_clock,
            new_clock,
            must_change=True,
        )
        listening_rows.append((group, row))

    control_rows = []
    for group, mode, ordinal, voice, progression in CONTROL_CASES:
        row = require_case(
            actual_by_key,
            changed_keys,
            mode,
            ordinal,
            voice,
            progression,
            must_change=False,
        )
        control_rows.append((group, row))

    reggae_moving = [
        row for row in actual_rows if row["mode"] == "Reggae" and row["harmonic_class"] == "moving"
    ]
    reggae_moving_changed = [row for row in reggae_moving if row_key(row) in changed_keys]

    for mode in ("Acid", "Rave", "Techno"):
        mode_rows = [row for row in actual_rows if row["mode"] == mode]
        if not mode_rows or any(row["harmonic_class"] != "static" for row in mode_rows):
            raise AssertionError(f"{mode} is no longer a fully-static frozen-corpus control")
        if any(row_key(row) in changed_keys for row in mode_rows):
            raise AssertionError(f"{mode} zero-change framing is no longer valid")

    report: list[str] = [
        "# 0.9.9-F08 Stage15 semantic drift classification",
        "",
        "## Snapshot",
        "",
        f"- Corpus rows: **{len(actual_rows)}**",
        f"- Changed rows: **{len(changed)}**",
        f"- Topology changes: **{column_counts['topology']}**",
        f"- Articulation changes: **{column_counts['articulation']}**",
        f"- Pitch changes: **{column_counts['pitch']}**",
        f"- Full-fingerprint changes: **{column_counts['full']}**",
        f"- Changed rows with different event counts: **{count_different}**",
        "- Changed rows with the same event count but different positions: "
        f"**{direction['same-count/different-position']}**",
        "",
        "## Evidence classification",
        "",
        "### Strong evidence",
        "",
        f"- topology changed = **{column_counts['topology']}**",
        f"- articulation changed = **{column_counts['articulation']}**",
        f"- pitch-only drift = **{column_counts['pitch']}** rows",
        f"- identical old/new clock -> changed = **{len(same_clock_changed)}** rows",
        "- same event count but different positions -> "
        f"**{direction['same-count/different-position']}** changed rows",
        "",
        "The same-count/different-position class is the clearest timing-ownership "
        "evidence: harmonic timing changed even when event cardinality did not.",
        "",
        "### Sanity check",
        "",
        f"- static harmony changed = **{len(static_changed)} / {len(static_rows)}**",
        "",
        "This is a sanity check, not independent proof of correctness: the old "
        "ChordProgression path already forced static progressions to one harmonic event.",
        "",
        "### Weak / non-independent evidence",
        "",
        "- Acid / Rave / Techno zero-change rows are not additional independent evidence. "
        "Their frozen corpus is fully static.",
        "",
        "### Sensitivity control",
        "",
        f"- Reggae moving rows: **{len(reggae_moving)}**",
        f"- Reggae moving rows changed: **{len(reggae_moving_changed)}**",
        "",
        "Reggae therefore supplies a useful moving negative control: its moving clock "
        "can differ while the final fingerprint remains unchanged.",
        "",
        "## Direction of change",
        "",
        f"- harmonic activity increased: **{direction['increased']}**",
        f"- harmonic activity decreased: **{direction['decreased']}**",
        "- event count same / positions changed: "
        f"**{direction['same-count/different-position']}**",
        "",
        "F08 must not be framed as reducing harmonic busyness. It replaces "
        "articulation-derived harmonic timing with independently owned harmonic timing; "
        "activity may increase, decrease, or keep the same count at different positions.",
        "",
        "### Old harmonic event counts among changed rows",
        "",
        "| old count | changed rows |",
        "| ---: | ---: |",
    ]
    for count in sorted(EXPECTED_OLD_COUNT_HISTOGRAM):
        report.append(f"| {count} | {old_count_histogram[count]} |")
    report.extend(
        [
            "",
            "## High-risk class: 0 -> 2 harmonic events",
            "",
            f"- Rows: **{len(zero_to_two)}**",
            "- Old harmonic clock: `0000`",
            "- F08 harmonic clock: `8080`",
            "",
            "This class is qualitatively larger than a timing relocation: harmonic "
            "advancement changes from none/effectively static to explicit two-state movement.",
            "",
            "Required listening representatives:",
            "",
            "- DrumAndBass 5 B / MINOR FALL / `0000 -> 8080`",
            "- TripHop 4 A / II-V-I / `0000 -> 8080`",
            "- House 4 A / POP CYCLE / `0000 -> 8080`",
            "",
            "## Bootstrap quarantine framing",
            "",
            "The current implementation intentionally collapses the default vocabulary to:",
            "",
            "- static -> `{0}`",
            "- moving -> `{0,8}`",
            "",
            "This is **not** accepted as the final HarmonicRhythm musical vocabulary. "
            "The separate F08 bootstrap quarantine remains active until musical context "
            "drives a richer moving-policy vocabulary and that musical acceptance is reviewed.",
            "",
            "Future policy should consume musical context such as progression vocabulary, "
            "Phrase position, phrase harmonic position, and temporal/tempo corridor context "
            "if needed. Genre-specific exception tables and raw BPM thresholds are not owners.",
            "",
            "## Tempo-invariance risk",
            "",
            "`{0,8}` means one harmonic change after half a 4/4 bar. The structural "
            "clock is identical, but perceived cadence varies materially with tempo.",
            "",
            "| illustrative context | BPM | seconds between harmonic states |",
            "| --- | ---: | ---: |",
            "| LoFi | 72 | 1.67 |",
            "| House | 124 | 0.97 |",
            "| UK Garage | 132 | 0.91 |",
            "| DnB | 174 | 0.69 |",
            "",
            "These are review examples, not BPM thresholds and not a genre ownership table.",
            "",
            "## Recommended hardware listening corpus",
            "",
            "| group | case | focus | progression | old clock -> F08 clock |",
            "| --- | --- | --- | --- | --- |",
        ]
    )
    for group, row in listening_rows:
        report.append(
            f"| {group} | {row['mode']} {row['ordinal']} {row['voice']} | "
            f"{focus_name(row)} | {row['progression']} | "
            f"`{row['old_chord_onsets']} -> {row['harmonic_onsets']}` |"
        )
    for group, row in control_rows:
        report.append(
            f"| {group} | {row['mode']} {row['ordinal']} {row['voice']} | "
            f"{focus_name(row)} | {row['progression']} | "
            f"`{row['old_chord_onsets']} -> {row['harmonic_onsets']}` |"
        )
    report.extend(
        [
            "",
            "## Neutral musical acceptance questions",
            "",
            "- [ ] harmonic activity соответствует роли/жанровому материалу?",
            "- [ ] harmony не стала слишком активной?",
            "- [ ] harmony не стала слишком статичной?",
            "- [ ] step 8 не слышится как искусственный обязательный перелом каждого такта?",
            "- [ ] в `0000 -> 8080` cases не появилась ли harmonic movement там, "
            "где прежняя редкость была музыкально полезной?",
            "- [ ] bass / chords / melody остаются согласованы с одним harmonic state?",
            "- [ ] progression остаётся различимой, но articulation не управляет progression timing?",
            "",
        ]
    )

    append_group(report, "By genre", grouped(actual_rows, changed_keys, "mode"))
    append_group(report, "By voice", grouped(actual_rows, changed_keys, "voice"))
    append_group(report, "By progression", grouped(actual_rows, changed_keys, "progression"))
    append_group(report, "By harmonic class", grouped(actual_rows, changed_keys, "harmonic_class"))
    append_group(report, "By old/new clock relation", grouped(actual_rows, changed_keys, "clock_relation"))

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
