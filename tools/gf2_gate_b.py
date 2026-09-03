#!/usr/bin/env python3
"""GF2-C2 Gate B deterministic materialized-capacity analysis.

The input is the production-backed TSV emitted by tools/gf2/gf2_gate_b_dump.cpp.
This module does not parse production declarations or reproduce generation
policy. It only neutralizes, signs, aggregates and compares already-materialized
observations.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Iterable, Mapping, Sequence

NOT_OBSERVED = "NOT_OBSERVED"
NONE = "NONE"
DEPTHS = ("P1", "P2", "P3")
POSITIVE_DIMENSIONS = (
    "rhythm",
    "bass",
    "harmony",
    "phrase",
    "role",
    "transformation",
)
ALL_DIMENSIONS = POSITIVE_DIMENSIONS + ("negative",)
ROLE_NAMES = {
    "0": "CHORD",
    "1": "MELODIC",
    "2": "HYBRID",
}


def load_seeds(path: Path | str) -> tuple[int, ...]:
    lines = Path(path).read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "seed":
        raise ValueError("Gate B seed corpus must begin with 'seed'")
    seeds = tuple(int(line, 0) for line in lines[1:] if line.strip())
    if len(seeds) != len(set(seeds)):
        raise ValueError("Gate B seed corpus contains duplicates")
    for seed in seeds:
        if seed < 0 or seed > 0xFFFFFFFF:
            raise ValueError(f"seed outside uint32 domain: {seed}")
        if (seed & 0xFFFF) == 0xFFFF:
            raise ValueError(f"seed uses reserved phrase identity: 0x{seed:08x}")
    return seeds


def canonical_id(prefix: str, payload: str) -> str:
    normalized_prefix = re.sub(r"[^A-Za-z0-9]+", "_", prefix).strip("_").upper()
    digest = hashlib.sha256(
        normalized_prefix.encode("utf-8") + b"\0" + payload.encode("utf-8")
    ).hexdigest()[:16]
    return f"{normalized_prefix}-{digest}"


def relation_for_sequences(left: Sequence[str], right: Sequence[str]) -> str:
    left_observed = tuple(value for value in left if value != NOT_OBSERVED)
    right_observed = tuple(value for value in right if value != NOT_OBSERVED)
    if not left_observed or not right_observed:
        return NOT_OBSERVED
    if left_observed == right_observed:
        return "SAME"
    if set(left_observed).isdisjoint(set(right_observed)):
        return "DISJOINT"
    return "OVERLAP"


def pair_classification(
    relations: Mapping[str, str], timbre_evidence: bool = False
) -> str:
    observed = [
        relations.get(dimension, NOT_OBSERVED)
        for dimension in ALL_DIMENSIONS
        if relations.get(dimension, NOT_OBSERVED) != NOT_OBSERVED
    ]
    positive_observed = [
        relations.get(dimension, NOT_OBSERVED)
        for dimension in POSITIVE_DIMENSIONS
        if relations.get(dimension, NOT_OBSERVED) != NOT_OBSERVED
    ]
    if not observed or not positive_observed:
        return "INSUFFICIENT EVIDENCE"

    disjoint = sum(value == "DISJOINT" for value in observed)
    overlap = sum(value == "OVERLAP" for value in observed)
    if disjoint >= 2:
        return "STRUCTURALLY DISTINCT"
    if disjoint == 1 or overlap > 0:
        return "PARTIALLY DISTINCT"

    if all(value == "SAME" for value in positive_observed):
        if len(positive_observed) < 4:
            return "INSUFFICIENT EVIDENCE"
        if timbre_evidence:
            return "TIMBRE-DEPENDENT"
        if all(value == "SAME" for value in observed):
            return "STRUCTURALLY REDUNDANT"
        return "PARTIALLY DISTINCT"
    return "INSUFFICIENT EVIDENCE"


def expected_pair_count(profile_count: int) -> int:
    return profile_count * (profile_count - 1) // 2


def _mask(value: str) -> int | None:
    if value in (NOT_OBSERVED, ""):
        return None
    return int(value, 16)


def _popcount(value: int | None) -> int | None:
    return None if value is None else value.bit_count()


def _observed_text(value: str) -> str:
    if value == NOT_OBSERVED:
        return NOT_OBSERVED
    return value if value else NONE


def _timing_values(value: str) -> tuple[int, ...] | None:
    if value == NOT_OBSERVED:
        return None
    if not value:
        return tuple()
    values: list[int] = []
    for item in value.split(","):
        if not item:
            continue
        values.append(int(item.rsplit(":", 1)[1]))
    return tuple(values)


def _timing_metrics(row: Mapping[str, str]) -> tuple[int | None, int | None]:
    all_values: list[int] = []
    for key in ("drum_timing", "synth_a_timing", "synth_b_timing"):
        values = _timing_values(row[key])
        if values is None:
            return None, None
        all_values.extend(values)
    displaced = sum(value != 0 for value in all_values)
    maximum = max((abs(value) for value in all_values), default=0)
    return displaced, maximum


def _beat_counts(mask: int | None) -> str:
    if mask is None:
        return NOT_OBSERVED
    counts: list[str] = []
    for beat in range(4):
        count = 0
        for step in range(beat * 4, beat * 4 + 4):
            bit = 1 << (15 - step)
            if mask & bit:
                count += 1
        counts.append(str(count))
    return ",".join(counts)


def _offbeat_count(mask: int | None) -> int | None:
    if mask is None:
        return None
    count = 0
    for step in range(16):
        if step % 4 == 0:
            continue
        if mask & (1 << (15 - step)):
            count += 1
    return count


def _role_name(raw: str) -> str:
    if raw == NOT_OBSERVED:
        return NOT_OBSERVED
    return ROLE_NAMES.get(raw, f"UNKNOWN_ROLE_{raw}")


def _sanitize_phrase_material(material: str) -> str:
    if not material:
        return NONE

    def replace_role(match: re.Match[str]) -> str:
        raw = match.group(1)
        return ",role=" + ROLE_NAMES.get(raw, f"UNKNOWN_ROLE_{raw}")

    return re.sub(r",r=(\d+)", replace_role, material)


def _phrase_metrics(row: Mapping[str, str]) -> dict[str, object]:
    admitted = row["phrase_admitted"] == "YES"
    applied = row["phrase_materialization_status"] == "ALL_APPLIED"
    if not admitted or not applied:
        return {
            "signature_payload": NOT_OBSERVED,
            "changed_bars": None,
            "final_return": NOT_OBSERVED,
            "repeated_bars": None,
        }

    material = _sanitize_phrase_material(row["phrase_material"])
    bars = material.split("|") if material not in ("", NONE) else []
    neutral: list[str] = []
    functions: list[str] = []
    for bar in bars:
        parts = bar.split("@", 2)
        if len(parts) != 3:
            raise ValueError(f"invalid phrase bar payload: {bar!r}")
        functions.append(parts[1])
        neutral.append(parts[2])

    if not neutral:
        return {
            "signature_payload": NOT_OBSERVED,
            "changed_bars": None,
            "final_return": NOT_OBSERVED,
            "repeated_bars": None,
        }
    base = neutral[0]
    changed_positions = [index for index, value in enumerate(neutral) if value != base]
    repeated = sum(value == base for value in neutral[1:])
    final_return = "YES" if neutral[-1] == base else "NO"
    bar_ids = [canonical_id("phrase_bar", value) for value in neutral]
    payload = (
        f"bars={len(neutral)};functions={','.join(functions)};"
        f"bar_ids={','.join(bar_ids)};changed={','.join(map(str, changed_positions)) or 'NONE'};"
        f"return={final_return}"
    )
    return {
        "signature_payload": payload,
        "changed_bars": len(changed_positions),
        "final_return": final_return,
        "repeated_bars": repeated,
    }


def _signature_payloads(row: Mapping[str, str]) -> dict[str, str]:
    if row["migration_status"] != "APPLIED":
        return {dimension: NOT_OBSERVED for dimension in ("rhythm", "bass", "harmony", "phrase", "role")}

    kick = _mask(row["kick_onsets"])
    backbeat = _mask(row["backbeat_onsets"])
    hats = _mask(row["hat_onsets"])
    support = _mask(row["support_onsets"])
    if None in (kick, backbeat, hats, support):
        raise ValueError("APPLIED row contains non-observed drum masks")
    drum_union = int(kick) | int(backbeat) | int(hats) | int(support)
    drum_silence = (~drum_union) & 0xFFFF
    rhythm_payload = ";".join(
        (
            f"kick={row['kick_onsets']}",
            f"backbeat={row['backbeat_onsets']}",
            f"hats={row['hat_onsets']}",
            f"support={row['support_onsets']}",
            f"kick_acc={row['kick_accents']}",
            f"backbeat_acc={row['backbeat_accents']}",
            f"hat_acc={row['hat_accents']}",
            f"support_acc={row['support_accents']}",
            f"timing={_observed_text(row['drum_timing'])}",
            f"beat_activity={_beat_counts(drum_union)}",
            f"offbeat={_offbeat_count(drum_union)}",
            f"silence={drum_silence:04x}",
        )
    )

    bass_onsets = _mask(row["synth_a_onsets"])
    if bass_onsets is None:
        raise ValueError("APPLIED row contains non-observed bass mask")
    bass_payload = ";".join(
        (
            f"onsets={row['synth_a_onsets']}",
            f"accent={row['synth_a_accents']}",
            f"ghost={row['synth_a_ghosts']}",
            f"timing={_observed_text(row['synth_a_timing'])}",
            f"pitch_class={_observed_text(row['synth_a_pitch_class'])}",
            f"contour={_observed_text(row['synth_a_contour'])}",
            f"kick_coincide={(bass_onsets & int(kick)):04x}",
            f"drum_coincide={(bass_onsets & drum_union):04x}",
            f"beat_activity={_beat_counts(bass_onsets)}",
        )
    )

    secondary_role = _role_name(row["synth_b_role"])
    chord_applied = row["chord_applied"] == "YES"
    melodic_applied = row["melodic_applied"] == "YES"
    harmony_payload = ";".join(
        (
            f"events={row['harmonic_event_onsets']}",
            f"count={row['harmonic_event_count']}",
            f"chord_onsets={row['chord_onsets']}",
            f"chord_applied={'YES' if chord_applied else 'NO'}",
            f"secondary_role={secondary_role}",
            "secondary_pitch="
            + (_observed_text(row["synth_b_pitch_class"]) if chord_applied else "NOT_APPLICABLE"),
            "secondary_contour="
            + (_observed_text(row["synth_b_contour"]) if chord_applied else "NOT_APPLICABLE"),
        )
    )

    roles: list[str] = []
    if drum_union:
        roles.append("DRUMS")
    if bass_onsets:
        roles.append("BASS")
    if chord_applied:
        roles.append("CHORD")
    if melodic_applied:
        roles.append("MELODIC")
    role_payload = ";".join(
        (
            f"participating={','.join(roles) if roles else 'NONE'}",
            f"secondary={secondary_role}",
            f"chord={'YES' if chord_applied else 'NO'}",
            f"melodic={'YES' if melodic_applied else 'NO'}",
            f"synth_b_active={'YES' if _mask(row['synth_b_onsets']) else 'NO'}",
        )
    )

    phrase = _phrase_metrics(row)
    return {
        "rhythm": rhythm_payload,
        "bass": bass_payload,
        "harmony": harmony_payload,
        "phrase": str(phrase["signature_payload"]),
        "role": role_payload,
    }


def _row_scalars(row: Mapping[str, str]) -> dict[str, object]:
    if row["migration_status"] != "APPLIED":
        return {
            "physical_onset_step_count": None,
            "structural_onset_count": None,
            "materialized_role_event_count": None,
            "topology_spread": None,
            "timing_displaced_events": None,
            "max_timing_delta": None,
            "role_participation_count": None,
            "phrase_changed_bars": None,
            "phrase_final_return": NOT_OBSERVED,
        }

    masks = {
        key: _mask(row[key])
        for key in (
            "kick_onsets",
            "backbeat_onsets",
            "hat_onsets",
            "support_onsets",
            "synth_a_onsets",
            "synth_b_onsets",
        )
    }
    union = 0
    topology_spread = 0
    for value in masks.values():
        if value:
            topology_spread += 1
            union |= int(value)
    displaced, max_delta = _timing_metrics(row)
    phrase = _phrase_metrics(row)
    role_participation = sum(
        (
            bool((masks["kick_onsets"] or 0) | (masks["backbeat_onsets"] or 0) |
                 (masks["hat_onsets"] or 0) | (masks["support_onsets"] or 0)),
            bool(masks["synth_a_onsets"]),
            row["chord_applied"] == "YES",
            row["melodic_applied"] == "YES",
        )
    )
    return {
        "physical_onset_step_count": union.bit_count(),
        # The physical pattern does not tag drum hits as structural/secondary.
        "structural_onset_count": None,
        "materialized_role_event_count": int(row["physical_event_count"]),
        "topology_spread": topology_spread,
        "timing_displaced_events": displaced,
        "max_timing_delta": max_delta,
        "role_participation_count": role_participation,
        "phrase_changed_bars": phrase["changed_bars"],
        "phrase_final_return": phrase["final_return"],
    }


def _normalize_raw_rows(raw_path: Path) -> list[dict[str, str]]:
    with raw_path.open(newline="", encoding="utf-8") as handle:
        rows = [dict(row) for row in csv.DictReader(handle, delimiter="\t")]
    if not rows:
        raise ValueError("Gate B raw corpus is empty")
    return rows


def _validate_corpus(
    rows: Sequence[Mapping[str, str]], seeds: Sequence[int], contract: Mapping[str, object]
) -> list[str]:
    expected_seed_text = {f"0x{seed:08x}" for seed in seeds}
    profiles = sorted({row["profile_id"] for row in rows})
    if len(profiles) != int(contract["profile_count"]):
        raise ValueError(
            f"profile count drift: observed={len(profiles)} frozen={contract['profile_count']}"
        )
    if len(seeds) != int(contract["seed_count"]):
        raise ValueError("seed count does not match frozen contract")
    expected_depths = tuple(str(value) for value in contract["depths"])
    expected_rows = len(profiles) * len(seeds) * len(expected_depths)
    if len(rows) != expected_rows:
        raise ValueError(f"corpus cardinality mismatch: {len(rows)} != {expected_rows}")

    seen: set[tuple[str, str, str]] = set()
    coverage: dict[tuple[str, str], set[str]] = defaultdict(set)
    for row in rows:
        key = (row["profile_id"], row["seed"], row["depth"])
        if key in seen:
            raise ValueError(f"duplicate realization {key}")
        seen.add(key)
        if row["seed"] not in expected_seed_text:
            raise ValueError(f"unfrozen seed in raw corpus: {row['seed']}")
        coverage[(row["profile_id"], row["seed"])].add(row["depth"])
    required_depths = set(expected_depths)
    if any(value != required_depths for value in coverage.values()):
        raise ValueError("P1/P2/P3 coverage is incomplete")
    return profiles


def _prepare_rows(rows: list[dict[str, str]]) -> None:
    for row in rows:
        payloads = _signature_payloads(row)
        row["_rhythm_payload"] = payloads["rhythm"]
        row["_bass_payload"] = payloads["bass"]
        row["_harmony_payload"] = payloads["harmony"]
        row["_phrase_payload"] = payloads["phrase"]
        row["_role_payload"] = payloads["role"]
        for dimension in ("rhythm", "bass", "harmony", "phrase", "role"):
            payload = payloads[dimension]
            row[f"{dimension}_signature_id"] = (
                NOT_OBSERVED if payload == NOT_OBSERVED else canonical_id(dimension, payload)
            )
        scalars = _row_scalars(row)
        for key, value in scalars.items():
            row[f"_{key}"] = NOT_OBSERVED if value is None else str(value)

    grouped: dict[tuple[str, str], dict[str, dict[str, str]]] = defaultdict(dict)
    for row in rows:
        grouped[(row["profile_id"], row["seed"])][row["depth"]] = row

    for depth_rows in grouped.values():
        if set(depth_rows) != set(DEPTHS):
            raise ValueError("transformation group lacks P1/P2/P3")
        signature_triplets: dict[str, list[str]] = {}
        for dimension in ("rhythm", "bass", "harmony", "role", "phrase"):
            signature_triplets[dimension] = [
                depth_rows[depth][f"{dimension}_signature_id"] for depth in DEPTHS
            ]

        changed = [
            dimension
            for dimension, values in signature_triplets.items()
            if NOT_OBSERVED not in values and len(set(values)) > 1
        ]
        event_counts = [
            depth_rows[depth]["_materialized_role_event_count"] for depth in DEPTHS
        ]
        timing_counts = [depth_rows[depth]["_timing_displaced_events"] for depth in DEPTHS]
        transformation_payload = ";".join(
            (
                "changed=" + (",".join(changed) if changed else "NONE"),
                "events=" + ",".join(event_counts),
                "timing=" + ",".join(timing_counts),
                "rhythm=" + ",".join(signature_triplets["rhythm"]),
                "role=" + ",".join(signature_triplets["role"]),
            )
        )
        transformation_id = canonical_id("transformation", transformation_payload)
        role_values = signature_triplets["role"]
        if NOT_OBSERVED in role_values:
            role_stable = NOT_OBSERVED
        else:
            role_stable = "YES" if len(set(role_values)) == 1 else "NO"

        for row in depth_rows.values():
            row["transformation_signature_id"] = transformation_id
            row["_transformation_payload"] = transformation_payload
            row["_depth_role_stable"] = role_stable
            row["_depth_structural_change"] = "YES" if changed else "NO"

            negative: list[str] = [
                "CORE_GRID_16_ONLY",
                "GRID_8_32_UNSUPPORTED",
            ]
            if role_stable == "YES":
                negative.append("DEPTH_ROLE_HIERARCHY_NOT_MATERIALIZED")
            if row["migration_status"] != "APPLIED":
                negative.append("GENERATION_NOT_MATERIALIZED:" + row["migration_status"])
            else:
                if row["_timing_displaced_events"] == "0":
                    negative.append("NO_MATERIALIZED_FEEL_DISPLACEMENT")
                try:
                    harmonic_count = int(row["harmonic_event_count"])
                except ValueError:
                    harmonic_count = -1
                if harmonic_count <= 1:
                    negative.append("NO_OBSERVED_HARMONIC_MOVEMENT")
                if row["chord_applied"] != "YES" and row["melodic_applied"] != "YES":
                    negative.append("SECONDARY_ROLE_ABSENT")
            if row["phrase_admitted"] == "NO":
                negative.append("PHRASE_NOT_ADMITTED:" + row["phrase_reject_reason"])
            negative_payload = ";".join(sorted(set(negative)))
            row["_negative_payload"] = negative_payload
            row["negative_signature_id"] = canonical_id("negative", negative_payload)


def _ordered_rows_for_profile(rows: Sequence[Mapping[str, str]]) -> list[Mapping[str, str]]:
    depth_order = {depth: index for index, depth in enumerate(DEPTHS)}
    return sorted(rows, key=lambda row: (int(row["seed"], 0), depth_order[row["depth"]]))


def _matched_relation(
    rows_a: Sequence[Mapping[str, str]], rows_b: Sequence[Mapping[str, str]], column: str
) -> str:
    ordered_a = _ordered_rows_for_profile(rows_a)
    ordered_b = _ordered_rows_for_profile(rows_b)
    if [(r["seed"], r["depth"]) for r in ordered_a] != [
        (r["seed"], r["depth"]) for r in ordered_b
    ]:
        raise ValueError("pairwise profiles do not share the same matched corpus")
    left: list[str] = []
    right: list[str] = []
    for row_a, row_b in zip(ordered_a, ordered_b):
        a = row_a[column]
        b = row_b[column]
        if a == NOT_OBSERVED or b == NOT_OBSERVED:
            continue
        left.append(a)
        right.append(b)
    return relation_for_sequences(tuple(left), tuple(right))


def _pair_rows(rows: Sequence[dict[str, str]], profiles: Sequence[str]) -> list[dict[str, str]]:
    by_profile: dict[str, list[dict[str, str]]] = defaultdict(list)
    metadata: dict[str, tuple[str, str]] = {}
    for row in rows:
        by_profile[row["profile_id"]].append(row)
        metadata[row["profile_id"]] = (row["genre"], row["recipe"])

    result: list[dict[str, str]] = []
    for index, profile_a in enumerate(profiles):
        for profile_b in profiles[index + 1 :]:
            relations: dict[str, str] = {}
            for dimension in ALL_DIMENSIONS:
                relations[dimension] = _matched_relation(
                    by_profile[profile_a], by_profile[profile_b], f"{dimension}_signature_id"
                )
            classification = pair_classification(relations, timbre_evidence=False)
            genre_a, recipe_a = metadata[profile_a]
            genre_b, recipe_b = metadata[profile_b]
            if recipe_a == "BASE" or recipe_b == "BASE":
                same_recipe_family = "NOT_APPLICABLE"
            else:
                same_recipe_family = "YES" if recipe_a == recipe_b else "NO"
            evidence = ";".join(f"{dimension}={relations[dimension]}" for dimension in ALL_DIMENSIONS)
            result.append(
                {
                    "profile_a": profile_a,
                    "profile_b": profile_b,
                    "same_genre": "YES" if genre_a == genre_b else "NO",
                    "same_recipe_family": same_recipe_family,
                    **{f"{dimension}_relation": relations[dimension] for dimension in ALL_DIMENSIONS},
                    "classification": classification,
                    "evidence_summary": evidence,
                }
            )
    return result


def _distribution(values: Iterable[str]) -> str:
    counts = Counter(values)
    return ",".join(f"{key}:{counts[key]}" for key in sorted(counts)) if counts else NONE


def _numeric_range(values: Iterable[str]) -> tuple[str, str, str]:
    parsed = sorted({int(value) for value in values if value != NOT_OBSERVED})
    if not parsed:
        return NOT_OBSERVED, NOT_OBSERVED, "0"
    return str(parsed[0]), str(parsed[-1]), str(len(parsed))


def _profile_rows(
    rows: Sequence[dict[str, str]], profiles: Sequence[str], pairwise: Sequence[dict[str, str]]
) -> list[dict[str, str]]:
    by_profile: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        by_profile[row["profile_id"]].append(row)
    pair_counts: dict[str, Counter[str]] = defaultdict(Counter)
    for pair in pairwise:
        pair_counts[pair["profile_a"]][pair["classification"]] += 1
        pair_counts[pair["profile_b"]][pair["classification"]] += 1

    output: list[dict[str, str]] = []
    for profile in profiles:
        values = by_profile[profile]
        first = values[0]
        accepted = [row for row in values if row["migration_status"] == "APPLIED"]
        failures = len(values) - len(accepted)
        density_min, density_max, density_distinct = _numeric_range(
            row["resolved_density"] for row in values
        )
        event_min, event_max, _ = _numeric_range(
            row["_materialized_role_event_count"] for row in values
        )
        max_timing = max(
            (int(row["_max_timing_delta"]) for row in values if row["_max_timing_delta"] != NOT_OBSERVED),
            default=0,
        )
        timing_effect = sum(
            row["_timing_displaced_events"] not in (NOT_OBSERVED, "0") for row in values
        )
        timing_zero = sum(row["_timing_displaced_events"] == "0" for row in values)
        admitted = sum(row["phrase_admitted"] == "YES" for row in values)
        changed_phrase = sum(
            row["_phrase_changed_bars"] not in (NOT_OBSERVED, "0") for row in values
        )
        triplet_rows = [row for row in values if row["depth"] == "P1"]
        depth_role_changes = sum(row["_depth_role_stable"] == "NO" for row in triplet_rows)
        depth_structural_changes = sum(row["_depth_structural_change"] == "YES" for row in triplet_rows)
        classification_counts = pair_counts[profile]
        output.append(
            {
                "profile_id": profile,
                "genre": first["genre"],
                "recipe": first["recipe"],
                "seed_coverage": str(len({row["seed"] for row in values})),
                "depth_coverage": ",".join(sorted({row["depth"] for row in values}, key=DEPTHS.index)),
                "realization_count": str(len(values)),
                "accepted_count": str(len(accepted)),
                "failure_count": str(failures),
                "rhythm_signature_count": str(len({r["rhythm_signature_id"] for r in accepted})),
                "bass_signature_count": str(len({r["bass_signature_id"] for r in accepted})),
                "harmony_signature_count": str(len({r["harmony_signature_id"] for r in accepted})),
                "phrase_signature_count": str(len({r["phrase_signature_id"] for r in values if r["phrase_signature_id"] != NOT_OBSERVED})),
                "role_signature_count": str(len({r["role_signature_id"] for r in accepted})),
                "transformation_signature_count": str(len({r["transformation_signature_id"] for r in values})),
                "negative_signature_count": str(len({r["negative_signature_id"] for r in values})),
                "density_target_min": density_min,
                "density_target_max": density_max,
                "density_target_distinct": density_distinct,
                "materialized_events_min": event_min,
                "materialized_events_max": event_max,
                "timing_effect_rows": str(timing_effect),
                "timing_zero_rows": str(timing_zero),
                "max_timing_delta": str(max_timing),
                "phrase_admitted_rows": str(admitted),
                "phrase_changed_rows": str(changed_phrase),
                "role_participation_distribution": _distribution(r["_role_payload"] for r in accepted),
                "secondary_role_distribution": _distribution(_role_name(r["synth_b_role"]) for r in accepted),
                "depth_triplets": str(len(triplet_rows)),
                "depth_role_change_triplets": str(depth_role_changes),
                "depth_structural_change_triplets": str(depth_structural_changes),
                "pair_structurally_distinct": str(classification_counts["STRUCTURALLY DISTINCT"]),
                "pair_partially_distinct": str(classification_counts["PARTIALLY DISTINCT"]),
                "pair_timbre_dependent": str(classification_counts["TIMBRE-DEPENDENT"]),
                "pair_structurally_redundant": str(classification_counts["STRUCTURALLY REDUNDANT"]),
                "pair_insufficient_evidence": str(classification_counts["INSUFFICIENT EVIDENCE"]),
            }
        )
    return output


def _materialized_rows(rows: Sequence[dict[str, str]]) -> list[dict[str, str]]:
    output: list[dict[str, str]] = []
    for row in _ordered_rows_for_profile(rows):
        output.append(
            {
                "profile_id": row["profile_id"],
                "genre": row["genre"],
                "recipe": row["recipe"],
                "seed": row["seed"],
                "depth": row["depth"],
                "declared_phrase_law": row["declared_phrase_law"],
                "requested_bars": row["requested_bars"],
                "resolved_trajectory": row["resolved_trajectory"],
                "phrase_admitted": row["phrase_admitted"],
                "actual_plan_bars": row["actual_plan_bars"],
                "fallback": row["fallback"],
                "rhythm_signature_id": row["rhythm_signature_id"],
                "bass_signature_id": row["bass_signature_id"],
                "harmony_signature_id": row["harmony_signature_id"],
                "phrase_signature_id": row["phrase_signature_id"],
                "role_signature_id": row["role_signature_id"],
                "transformation_signature_id": row["transformation_signature_id"],
                "negative_signature_id": row["negative_signature_id"],
                "selection_status": row["selection_status"],
                "migration_status": row["migration_status"],
                "v0r_provenance": row["v0r_provenance"],
                "phrase_execution_status": row["phrase_execution_status"],
                "phrase_materialization_status": row["phrase_materialization_status"],
                "resolved_density": row["resolved_density"],
                "grid_steps": row["grid_steps"],
                "resolved_feel": row["resolved_feel"],
                "structural_onset_count": row["_structural_onset_count"],
                "physical_onset_step_count": row["_physical_onset_step_count"],
                "materialized_role_event_count": row["_materialized_role_event_count"],
                "topology_spread": row["_topology_spread"],
                "timing_displaced_events": row["_timing_displaced_events"],
                "max_timing_delta": row["_max_timing_delta"],
                "harmonic_event_count": row["harmonic_event_count"] if row["migration_status"] == "APPLIED" else NOT_OBSERVED,
                "role_participation_count": row["_role_participation_count"],
                "phrase_changed_bars": row["_phrase_changed_bars"],
                "phrase_final_return": row["_phrase_final_return"],
                "depth_role_identity_stable": row["_depth_role_stable"],
                "physical_duration": "NOT_OBSERVED",
            }
        )
    return output


def _write_tsv(path: Path, rows: Sequence[Mapping[str, str]]) -> None:
    if not rows:
        raise ValueError(f"refusing to write empty TSV: {path}")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=list(rows[0].keys()),
            delimiter="\t",
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(rows)


def _one_dimensional_pairs(pairwise: Sequence[Mapping[str, str]]) -> list[tuple[str, str, str]]:
    result: list[tuple[str, str, str]] = []
    for pair in pairwise:
        differing = [
            dimension
            for dimension in POSITIVE_DIMENSIONS
            if pair[f"{dimension}_relation"] in ("DISJOINT", "OVERLAP")
        ]
        observed_others = [
            pair[f"{dimension}_relation"]
            for dimension in POSITIVE_DIMENSIONS
            if dimension not in differing
        ]
        if len(differing) == 1 and all(value == "SAME" for value in observed_others):
            result.append((pair["profile_a"], pair["profile_b"], differing[0]))
    return result


def _findings(
    rows: Sequence[dict[str, str]],
    profiles: Sequence[str],
    profile_rows: Sequence[dict[str, str]],
    pairwise: Sequence[dict[str, str]],
    contract: Mapping[str, object],
) -> str:
    accepted = [row for row in rows if row["migration_status"] == "APPLIED"]
    pair_counts = Counter(row["classification"] for row in pairwise)
    law_counts = Counter(row["declared_phrase_law"] for row in rows if row["declared_phrase_law"] != NOT_OBSERVED)
    law_admitted = Counter(row["declared_phrase_law"] for row in rows if row["phrase_admitted"] == "YES")
    law_changed = Counter(
        row["declared_phrase_law"]
        for row in rows
        if row["_phrase_changed_bars"] not in (NOT_OBSERVED, "0")
    )
    timing_effect = sum(row["_timing_displaced_events"] not in (NOT_OBSERVED, "0") for row in rows)
    timing_zero = sum(row["_timing_displaced_events"] == "0" for row in rows)
    max_timing = max(
        (int(row["_max_timing_delta"]) for row in rows if row["_max_timing_delta"] != NOT_OBSERVED),
        default=0,
    )
    density_targets = sorted({int(row["resolved_density"]) for row in rows if row["resolved_density"] != NOT_OBSERVED})
    event_counts = sorted({int(row["_materialized_role_event_count"]) for row in rows if row["_materialized_role_event_count"] != NOT_OBSERVED})
    role_distribution = Counter(_role_name(row["synth_b_role"]) for row in accepted)
    p1_rows = [row for row in rows if row["depth"] == "P1"]
    depth_role_changes = sum(row["_depth_role_stable"] == "NO" for row in p1_rows)
    depth_role_unknown = sum(row["_depth_role_stable"] == NOT_OBSERVED for row in p1_rows)
    depth_structural_changes = sum(row["_depth_structural_change"] == "YES" for row in p1_rows)
    grid_values = sorted({row["grid_steps"] for row in rows if row["grid_steps"] != NOT_OBSERVED})

    same_genre = [
        row for row in pairwise
        if row["same_genre"] == "YES" and row["classification"] in (
            "STRUCTURALLY REDUNDANT", "PARTIALLY DISTINCT", "INSUFFICIENT EVIDENCE"
        )
    ]
    cross_genre = [
        row for row in pairwise
        if row["same_genre"] == "NO" and row["classification"] in (
            "STRUCTURALLY REDUNDANT", "PARTIALLY DISTINCT"
        )
    ]
    one_dimensional = _one_dimensional_pairs(pairwise)

    def pair_lines(items: Sequence[Mapping[str, str]], limit: int = 40) -> list[str]:
        if not items:
            return ["- None measured in the frozen corpus."]
        lines = []
        for item in items[:limit]:
            lines.append(
                f"- `{item['profile_a']}` ↔ `{item['profile_b']}` — "
                f"{item['classification']}; {item['evidence_summary']}."
            )
        if len(items) > limit:
            lines.append(f"- … {len(items) - limit} additional pairs are in the pairwise TSV.")
        return lines

    lines = [
        "# GF2 Gate B — Materialized Musical Capacity",
        "",
        "## 1. Exact base",
        "",
        f"Frozen measurement base: `{contract['exact_base_sha']}`.",
        "",
        "## 2. Corpus",
        "",
        f"- Production profiles: **{len(profiles)}**.",
        f"- Frozen seeds: **{contract['seed_count']}**.",
        f"- DEPTH coverage: **P1 / P2 / P3** for every matched profile+seed.",
        f"- Main deterministic realizations: **{len(rows)}**.",
        f"- Applied materializations: **{len(accepted)}**; failed/non-applied: **{len(rows) - len(accepted)}**.",
        f"- Unordered profile pairs: **{len(pairwise)}**.",
        "",
        "## 3. Determinism",
        "",
        "**PASS by Gate B contract.** The focused runner requires byte-identical repeated GCC raw dumps, GCC/Clang equality when Clang is available, deterministic Python ordering, and byte-for-byte equality with the committed review artifacts. The artifacts themselves contain no timestamps or UUIDs.",
        "",
        "## 4. Measured axis capacity",
        "",
        "### Genre / Recipe",
        "",
        f"The production catalog materialized **{len(profiles)}** profile identities. Pairwise structural identity is reported dimension-by-dimension below; profile labels alone are never counted as musical capacity.",
        "",
        "### Density",
        "",
        f"Observed production structural-density targets: **{density_targets if density_targets else NOT_OBSERVED}**. Materialized physical role-event counts span **{event_counts[0] if event_counts else NOT_OBSERVED}..{event_counts[-1] if event_counts else NOT_OBSERVED}**. Gate B records `structural_onset_count=NOT_OBSERVED` because the physical pattern does not tag drum hits as structural vs secondary; it does not reconstruct that distinction from velocity or declarations.",
        "",
        "The I4 causal density path remains frozen evidence. Gate B adds full-catalog distribution/collision evidence, not a second density intervention.",
        "",
        "### Feel",
        "",
        f"Actual physical timing displacement is present in **{timing_effect}** realizations and zero in **{timing_zero}** applied realizations; maximum observed absolute displacement is **{max_timing} ticks**. AUTO→zero cases are retained as inert materialized results, not automatically classified as bugs.",
        "",
        "### Phrase law",
        "",
    ]
    for law in sorted(law_counts):
        lines.append(
            f"- `{law}`: declared **{law_counts[law]}**, admitted **{law_admitted[law]}**, materialized with bar-to-bar change in **{law_changed[law]}** runs."
        )
    lines += [
        "",
        "Phrase distinctness is derived from the actual materialized bar sequence, change locations and final-return relation. Trajectory IDs remain provenance only.",
        "",
        "### secondaryRole",
        "",
        "Actual materialized Synth-B semantic-role distribution over applied realizations: "
        + _distribution(f"{key}={value}" for key, value in sorted(role_distribution.items())),
        "RoleSignature uses physical/semantic participation (`DRUMS`, `BASS`, `CHORD`, `MELODIC`) rather than treating the enum ordinal as musical evidence.",
        "",
        "### DEPTH",
        "",
        f"Matched profile+seed triplets: **{len(p1_rows)}**. Structural material changes somewhere across P1/P2/P3 in **{depth_structural_changes}** triplets. Materialized role identity/participation changes solely across the DEPTH intervention in **{depth_role_changes}** triplets; **{depth_role_unknown}** triplets lack sufficient role observation.",
    ]
    if depth_role_changes:
        lines.append(
            "This is a **Gate B finding against the frozen I5 role-hierarchy characterization** and is recorded as evidence only; production is not changed here."
        )
    else:
        lines.append(
            "No DEPTH-only role-identity/participation change was observed in the full frozen corpus, preserving the I5 negative-capacity finding for **ROLE HIERARCHY VIA DEPTH**. DEPTH itself remains a measured transformation-magnitude axis, not negative capacity."
        )
    lines += [
        "",
        "### GRID negative capacity",
        "",
        f"Observed production GRID values in the frozen corpus: **{grid_values}**. The frozen Core-v1 contract remains **GRID=16**; GRID 8/32 remain intentionally unsupported negative capacity. Gate B does not implement them.",
        "",
        "## 5. Pairwise results",
        "",
        f"- STRUCTURALLY DISTINCT: **{pair_counts['STRUCTURALLY DISTINCT']}**",
        f"- PARTIALLY DISTINCT: **{pair_counts['PARTIALLY DISTINCT']}**",
        f"- TIMBRE-DEPENDENT: **{pair_counts['TIMBRE-DEPENDENT']}**",
        f"- STRUCTURALLY REDUNDANT: **{pair_counts['STRUCTURALLY REDUNDANT']}**",
        f"- INSUFFICIENT EVIDENCE: **{pair_counts['INSUFFICIENT EVIDENCE']}**",
        "",
        "No aggregate magic distance decides these classes; every pair carries Rhythm/Bass/Harmony/Phrase/Role/Transformation/Negative relations in the pairwise TSV.",
        "",
        "## 6. Same-Genre Recipe collisions",
        "",
        *pair_lines(same_genre),
        "",
        "## 7. Cross-Genre collisions",
        "",
        *pair_lines(cross_genre),
        "",
        "## 8. One-dimensional profiles",
        "",
    ]
    if one_dimensional:
        for a, b, dimension in one_dimensional[:40]:
            lines.append(f"- `{a}` ↔ `{b}` — nearest measured distinction is only **{dimension}**.")
        if len(one_dimensional) > 40:
            lines.append(f"- … {len(one_dimensional) - 40} additional one-axis pairs are derivable from the pairwise TSV.")
    else:
        lines.append("- No pair differed on exactly one fully-observed positive structural axis.")
    lines += [
        "",
        "## 9. Negative capacity",
        "",
        "- Core-v1 structural GRID 8/32: **NEGATIVE CAPACITY / intentionally unsupported**; GRID 16 is the only materialized structural grid in this checkpoint.",
        "- DEPTH as realization/transformation magnitude: **causal/distinct frozen capacity**, not negative capacity.",
        "- ROLE HIERARCHY VIA DEPTH: **frozen negative capacity unless the full-corpus DEPTH check above reports a role-change finding**.",
        "- Rejected phrase requests, consistently absent optional roles, no-observed-harmonic-movement realizations, and inert FEEL realizations are retained in `NegativeSignature`; they are not silently discarded.",
        "",
        "## 10. Observation limitations",
        "",
        "- Physical note duration is not exposed by this observation path, so duration is **NOT_OBSERVED**, never zero.",
        "- Physical DrumStep does not expose structural/secondary/ghost importance. Gate B therefore does not fabricate a structural drum-onset count or drum ghost label from velocity.",
        "- The neutral observation intentionally removes engine, oscillator, sample/kit, FX and cosmetic velocity identity. The V0R seam does not expose a separate positive sound/timbre identity suitable for pairwise proof, so **TIMBRE-DEPENDENT cannot be inferred from profile labels alone**.",
        "- Relative pitch is measured as pitch class relative to the shared tonal root plus interval contour. It does not claim a richer chord-function analysis than the production observation actually exposes.",
        "- Pairwise results are matched over the frozen seeds and DEPTH levels. They characterize this deterministic production corpus; they are not an exhaustive proof over every possible generation identity.",
        "",
        "## 11. Gate B conclusion",
        "",
        "Gate B freezes a production-backed, timbre-free measurement of the current engine's materialized structural capacity and its collisions/negative capacity. It deliberately makes **no recommendation** about deleting profiles, merging Genres, promoting Recipes, exposing controls, renaming DEPTH, redesigning UI, or building a next feature. Those interpretation decisions belong to **GF2-G1 — INTERPRET ACTUAL CAPACITY**.",
        "",
    ]
    return "\n".join(lines)


def generate_artifacts(
    raw_path: Path,
    seeds_path: Path,
    contract_path: Path,
    output_dir: Path,
) -> dict[str, Path]:
    seeds = load_seeds(seeds_path)
    contract = json.loads(contract_path.read_text(encoding="utf-8"))
    rows = _normalize_raw_rows(raw_path)
    profiles = _validate_corpus(rows, seeds, contract)
    _prepare_rows(rows)
    pairwise = _pair_rows(rows, profiles)
    expected_pairs = expected_pair_count(len(profiles))
    if len(pairwise) != expected_pairs:
        raise ValueError(f"pairwise completeness failure: {len(pairwise)} != {expected_pairs}")
    if expected_pairs != int(contract["pairwise_count"]):
        raise ValueError(
            f"pairwise contract drift: observed={expected_pairs} frozen={contract['pairwise_count']}"
        )
    if len({(row["profile_a"], row["profile_b"]) for row in pairwise}) != len(pairwise):
        raise ValueError("pairwise output contains duplicate unordered pairs")

    profiles_summary = _profile_rows(rows, profiles, pairwise)
    corpus = _materialized_rows(rows)
    output_dir.mkdir(parents=True, exist_ok=True)
    paths = {
        "corpus": output_dir / "GF2_GATE_B_MATERIALIZED_CORPUS.tsv",
        "profiles": output_dir / "GF2_GATE_B_PROFILE_SIGNATURES.tsv",
        "pairwise": output_dir / "GF2_GATE_B_PAIRWISE_DISTINCTNESS.tsv",
        "findings": output_dir / "GF2_GATE_B_FINDINGS.md",
    }
    _write_tsv(paths["corpus"], corpus)
    _write_tsv(paths["profiles"], profiles_summary)
    _write_tsv(paths["pairwise"], pairwise)
    paths["findings"].write_text(
        _findings(rows, profiles, profiles_summary, pairwise, contract),
        encoding="utf-8",
        newline="\n",
    )
    return paths


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raw", required=True, type=Path)
    parser.add_argument("--seeds", required=True, type=Path)
    parser.add_argument("--contract", required=True, type=Path)
    parser.add_argument("--output-dir", required=True, type=Path)
    args = parser.parse_args()

    paths = generate_artifacts(args.raw, args.seeds, args.contract, args.output_dir)
    for key in ("corpus", "profiles", "pairwise", "findings"):
        print(f"{key}={paths[key]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
