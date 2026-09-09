#!/usr/bin/env python3
"""G4-C0R6 deterministic baseline-to-I6 causal delta attribution.

Research-only. This analyzer joins two already-observed C0R5 corpora by
(profile_ordinal, identity_ordinal), classifies literal transitions, and emits
transparent evidence. It owns no musical policy and assigns no quality score.
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

TOPOLOGY_CLASSES = (
    "EXACT_MATCH",
    "PARTIAL_OVERLAP",
    "DISJOINT_NONEMPTY",
    "PLANNING_EMPTY_AUDIBLE_NONEMPTY",
    "AUDIBLE_EMPTY_PLANNING_NONEMPTY",
    "BOTH_EMPTY",
)

NATIVE_CLASSES = ("NATIVE", "OUTSIDE_NATIVE_SET")
NATIVE_TRANSITIONS = (
    "NATIVE_STABLE",
    "OUTSIDE_STABLE",
    "OUTSIDE_TO_NATIVE",
    "NATIVE_TO_OUTSIDE",
)
CHANGE_SOURCES = (
    "NO_CHANGE",
    "SELECTION_CHANGE",
    "NATIVE_SET_CHANGE",
    "PLANNING_CHANGE",
    "AUDIBLE_CHANGE",
    "MULTI_LAYER_CHANGE",
)

REQUIRED_INPUT_FIELDS = {
    "profile_ordinal",
    "profile_id",
    "depth",
    "identity_ordinal",
    "generation_attempt_ordinal",
    "pattern_address",
    "migration_status",
    "selected_archetype",
    "rhythm_family",
    "selected_bass_rhythm",
    "bass_native_candidate_mask",
    "bass_native_membership",
    "planning_bass_onset_mask",
    "bass_attack_mask",
    "planning_vs_audible",
}

OPTIONAL_INPUT_FIELDS = (
    "planning_bass_structural_mask",
    "planning_bass_secondary_mask",
    "planning_bass_ghost_mask",
    "bass_continuation_mask",
    "selected_chord_rhythm",
    "selected_melodic_rhythm",
    "selected_motif_shape",
    "selected_progression",
    "synth_b_role",
)

ROW_FIELDS = (
    "profile_ordinal",
    "profile_id",
    "identity_ordinal",
    "baseline_archetype",
    "i6_archetype",
    "archetype_changed",
    "baseline_rhythm_family",
    "i6_rhythm_family",
    "rhythm_family_changed",
    "baseline_bass_rhythm",
    "i6_bass_rhythm",
    "bass_rhythm_changed",
    "baseline_native_mask",
    "i6_native_mask",
    "native_mask_changed",
    "baseline_native_membership",
    "i6_native_membership",
    "native_transition",
    "baseline_planning_mask",
    "i6_planning_mask",
    "planning_changed",
    "baseline_audible_mask",
    "i6_audible_mask",
    "audible_changed",
    "baseline_topology",
    "i6_topology",
    "topology_transition",
    "change_source",
    "selection_layer_changed",
) + tuple(
    f"baseline_{field}" for field in OPTIONAL_INPUT_FIELDS
) + tuple(
    f"i6_{field}" for field in OPTIONAL_INPUT_FIELDS
)

SUMMARY_FIELDS = (
    "profile_ordinal",
    "profile_id",
    "rows",
    "unchanged_rows",
    "changed_rows",
    "baseline_native",
    "baseline_outside",
    "i6_native",
    "i6_outside",
    "native_stable",
    "outside_stable",
    "outside_to_native",
    "native_to_outside",
    "baseline_planning_empty",
    "i6_planning_empty",
    "topology_changed_rows",
    "archetype_changed_rows",
    "family_changed_rows",
    "bass_choice_changed_rows",
    "native_set_changed_rows",
    "planning_changed_rows",
    "audible_changed_rows",
    "selection_change_rows",
    "native_set_only_rows",
    "planning_only_rows",
    "audible_only_rows",
    "multi_layer_rows",
)

TRANSITION_FIELDS = (
    "profile_ordinal",
    "profile_id",
    "transition",
    "rows",
    "archetype_changed_rows",
    "family_changed_rows",
    "bass_choice_changed_rows",
    "native_set_changed_rows",
    "planning_changed_rows",
    "audible_changed_rows",
)


class CausalDeltaError(RuntimeError):
    pass


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", required=True, type=Path)
    parser.add_argument("--i6", required=True, type=Path)
    parser.add_argument("--rows-output", required=True, type=Path)
    parser.add_argument("--summary-output", required=True, type=Path)
    parser.add_argument("--native-output", required=True, type=Path)
    parser.add_argument("--topology-output", required=True, type=Path)
    parser.add_argument("--report-output", required=True, type=Path)
    return parser.parse_args()


def parse_mask(text: str, field: str, source: str, coordinate: Tuple[str, str]) -> int:
    if text in ("", "NOT_OBSERVED", "INVALID"):
        raise CausalDeltaError(
            f"{source} {coordinate}: {field} not observed: {text!r}"
        )
    try:
        value = int(text, 16)
    except ValueError as exc:
        raise CausalDeltaError(
            f"{source} {coordinate}: invalid {field}: {text!r}"
        ) from exc
    if not 0 <= value <= 0xFFFF:
        raise CausalDeltaError(
            f"{source} {coordinate}: {field} outside 16-bit range: {text!r}"
        )
    return value


def canonical_mask(value: int) -> str:
    return f"0x{value:04x}"


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


def native_transition(before: str, after: str) -> str:
    pair = (before, after)
    mapping = {
        ("NATIVE", "NATIVE"): "NATIVE_STABLE",
        ("OUTSIDE_NATIVE_SET", "OUTSIDE_NATIVE_SET"): "OUTSIDE_STABLE",
        ("OUTSIDE_NATIVE_SET", "NATIVE"): "OUTSIDE_TO_NATIVE",
        ("NATIVE", "OUTSIDE_NATIVE_SET"): "NATIVE_TO_OUTSIDE",
    }
    try:
        return mapping[pair]
    except KeyError as exc:
        raise CausalDeltaError(f"invalid native membership transition: {pair}") from exc


def bool_text(value: bool) -> str:
    return "1" if value else "0"


def classify_change_source(
    selection_changed: bool,
    native_set_changed: bool,
    planning_changed: bool,
    audible_changed: bool,
) -> str:
    layers = [
        ("SELECTION_CHANGE", selection_changed),
        ("NATIVE_SET_CHANGE", native_set_changed),
        ("PLANNING_CHANGE", planning_changed),
        ("AUDIBLE_CHANGE", audible_changed),
    ]
    active = [name for name, changed in layers if changed]
    if not active:
        return "NO_CHANGE"
    if len(active) == 1:
        return active[0]
    return "MULTI_LAYER_CHANGE"


def load_corpus(path: Path, source: str) -> Dict[Tuple[str, str], Dict[str, str]]:
    try:
        with path.open(newline="", encoding="utf-8") as handle:
            reader = csv.DictReader(handle, delimiter="\t")
            if reader.fieldnames is None:
                raise CausalDeltaError(f"{source}: missing header")
            missing = REQUIRED_INPUT_FIELDS - set(reader.fieldnames)
            if missing:
                raise CausalDeltaError(f"{source}: missing fields {sorted(missing)}")
            rows = [dict(row) for row in reader]
    except OSError as exc:
        raise CausalDeltaError(f"cannot read {source} corpus {path}: {exc}") from exc

    if len(rows) != 512:
        raise CausalDeltaError(f"{source}: expected 512 rows, got {len(rows)}")

    result: Dict[Tuple[str, str], Dict[str, str]] = {}
    expected = {
        (profile, str(identity))
        for profile in EXPECTED_PROFILES
        for identity in range(128)
    }

    for row in rows:
        coordinate = (row["profile_ordinal"], row["identity_ordinal"])
        if coordinate in result:
            raise CausalDeltaError(f"{source}: duplicate coordinate {coordinate}")
        profile, identity = coordinate
        if profile not in EXPECTED_PROFILES:
            raise CausalDeltaError(f"{source}: unexpected profile ordinal {profile}")
        if row["profile_id"] != EXPECTED_PROFILES[profile]:
            raise CausalDeltaError(
                f"{source} {coordinate}: profile binding {row['profile_id']!r}"
            )
        try:
            identity_value = int(identity, 10)
        except ValueError as exc:
            raise CausalDeltaError(f"{source}: invalid identity {identity!r}") from exc
        if not 0 <= identity_value < 128:
            raise CausalDeltaError(f"{source}: identity outside 0..127: {identity}")
        if row["depth"] != "P1":
            raise CausalDeltaError(f"{source} {coordinate}: depth != P1")
        if row["generation_attempt_ordinal"] != "0":
            raise CausalDeltaError(f"{source} {coordinate}: attempt != 0")
        if row["pattern_address"] != "23":
            raise CausalDeltaError(f"{source} {coordinate}: pattern address != 23")
        if row["migration_status"] != "APPLIED":
            raise CausalDeltaError(
                f"{source} {coordinate}: migration {row['migration_status']!r}"
            )
        if row["bass_native_membership"] not in NATIVE_CLASSES:
            raise CausalDeltaError(
                f"{source} {coordinate}: invalid native membership "
                f"{row['bass_native_membership']!r}"
            )

        planning = parse_mask(row["planning_bass_onset_mask"], "planning mask", source, coordinate)
        audible = parse_mask(row["bass_attack_mask"], "audible mask", source, coordinate)
        native = parse_mask(
            row["bass_native_candidate_mask"], "native candidate mask", source, coordinate
        )
        if native == 0:
            raise CausalDeltaError(f"{source} {coordinate}: empty native candidate mask")
        observed_topology = classify_topology(planning, audible)
        if row["planning_vs_audible"] != observed_topology:
            raise CausalDeltaError(
                f"{source} {coordinate}: topology field {row['planning_vs_audible']!r} "
                f"!= recomputed {observed_topology!r}"
            )
        row["planning_bass_onset_mask"] = canonical_mask(planning)
        row["bass_attack_mask"] = canonical_mask(audible)
        row["bass_native_candidate_mask"] = canonical_mask(native)
        result[coordinate] = row

    actual = set(result)
    if actual != expected:
        raise CausalDeltaError(
            f"{source}: coordinate mismatch missing={sorted(expected-actual)[:8]} "
            f"extra={sorted(actual-expected)[:8]}"
        )
    return result


def joined_rows(
    baseline: Mapping[Tuple[str, str], Dict[str, str]],
    i6: Mapping[Tuple[str, str], Dict[str, str]],
) -> List[Dict[str, str]]:
    if set(baseline) != set(i6):
        raise CausalDeltaError("baseline and I6 coordinate sets differ")

    output: List[Dict[str, str]] = []
    for coordinate in sorted(baseline, key=lambda c: (int(c[0]), int(c[1]))):
        before = baseline[coordinate]
        after = i6[coordinate]
        if before["profile_id"] != after["profile_id"]:
            raise CausalDeltaError(f"{coordinate}: profile id changed")

        archetype_changed = before["selected_archetype"] != after["selected_archetype"]
        family_changed = before["rhythm_family"] != after["rhythm_family"]
        bass_changed = before["selected_bass_rhythm"] != after["selected_bass_rhythm"]
        native_mask_changed = (
            before["bass_native_candidate_mask"] != after["bass_native_candidate_mask"]
        )
        planning_changed = (
            before["planning_bass_onset_mask"] != after["planning_bass_onset_mask"]
        )
        audible_changed = before["bass_attack_mask"] != after["bass_attack_mask"]
        selection_changed = archetype_changed or family_changed or bass_changed

        native = native_transition(
            before["bass_native_membership"], after["bass_native_membership"]
        )
        topology_transition = (
            f"{before['planning_vs_audible']}->{after['planning_vs_audible']}"
        )
        change_source = classify_change_source(
            selection_changed, native_mask_changed, planning_changed, audible_changed
        )

        row = {
            "profile_ordinal": coordinate[0],
            "profile_id": before["profile_id"],
            "identity_ordinal": coordinate[1],
            "baseline_archetype": before["selected_archetype"],
            "i6_archetype": after["selected_archetype"],
            "archetype_changed": bool_text(archetype_changed),
            "baseline_rhythm_family": before["rhythm_family"],
            "i6_rhythm_family": after["rhythm_family"],
            "rhythm_family_changed": bool_text(family_changed),
            "baseline_bass_rhythm": before["selected_bass_rhythm"],
            "i6_bass_rhythm": after["selected_bass_rhythm"],
            "bass_rhythm_changed": bool_text(bass_changed),
            "baseline_native_mask": before["bass_native_candidate_mask"],
            "i6_native_mask": after["bass_native_candidate_mask"],
            "native_mask_changed": bool_text(native_mask_changed),
            "baseline_native_membership": before["bass_native_membership"],
            "i6_native_membership": after["bass_native_membership"],
            "native_transition": native,
            "baseline_planning_mask": before["planning_bass_onset_mask"],
            "i6_planning_mask": after["planning_bass_onset_mask"],
            "planning_changed": bool_text(planning_changed),
            "baseline_audible_mask": before["bass_attack_mask"],
            "i6_audible_mask": after["bass_attack_mask"],
            "audible_changed": bool_text(audible_changed),
            "baseline_topology": before["planning_vs_audible"],
            "i6_topology": after["planning_vs_audible"],
            "topology_transition": topology_transition,
            "change_source": change_source,
            "selection_layer_changed": bool_text(selection_changed),
        }
        for field in OPTIONAL_INPUT_FIELDS:
            row[f"baseline_{field}"] = before.get(field, "")
            row[f"i6_{field}"] = after.get(field, "")
        output.append(row)
    return output


def flag_count(rows: Sequence[Mapping[str, str]], field: str) -> int:
    return sum(row[field] == "1" for row in rows)


def summarize(rows: Sequence[Dict[str, str]]) -> List[Dict[str, str]]:
    groups: Dict[Tuple[int, str], List[Dict[str, str]]] = defaultdict(list)
    for row in rows:
        groups[(int(row["profile_ordinal"]), row["profile_id"])].append(row)

    result: List[Dict[str, str]] = []
    for (ordinal, profile_id), group in sorted(groups.items()):
        native = Counter(row["native_transition"] for row in group)
        sources = Counter(row["change_source"] for row in group)
        result.append({
            "profile_ordinal": str(ordinal),
            "profile_id": profile_id,
            "rows": str(len(group)),
            "unchanged_rows": str(sources["NO_CHANGE"]),
            "changed_rows": str(len(group) - sources["NO_CHANGE"]),
            "baseline_native": str(sum(r["baseline_native_membership"] == "NATIVE" for r in group)),
            "baseline_outside": str(sum(r["baseline_native_membership"] == "OUTSIDE_NATIVE_SET" for r in group)),
            "i6_native": str(sum(r["i6_native_membership"] == "NATIVE" for r in group)),
            "i6_outside": str(sum(r["i6_native_membership"] == "OUTSIDE_NATIVE_SET" for r in group)),
            "native_stable": str(native["NATIVE_STABLE"]),
            "outside_stable": str(native["OUTSIDE_STABLE"]),
            "outside_to_native": str(native["OUTSIDE_TO_NATIVE"]),
            "native_to_outside": str(native["NATIVE_TO_OUTSIDE"]),
            "baseline_planning_empty": str(sum(r["baseline_topology"] == "PLANNING_EMPTY_AUDIBLE_NONEMPTY" for r in group)),
            "i6_planning_empty": str(sum(r["i6_topology"] == "PLANNING_EMPTY_AUDIBLE_NONEMPTY" for r in group)),
            "topology_changed_rows": str(sum(r["baseline_topology"] != r["i6_topology"] for r in group)),
            "archetype_changed_rows": str(flag_count(group, "archetype_changed")),
            "family_changed_rows": str(flag_count(group, "rhythm_family_changed")),
            "bass_choice_changed_rows": str(flag_count(group, "bass_rhythm_changed")),
            "native_set_changed_rows": str(flag_count(group, "native_mask_changed")),
            "planning_changed_rows": str(flag_count(group, "planning_changed")),
            "audible_changed_rows": str(flag_count(group, "audible_changed")),
            "selection_change_rows": str(sources["SELECTION_CHANGE"]),
            "native_set_only_rows": str(sources["NATIVE_SET_CHANGE"]),
            "planning_only_rows": str(sources["PLANNING_CHANGE"]),
            "audible_only_rows": str(sources["AUDIBLE_CHANGE"]),
            "multi_layer_rows": str(sources["MULTI_LAYER_CHANGE"]),
        })
    return result


def transition_table(rows: Sequence[Dict[str, str]], field: str) -> List[Dict[str, str]]:
    groups: Dict[Tuple[int, str, str], List[Dict[str, str]]] = defaultdict(list)
    for row in rows:
        groups[(int(row["profile_ordinal"]), row["profile_id"], row[field])].append(row)
    result: List[Dict[str, str]] = []
    for (ordinal, profile_id, transition), group in sorted(groups.items()):
        result.append({
            "profile_ordinal": str(ordinal),
            "profile_id": profile_id,
            "transition": transition,
            "rows": str(len(group)),
            "archetype_changed_rows": str(flag_count(group, "archetype_changed")),
            "family_changed_rows": str(flag_count(group, "rhythm_family_changed")),
            "bass_choice_changed_rows": str(flag_count(group, "bass_rhythm_changed")),
            "native_set_changed_rows": str(flag_count(group, "native_mask_changed")),
            "planning_changed_rows": str(flag_count(group, "planning_changed")),
            "audible_changed_rows": str(flag_count(group, "audible_changed")),
        })
    return result


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
        raise CausalDeltaError(f"cannot write {path}: {exc}") from exc


def subset(rows: Sequence[Dict[str, str]], profile: str) -> List[Dict[str, str]]:
    return [row for row in rows if row["profile_ordinal"] == profile]


def causal_combo(rows: Sequence[Dict[str, str]]) -> Tuple[int, int, int, int]:
    bass_only = native_only = both = neither = 0
    for row in rows:
        bass = row["bass_rhythm_changed"] == "1"
        native = row["native_mask_changed"] == "1"
        if bass and native:
            both += 1
        elif bass:
            bass_only += 1
        elif native:
            native_only += 1
        else:
            neither += 1
    return bass_only, native_only, both, neither


def plan_audible_combo(rows: Sequence[Dict[str, str]]) -> Tuple[int, int, int, int]:
    planning_only = audible_only = both = neither = 0
    for row in rows:
        planning = row["planning_changed"] == "1"
        audible = row["audible_changed"] == "1"
        if planning and audible:
            both += 1
        elif planning:
            planning_only += 1
        elif audible:
            audible_only += 1
        else:
            neither += 1
    return planning_only, audible_only, both, neither


def top_groups(
    rows: Sequence[Dict[str, str]],
    fields: Sequence[str],
    limit: int = 8,
) -> List[Tuple[Tuple[str, ...], int]]:
    counts = Counter(tuple(row[field] for field in fields) for row in rows)
    return sorted(counts.items(), key=lambda item: (-item[1], item[0]))[:limit]


def render_groups(groups: Sequence[Tuple[Tuple[str, ...], int]], fields: Sequence[str]) -> List[str]:
    if not groups:
        return ["- none"]
    lines = []
    for values, count in groups:
        detail = ", ".join(f"{field}={value}" for field, value in zip(fields, values))
        lines.append(f"- {count}: {detail}")
    return lines


def report(rows: Sequence[Dict[str, str]], summaries: Sequence[Dict[str, str]]) -> str:
    by_summary = {row["profile_ordinal"]: row for row in summaries}
    acid = subset(rows, "0")
    dub = subset(rows, "9")
    house = subset(rows, "20")
    dnb = subset(rows, "27")

    dnb_out_to_native = [r for r in dnb if r["native_transition"] == "OUTSIDE_TO_NATIVE"]
    dnb_bass_only, dnb_native_only, dnb_both, dnb_neither = causal_combo(dnb_out_to_native)
    dnb_new_disjoint = [
        r for r in dnb
        if r["i6_topology"] == "DISJOINT_NONEMPTY"
        and r["baseline_topology"] != "DISJOINT_NONEMPTY"
    ]
    dnb_plan_only, dnb_aud_only, dnb_plan_aud_both, dnb_plan_aud_neither = plan_audible_combo(dnb_new_disjoint)

    dub_nat_to_out = [r for r in dub if r["native_transition"] == "NATIVE_TO_OUTSIDE"]
    dub_group_fields = (
        "baseline_rhythm_family", "i6_rhythm_family",
        "baseline_bass_rhythm", "i6_bass_rhythm",
        "baseline_native_mask", "i6_native_mask",
    )

    house_out_to_native = [r for r in house if r["native_transition"] == "OUTSIDE_TO_NATIVE"]
    house_nat_to_out = [r for r in house if r["native_transition"] == "NATIVE_TO_OUTSIDE"]
    house_planning_resolved = [
        r for r in house
        if r["baseline_topology"] == "PLANNING_EMPTY_AUDIBLE_NONEMPTY"
        and r["i6_topology"] != "PLANNING_EMPTY_AUDIBLE_NONEMPTY"
    ]
    h_plan_only, h_aud_only, h_both, h_neither = plan_audible_combo(house_planning_resolved)
    house_residual = [r for r in house if r["i6_native_membership"] == "OUTSIDE_NATIVE_SET"]
    house_group_fields = (
        "i6_archetype", "i6_rhythm_family", "i6_bass_rhythm", "i6_native_mask"
    )

    topology_changed = [r for r in rows if r["baseline_topology"] != r["i6_topology"]]
    compat_changed = [r for r in rows if r["native_transition"] in {"OUTSIDE_TO_NATIVE", "NATIVE_TO_OUTSIDE"}]
    compat_only = [r for r in compat_changed if r["planning_changed"] == "0" and r["audible_changed"] == "0"]
    planning_owner = [r for r in topology_changed if r["planning_changed"] == "1" and r["audible_changed"] == "0"]
    audible_owner = [r for r in topology_changed if r["planning_changed"] == "0" and r["audible_changed"] == "1"]
    cross_layer = [r for r in topology_changed if r["planning_changed"] == "1" and r["audible_changed"] == "1"]

    global_sources = Counter(r["change_source"] for r in rows)
    next_checkpoint = (
        "G4-I7 Dub bass-role compatibility diagnosis"
        if dub_nat_to_out
        else "G4-I7 DnB role-plan coherence diagnosis"
        if dnb_new_disjoint
        else "G4-C0R7 deeper observation"
    )

    lines = [
        "# G4-C0R6 Causal Delta Attribution",
        "",
        "This report describes observed structural transitions only. NATIVE is not a quality score; DISJOINT is not automatically an error.",
        "",
        "## Profile summary",
        "",
        "| Profile | Unchanged | Changed | OUTSIDE baseline→I6 | OUTSIDE→NATIVE | NATIVE→OUTSIDE | Topology changed |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: |",
    ]
    for ordinal in ("0", "9", "20", "27"):
        s = by_summary[ordinal]
        lines.append(
            f"| {s['profile_id']} | {s['unchanged_rows']} | {s['changed_rows']} | "
            f"{s['baseline_outside']}→{s['i6_outside']} | {s['outside_to_native']} | "
            f"{s['native_to_outside']} | {s['topology_changed_rows']} |"
        )

    lines += [
        "",
        "## Acid control",
        "",
        f"- Row-level unchanged: {by_summary['0']['unchanged_rows']}/128.",
        f"- Row-level changed: {by_summary['0']['changed_rows']}/128.",
        f"- OUTSIDE_NATIVE_SET: {by_summary['0']['baseline_outside']}→{by_summary['0']['i6_outside']}.",
        f"- Topology changed rows: {by_summary['0']['topology_changed_rows']}/128.",
        "",
        "## DnB attribution",
        "",
        f"- OUTSIDE_TO_NATIVE: {len(dnb_out_to_native)}.",
        f"- Among OUTSIDE_TO_NATIVE: bass-choice-only={dnb_bass_only}, native-set-only={dnb_native_only}, both={dnb_both}, neither={dnb_neither}.",
        f"- Newly DISJOINT_NONEMPTY: {len(dnb_new_disjoint)}.",
        f"- Among newly disjoint: planning-only-changed={dnb_plan_only}, audible-only-changed={dnb_aud_only}, both-changed={dnb_plan_aud_both}, neither={dnb_plan_aud_neither}.",
        "",
        "## Dub Techno attribution",
        "",
        f"- NATIVE_TO_OUTSIDE: {len(dub_nat_to_out)}.",
        "- Dominant NATIVE_TO_OUTSIDE groups:",
    ]
    lines += render_groups(top_groups(dub_nat_to_out, dub_group_fields), dub_group_fields)

    lines += [
        "",
        "## House attribution",
        "",
        f"- OUTSIDE_TO_NATIVE: {len(house_out_to_native)}.",
        f"- NATIVE_TO_OUTSIDE: {len(house_nat_to_out)}.",
        f"- Planning-empty rows that became non-empty topology: {len(house_planning_resolved)}.",
        f"- Among those rows: planning-only-changed={h_plan_only}, audible-only-changed={h_aud_only}, both-changed={h_both}, neither={h_neither}.",
        f"- Residual I6 OUTSIDE_NATIVE_SET: {len(house_residual)}.",
        "- Dominant residual groups:",
    ]
    lines += render_groups(top_groups(house_residual, house_group_fields), house_group_fields)

    lines += [
        "",
        "## Cross-profile change-source classification",
        "",
    ]
    for source in CHANGE_SOURCES:
        lines.append(f"- {source}: {global_sources[source]}")

    lines += [
        "",
        "## Architectural evidence matrix",
        "",
        f"- A. Compatibility-only evidence: {len(compat_only)} membership-transition rows changed without planning or audible masks changing.",
        f"- B. Planning-owner evidence: {len(planning_owner)} topology-transition rows changed planning while audible stayed byte-stable.",
        f"- C. Audible-realization evidence: {len(audible_owner)} topology-transition rows changed audible while planning stayed byte-stable.",
        f"- D. Cross-layer ownership evidence: {len(cross_layer)} topology-transition rows changed both planning and audible masks.",
        "- E. Insufficient-evidence rule remains active: these counts establish causal layer changes, not musical correctness.",
        "",
        "## Decision",
        "",
        "G4 PRODUCTION: BLOCKED",
        "",
        f"NEXT: {next_checkpoint}",
        "",
        "Production changes remain blocked until the dominant row-level transition groups are interpreted against the genre contract.",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    args = parse_args()
    try:
        baseline = load_corpus(args.baseline, "baseline")
        i6 = load_corpus(args.i6, "I6")
        rows = joined_rows(baseline, i6)
        summaries = summarize(rows)
        native = transition_table(rows, "native_transition")
        topology = transition_table(rows, "topology_transition")

        if len(rows) != 512 or len(summaries) != 4:
            raise CausalDeltaError("internal output cardinality failure")
        for row in rows:
            if row["native_transition"] not in NATIVE_TRANSITIONS:
                raise CausalDeltaError(f"unknown native transition {row['native_transition']}")
            if row["baseline_topology"] not in TOPOLOGY_CLASSES or row["i6_topology"] not in TOPOLOGY_CLASSES:
                raise CausalDeltaError("unknown topology class")
            if row["change_source"] not in CHANGE_SOURCES:
                raise CausalDeltaError(f"unknown change source {row['change_source']}")

        write_tsv(args.rows_output, rows, ROW_FIELDS)
        write_tsv(args.summary_output, summaries, SUMMARY_FIELDS)
        write_tsv(args.native_output, native, TRANSITION_FIELDS)
        write_tsv(args.topology_output, topology, TRANSITION_FIELDS)
        args.report_output.parent.mkdir(parents=True, exist_ok=True)
        args.report_output.write_text(report(rows, summaries), encoding="utf-8", newline="\n")

        for summary in summaries:
            print(
                "C0R6",
                summary["profile_id"],
                f"unchanged={summary['unchanged_rows']}/128",
                f"outside={summary['baseline_outside']}->{summary['i6_outside']}",
                f"out_to_native={summary['outside_to_native']}",
                f"native_to_out={summary['native_to_outside']}",
                f"topology_changed={summary['topology_changed_rows']}",
            )
        return 0
    except (CausalDeltaError, OSError) as exc:
        print(f"G4_C0R6_ANALYSIS_FAIL reason={exc}", file=__import__("sys").stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
