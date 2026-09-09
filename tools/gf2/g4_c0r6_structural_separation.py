#!/usr/bin/env python3
"""G4-C0R6 structural identity/take separation primitives.

Research-only. This module does not define musical Idea semantics. It projects
only production-observed active selection axes and pre-adapter semantic topology
so identity-owned selection can be distinguished from attempt-owned realization.
"""

from __future__ import annotations

from dataclasses import dataclass
from collections import defaultdict
from typing import Iterable, Mapping, Sequence


ACTIVE = "ACTIVE"


@dataclass(frozen=True)
class GroupAnalysis:
    selection_drift: bool
    unique_take_signatures: int
    attempt_changes_topology: bool


def idea_signature(row: Mapping[str, str]) -> tuple[tuple[str, str], ...]:
    """Project only selection fields whose production axis is ACTIVE."""
    values: list[tuple[str, str]] = [
        ("synth_b_role", row["synth_b_role"]),
        ("rhythm_family", row["rhythm_family"]),
    ]
    axis_fields = (
        ("archetype_axis_status", "selected_archetype"),
        ("bass_axis_status", "selected_bass_rhythm"),
        ("chord_axis_status", "selected_chord_rhythm"),
        ("melodic_axis_status", "selected_melodic_rhythm"),
        ("motif_axis_status", "selected_motif_shape"),
        ("progression_axis_status", "selected_progression"),
    )
    for status_field, value_field in axis_fields:
        if row[status_field] == ACTIVE:
            values.append((value_field, row[value_field]))
    return tuple(values)


def take_signature(row: Mapping[str, str]) -> tuple[tuple[str, str], ...]:
    """Project observed semantic topology, excluding pitch/timbre fingerprints."""
    return (
        ("bass_attack_mask", row["bass_attack_mask"].lower()),
        ("bass_continuation_mask", row["bass_continuation_mask"].lower()),
        ("secondary_attack_mask", row["secondary_attack_mask"].lower()),
        ("secondary_continuation_mask", row["secondary_continuation_mask"].lower()),
        ("secondary_topology_role", row["secondary_topology_role"]),
    )


def take_space(rows: Sequence[Mapping[str, str]]) -> tuple[tuple[tuple[str, str], ...], ...]:
    """Canonical sampled take multiset; TRY ordinal is deliberately non-musical."""
    return tuple(sorted(take_signature(row) for row in rows))


def analyze_group(rows: Sequence[Mapping[str, str]]) -> GroupAnalysis:
    if not rows:
        raise ValueError("identity group must not be empty")
    ideas = {idea_signature(row) for row in rows}
    takes = {take_signature(row) for row in rows}
    return GroupAnalysis(
        selection_drift=len(ideas) != 1,
        unique_take_signatures=len(takes),
        attempt_changes_topology=len(takes) > 1,
    )


def _identity_groups(
    rows: Iterable[Mapping[str, str]],
) -> dict[tuple[str, str, int], list[Mapping[str, str]]]:
    identities: dict[tuple[str, str, int], list[Mapping[str, str]]] = defaultdict(list)
    for row in rows:
        key = (
            row["profile_ordinal"],
            row["depth"],
            int(row["identity_ordinal"], 10),
        )
        identities[key].append(row)
    return identities


def structural_collision_groups(
    rows: Iterable[Mapping[str, str]],
) -> list[tuple[int, ...]]:
    """Return identities with identical active selection and sampled take space.

    Collisions are scoped by profile and depth. Attempt ordinals are excluded
    because TRY numbering is an engineering coordinate, not a musical decision.
    Multiplicity is preserved, so four identical takes remain distinct from a
    sampled space containing two copies of two different take topologies.
    """
    by_structure: dict[
        tuple[str, str, tuple[tuple[str, str], ...], tuple[tuple[tuple[str, str], ...], ...]],
        list[int],
    ] = defaultdict(list)

    for (profile, depth, identity), group in _identity_groups(rows).items():
        ideas = {idea_signature(row) for row in group}
        if len(ideas) != 1:
            continue
        structure = (
            profile,
            depth,
            next(iter(ideas)),
            take_space(group),
        )
        by_structure[structure].append(identity)

    collisions = [
        tuple(sorted(group))
        for group in by_structure.values()
        if len(group) > 1
    ]
    return sorted(collisions)


def topology_collision_groups(
    rows: Iterable[Mapping[str, str]],
) -> list[tuple[int, ...]]:
    """Return identities whose sampled audible topology converges.

    Active selection is intentionally ignored here. This catches cases where
    distinct intended selections collapse onto the same observed rhythmic/
    lifetime topology. Identities with internal selection drift are excluded
    because their identity-side contract is already unstable.
    """
    by_topology: dict[
        tuple[str, str, tuple[tuple[tuple[str, str], ...], ...]],
        list[int],
    ] = defaultdict(list)

    for (profile, depth, identity), group in _identity_groups(rows).items():
        if len({idea_signature(row) for row in group}) != 1:
            continue
        by_topology[(profile, depth, take_space(group))].append(identity)

    collisions = [
        tuple(sorted(group))
        for group in by_topology.values()
        if len(group) > 1
    ]
    return sorted(collisions)
