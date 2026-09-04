#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Iterable, NamedTuple

DEPTH_ORDER = {"P1": 0, "P2": 1, "P3": 2}
ROLE_LABELS = {"0": "CHORD", "1": "MELODIC", "2": "HYBRID"}
OBSERVED_MISSING = {"", "NOT_OBSERVED"}


class DepthStatus(NamedTuple):
    role_identity_stable: str
    supporting_activity_stable: str


class FeelSummary(NamedTuple):
    effect_profiles: tuple[str, ...]
    effect_rows: int
    zero_rows: int
    total_displaced_events: int
    max_abs_ticks: int


class DensitySummary(NamedTuple):
    corridor_min_values: tuple[int, ...]
    corridor_max_values: tuple[int, ...]
    resolved_targets: tuple[int, ...]
    same_target_multi_topology_profiles: tuple[str, ...]
    exact_rhythm_across_targets_profiles: tuple[str, ...]


class PhraseSummary(NamedTuple):
    selected: Counter[str]
    admitted: Counter[str]
    changed: Counter[str]


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def write_tsv(path: Path, rows: list[dict[str, str]], fieldnames: list[str]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=fieldnames,
            delimiter="\t",
            lineterminator="\n",
            extrasaction="raise",
        )
        writer.writeheader()
        writer.writerows(rows)


def observed(value: str) -> bool:
    return value not in OBSERVED_MISSING


def physical_synth_b_activity(row: dict[str, str]) -> bool | None:
    value = row.get("synth_b_onsets", "")
    if not observed(value):
        return None
    return value != "0000"


def _stable(values: Iterable[object | None]) -> str:
    material = list(values)
    if not material or any(value is None for value in material):
        return "NOT_OBSERVED"
    return "YES" if len(set(material)) == 1 else "NO"


def depth_statuses(raw_rows: list[dict[str, str]]) -> dict[tuple[str, str], DepthStatus]:
    grouped: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in raw_rows:
        grouped[(row["profile_id"], row["seed"])].append(row)

    statuses: dict[tuple[str, str], DepthStatus] = {}
    for key in sorted(grouped):
        rows = sorted(grouped[key], key=lambda row: DEPTH_ORDER.get(row.get("depth", ""), 99))
        depths = [row.get("depth", "") for row in rows]
        if depths != ["P1", "P2", "P3"]:
            statuses[key] = DepthStatus("NOT_OBSERVED", "NOT_OBSERVED")
            continue

        role_values: list[str | None] = [
            row["synth_b_role"] if observed(row.get("synth_b_role", "")) else None
            for row in rows
        ]
        activity_values = [physical_synth_b_activity(row) for row in rows]
        statuses[key] = DepthStatus(_stable(role_values), _stable(activity_values))
    return statuses


def _as_int(value: str) -> int | None:
    if not observed(value):
        return None
    return int(value)


def feel_summary(materialized_rows: list[dict[str, str]]) -> FeelSummary:
    effect_profiles: set[str] = set()
    effect_rows = 0
    zero_rows = 0
    total_displaced_events = 0
    max_abs_ticks = 0

    for row in materialized_rows:
        displaced = _as_int(row.get("timing_displaced_events", ""))
        max_delta = _as_int(row.get("max_timing_delta", ""))
        if displaced is None or max_delta is None:
            continue
        if displaced > 0:
            effect_rows += 1
            effect_profiles.add(row["profile_id"])
            total_displaced_events += displaced
            max_abs_ticks = max(max_abs_ticks, abs(max_delta))
        else:
            zero_rows += 1

    return FeelSummary(
        tuple(sorted(effect_profiles)),
        effect_rows,
        zero_rows,
        total_displaced_events,
        max_abs_ticks,
    )


def density_summary(
    raw_rows: list[dict[str, str]],
    materialized_rows: list[dict[str, str]],
) -> DensitySummary:
    corridor_min_values = sorted(
        {value for row in raw_rows if (value := _as_int(row.get("density_min", ""))) is not None}
    )
    corridor_max_values = sorted(
        {value for row in raw_rows if (value := _as_int(row.get("density_max", ""))) is not None}
    )
    resolved_targets = sorted(
        {
            value
            for row in materialized_rows
            if (value := _as_int(row.get("resolved_density", ""))) is not None
        }
    )

    by_profile: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in materialized_rows:
        by_profile[row["profile_id"]].append(row)

    same_target_multi_topology_profiles: list[str] = []
    exact_rhythm_across_targets_profiles: list[str] = []
    for profile_id in sorted(by_profile):
        target_to_rhythm: dict[int, set[str]] = defaultdict(set)
        rhythm_to_target: dict[str, set[int]] = defaultdict(set)
        for row in by_profile[profile_id]:
            target = _as_int(row.get("resolved_density", ""))
            rhythm = row.get("rhythm_signature_id", "")
            if target is None or not observed(rhythm):
                continue
            target_to_rhythm[target].add(rhythm)
            rhythm_to_target[rhythm].add(target)

        if any(len(signatures) > 1 for signatures in target_to_rhythm.values()):
            same_target_multi_topology_profiles.append(profile_id)
        if any(len(targets) > 1 for targets in rhythm_to_target.values()):
            exact_rhythm_across_targets_profiles.append(profile_id)

    return DensitySummary(
        tuple(corridor_min_values),
        tuple(corridor_max_values),
        tuple(resolved_targets),
        tuple(same_target_multi_topology_profiles),
        tuple(exact_rhythm_across_targets_profiles),
    )


def phrase_summary(materialized_rows: list[dict[str, str]]) -> PhraseSummary:
    selected: Counter[str] = Counter()
    admitted: Counter[str] = Counter()
    changed: Counter[str] = Counter()
    for row in materialized_rows:
        law = row.get("declared_phrase_law", "")
        if not observed(law):
            continue
        selected[law] += 1
        if row.get("phrase_admitted") == "YES":
            admitted[law] += 1
            changed_bars = _as_int(row.get("phrase_changed_bars", ""))
            if changed_bars is not None and changed_bars > 0:
                changed[law] += 1
    return PhraseSummary(selected, admitted, changed)


def role_distribution(raw_rows: list[dict[str, str]]) -> Counter[str]:
    result: Counter[str] = Counter()
    for row in raw_rows:
        value = row.get("synth_b_role", "")
        if observed(value):
            result[ROLE_LABELS.get(value, f"UNKNOWN({value})")] += 1
    return result


def classification_counts(pairwise_rows: list[dict[str, str]]) -> Counter[str]:
    return Counter(row["classification"] for row in pairwise_rows)


def _insert_after(fieldnames: list[str], existing: str, new_name: str) -> list[str]:
    if new_name in fieldnames:
        return fieldnames
    result = list(fieldnames)
    index = result.index(existing) + 1
    result.insert(index, new_name)
    return result


def apply_depth_reporting(
    raw_rows: list[dict[str, str]],
    materialized_rows: list[dict[str, str]],
    profile_rows: list[dict[str, str]],
) -> tuple[list[str], list[str], dict[tuple[str, str], DepthStatus]]:
    statuses = depth_statuses(raw_rows)

    materialized_fields = list(materialized_rows[0].keys())
    materialized_fields = _insert_after(
        materialized_fields,
        "depth_role_identity_stable",
        "depth_supporting_activity_stable",
    )
    for row in materialized_rows:
        status = statuses[(row["profile_id"], row["seed"])]
        row["depth_role_identity_stable"] = status.role_identity_stable
        row["depth_supporting_activity_stable"] = status.supporting_activity_stable

    by_profile: dict[str, list[DepthStatus]] = defaultdict(list)
    for (profile_id, _seed), status in statuses.items():
        by_profile[profile_id].append(status)

    profile_fields = list(profile_rows[0].keys())
    profile_fields = _insert_after(
        profile_fields,
        "depth_role_change_triplets",
        "depth_supporting_activity_change_triplets",
    )
    for row in profile_rows:
        profile_statuses = by_profile[row["profile_id"]]
        row["depth_role_change_triplets"] = str(
            sum(status.role_identity_stable == "NO" for status in profile_statuses)
        )
        row["depth_supporting_activity_change_triplets"] = str(
            sum(status.supporting_activity_stable == "NO" for status in profile_statuses)
        )

    return materialized_fields, profile_fields, statuses


def _fmt_int_values(values: tuple[int, ...]) -> str:
    return "[" + ", ".join(str(value) for value in values) + "]"


def _replace_section(text: str, heading: str, next_heading: str, replacement: str) -> str:
    pattern = re.compile(
        rf"{re.escape(heading)}\n.*?(?=\n{re.escape(next_heading)}\n)",
        flags=re.DOTALL,
    )
    updated, count = pattern.subn(replacement.rstrip(), text, count=1)
    if count != 1:
        raise RuntimeError(f"cannot replace findings section {heading!r}")
    return updated


def render_measured_axis_section(
    raw_rows: list[dict[str, str]],
    materialized_rows: list[dict[str, str]],
    statuses: dict[tuple[str, str], DepthStatus],
) -> str:
    density = density_summary(raw_rows, materialized_rows)
    feel = feel_summary(materialized_rows)
    phrase = phrase_summary(materialized_rows)
    roles = role_distribution(raw_rows)

    role_identity_changes = sum(
        status.role_identity_stable == "NO" for status in statuses.values()
    )
    role_identity_observed = sum(
        status.role_identity_stable != "NOT_OBSERVED" for status in statuses.values()
    )
    activity_changes = sum(
        status.supporting_activity_stable == "NO" for status in statuses.values()
    )
    activity_observed = sum(
        status.supporting_activity_stable != "NOT_OBSERVED" for status in statuses.values()
    )

    grouped_rhythm: dict[tuple[str, str], set[str]] = defaultdict(set)
    for row in materialized_rows:
        grouped_rhythm[(row["profile_id"], row["seed"])].add(row["rhythm_signature_id"])
    structural_changes = {key for key, values in grouped_rhythm.items() if len(values) > 1}

    resolved_feel_effect = Counter(
        row.get("resolved_feel", "NOT_OBSERVED")
        for row in materialized_rows
        if (_as_int(row.get("timing_displaced_events", "")) or 0) > 0
    )

    lines = [
        "## 4. Measured axis capacity",
        "",
        "### Genre / Recipe",
        "",
        f"The production catalog materialized **{len({row['profile_id'] for row in materialized_rows})}** profile identities. Labels alone are not counted as musical capacity.",
        "",
        "### Density",
        "",
        f"Profile corridor minima observed in the frozen production selection are **{_fmt_int_values(density.corridor_min_values)}** and corridor maxima are **{_fmt_int_values(density.corridor_max_values)}**.",
        f"The actually resolved internal `structuralDensityTarget` values are **{_fmt_int_values(density.resolved_targets)}**. These are different quantities; corridor bounds must not be reported as resolved materialized targets.",
        f"All **{len(density.same_target_multi_topology_profiles)}/33** profiles materialize more than one RhythmSignature at at least one identical resolved density target, so a resolved target is not a unique topology identity.",
        f"Exact RhythmSignature reuse across different resolved density targets occurs in **{len(density.exact_rhythm_across_targets_profiles)}** profiles: " + ", ".join(f"`{profile}`" for profile in density.exact_rhythm_across_targets_profiles) + ".",
        "Conclusion: **DENSITY is materialized and causal, but is not a unique structural-identity owner.**",
        "",
        "### Feel",
        "",
        f"Actual timing displacement is present in **{feel.effect_rows}** realizations across **{len(feel.effect_profiles)}/33** profiles and zero in **{feel.zero_rows}** applied realizations.",
        f"The corpus contains **{feel.total_displaced_events}** displaced physical events; maximum observed absolute displacement is **{feel.max_abs_ticks} transport tick**.",
        "Profiles with materialized displacement: " + ", ".join(f"`{profile}`" for profile in feel.effect_profiles) + ".",
        f"The other **{33 - len(feel.effect_profiles)}/33** profiles have **NO MATERIALIZED EFFECT IN THE FROZEN CORPUS** for FEEL; this is characterization, not an automatic bug classification.",
        "`kFeelTicksPerStep = 24` is the grid-step size at 96 PPQN, not the measured displacement magnitude; the observed offset field is already expressed in transport ticks.",
        "Resolved FEEL values on displaced rows: " + ", ".join(f"`{key}`={value}" for key, value in sorted(resolved_feel_effect.items())) + ".",
        "",
        "### Phrase law",
        "",
    ]
    for law in ("DEVELOP/RETURN", "LOOP", "REPEAT/REPLY", "SPARSE DRIFT"):
        lines.append(
            f"- `{law}`: selected **{phrase.selected[law]}**, admitted **{phrase.admitted[law]}**, materialized with bar-to-bar change in **{phrase.changed[law]}** realizations."
        )
    lines.extend(
        [
            "",
            f"Production-valid phrase admission is **{sum(phrase.admitted.values())} / {len(materialized_rows)}**.",
            "`declared_phrase_law` in the raw Gate B seam is the weighted production law selected for that realization. It is not a static one-law-per-profile owner.",
            "PhraseSignature encodes relative temporal form: equality classes, base/previous structural deltas, material-change locations, functions, and final return. Absolute bar hashes are not part of the musical signature. Trajectory ID remains provenance only.",
            "",
            "### secondaryRole",
            "",
            "Actual semantic-role distribution over applied realizations: "
            + ", ".join(f"{role}={roles[role]}" for role in ("CHORD", "MELODIC", "HYBRID"))
            + ".",
            "RoleSignature exclusively owns secondary-role identity plus DRUMS/BASS/CHORD/MELODIC and Synth-B participation. HarmonySignature contains only observed harmonic material.",
            "",
            "### DEPTH",
            "",
            f"Matched profile+seed P1/P2/P3 triplets: **{len(statuses)}**.",
            f"`secondaryRole` semantic identity is observed and stable in **{role_identity_observed - role_identity_changes} / {len(statuses)}** triplets; identity changes: **{role_identity_changes} / {len(statuses)}**.",
            f"Actual Synth-B active/inactive participation changes while the semantic role identity remains stable in **{activity_changes} / {len(statuses)}** triplets; supporting activity is sufficiently observed in **{activity_observed} / {len(statuses)}** triplets.",
            f"RhythmSignature changes across P1/P2/P3 in **{len(structural_changes)} / {len(statuses)}** matched triplets; TransformationSignature separately measures relative P1→P2→P3 intervention magnitude.",
            "Conclusion: **DEPTH itself is a causal realization/transformation-magnitude axis. ROLE IDENTITY VIA DEPTH is negative capacity in this frozen corpus. DEPTH can nevertheless affect the materialized activity magnitude of the already-selected supporting role in a small subset.**",
            "",
            "### GRID negative capacity",
            "",
            "Observed structural GRID values: **[16]**. Core-v1 remains GRID=16; GRID 8/32 remain intentionally unsupported **NEGATIVE CAPACITY**.",
        ]
    )
    return "\n".join(lines)


def render_negative_section() -> str:
    return "\n".join(
        [
            "## 9. Negative capacity",
            "",
            "- Core-v1 structural GRID 8/32: **NEGATIVE CAPACITY / intentionally unsupported**.",
            "- DEPTH realization/transformation magnitude: **causal/distinct frozen capacity**, not negative capacity.",
            "- ROLE IDENTITY VIA DEPTH: **NEGATIVE CAPACITY in the frozen corpus**; 0/1,815 semantic secondaryRole identity reassignments are observed.",
            "- DEPTH → supporting-role activity magnitude is **not** negative capacity: Synth-B active/inactive changes are observed in 15/1,815 matched triplets.",
            "- Inert FEEL means **NO MATERIALIZED EFFECT IN THE FROZEN CORPUS** for the affected profiles; it is evidence, not automatically a production defect.",
            "- NegativeSignature remains first-class prohibition/absence evidence but never double-votes the positive source dimension.",
        ]
    )


def render_limitations_section() -> str:
    return "\n".join(
        [
            "## 10. Observation limitations",
            "",
            "- Physical note duration is not exposed: **NOT_OBSERVED**, never zero.",
            "- Physical DrumStep does not expose structural/secondary/ghost importance; Gate B does not reconstruct it from velocity or declarations.",
            "- Timbre/engine/oscillator/sample/kit/FX identity is intentionally removed, and the V0R seam does not expose separate positive timbre evidence; therefore TIMBRE-DEPENDENT cannot be inferred from labels.",
            "- Relative pitch is observed as pitch class relative to tonal root plus contour; richer harmonic function is not fabricated.",
            "- `resolved_density` is the internal production `structuralDensityTarget`; profile corridor bounds (`density_min` / `density_max`) are related selection inputs, not aliases for that resolved target.",
            "- `declared_phrase_law` records the production-weighted law selected for the individual realization; Gate B does not invent a single canonical law per profile.",
            "- FEEL timing offsets in the observation seam are already transport ticks. The 24-tick sixteenth-note step size is not a displacement multiplier.",
            "- Pairwise results characterize the frozen deterministic 55-seed matched corpus, not every possible RNG identity.",
        ]
    )


def finalize_findings(
    findings_path: Path,
    raw_rows: list[dict[str, str]],
    materialized_rows: list[dict[str, str]],
    pairwise_rows: list[dict[str, str]],
    statuses: dict[tuple[str, str], DepthStatus],
) -> None:
    text = findings_path.read_text(encoding="utf-8")
    text = _replace_section(
        text,
        "## 4. Measured axis capacity",
        "## 5. Pairwise results",
        render_measured_axis_section(raw_rows, materialized_rows, statuses),
    )
    text = _replace_section(
        text,
        "## 9. Negative capacity",
        "## 10. Observation limitations",
        render_negative_section(),
    )
    text = _replace_section(
        text,
        "## 10. Observation limitations",
        "## 11. Gate B conclusion",
        render_limitations_section(),
    )

    counts = classification_counts(pairwise_rows)
    expected = {
        "STRUCTURALLY DISTINCT": 381,
        "PARTIALLY DISTINCT": 147,
        "TIMBRE-DEPENDENT": 0,
        "STRUCTURALLY REDUNDANT": 0,
        "INSUFFICIENT EVIDENCE": 0,
    }
    if sum(counts.values()) != 528:
        raise RuntimeError(f"pairwise cardinality drift: {sum(counts.values())} != 528")
    if any(counts[name] != value for name, value in expected.items()):
        raise RuntimeError(f"pairwise classification drift: {dict(counts)}")

    findings_path.write_text(text.rstrip() + "\n", encoding="utf-8")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Finalize GF2-C2 Gate B reporting from raw production-backed evidence."
    )
    parser.add_argument("--raw", required=True, type=Path)
    parser.add_argument("--generated-dir", required=True, type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    generated_dir: Path = args.generated_dir
    materialized_path = generated_dir / "GF2_GATE_B_MATERIALIZED_CORPUS.tsv"
    profile_path = generated_dir / "GF2_GATE_B_PROFILE_SIGNATURES.tsv"
    pairwise_path = generated_dir / "GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv"
    findings_path = generated_dir / "GF2_GATE_B_FINDINGS.md"

    raw_rows = read_tsv(args.raw)
    materialized_rows = read_tsv(materialized_path)
    profile_rows = read_tsv(profile_path)
    pairwise_rows = read_tsv(pairwise_path)

    if len(raw_rows) != 5445 or len(materialized_rows) != 5445:
        raise RuntimeError(
            f"corpus cardinality drift: raw={len(raw_rows)} materialized={len(materialized_rows)}"
        )
    if len(profile_rows) != 33:
        raise RuntimeError(f"profile cardinality drift: {len(profile_rows)} != 33")
    if len(pairwise_rows) != 528:
        raise RuntimeError(f"pairwise cardinality drift: {len(pairwise_rows)} != 528")

    materialized_fields, profile_fields, statuses = apply_depth_reporting(
        raw_rows, materialized_rows, profile_rows
    )

    identity_changes = sum(
        status.role_identity_stable == "NO" for status in statuses.values()
    )
    activity_changes = sum(
        status.supporting_activity_stable == "NO" for status in statuses.values()
    )
    if len(statuses) != 1815:
        raise RuntimeError(f"DEPTH triplet cardinality drift: {len(statuses)} != 1815")
    if identity_changes != 0:
        raise RuntimeError(
            f"semantic secondaryRole identity changed in {identity_changes} triplets; inspect before freeze"
        )
    if activity_changes != 15:
        raise RuntimeError(
            f"supporting Synth-B activity changed in {activity_changes} triplets; expected reviewed corpus=15"
        )

    write_tsv(materialized_path, materialized_rows, materialized_fields)
    write_tsv(profile_path, profile_rows, profile_fields)
    finalize_findings(
        findings_path,
        raw_rows,
        materialized_rows,
        pairwise_rows,
        statuses,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
