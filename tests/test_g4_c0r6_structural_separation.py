#!/usr/bin/env python3
"""RED characterization for G4-C0R6 identity/take structural separation."""

from __future__ import annotations

import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))

from tools.gf2 import g4_c0r6_structural_separation as c0r6


def row(
    *,
    identity: int,
    attempt: int,
    bass_attack: str = "0x1111",
    bass_cont: str = "0x0000",
    secondary_attack: str = "0x4444",
    secondary_cont: str = "0x0000",
    archetype: str = "1",
    bass_rhythm: str = "2",
    chord_rhythm: str = "3",
    melodic_rhythm: str = "4",
    motif: str = "5",
    progression: str = "6",
    role: str = "CHORD",
    chord_axis: str = "ACTIVE",
    melodic_axis: str = "INACTIVE_BY_PHYSICAL_ROLE",
    motif_axis: str = "INACTIVE_BY_PHYSICAL_ROLE",
) -> dict[str, str]:
    return {
        "profile_ordinal": "0",
        "profile_id": "Acid/BASE",
        "depth": "P1",
        "identity_ordinal": str(identity),
        "generation_attempt_ordinal": str(attempt),
        "pattern_address": "23",
        "migration_status": "APPLIED",
        "selected_archetype": archetype,
        "selected_bass_rhythm": bass_rhythm,
        "selected_chord_rhythm": chord_rhythm,
        "selected_melodic_rhythm": melodic_rhythm,
        "selected_motif_shape": motif,
        "selected_progression": progression,
        "synth_b_role": role,
        "archetype_axis_status": "ACTIVE",
        "bass_axis_status": "ACTIVE",
        "chord_axis_status": chord_axis,
        "melodic_axis_status": melodic_axis,
        "motif_axis_status": motif_axis,
        "progression_axis_status": "ACTIVE",
        "phrase_law_axis_status": "PLANNING_ONLY",
        "bass_attack_mask": bass_attack,
        "bass_continuation_mask": bass_cont,
        "secondary_attack_mask": secondary_attack,
        "secondary_continuation_mask": secondary_cont,
        "secondary_topology_role": role,
        "rhythm_family": "MACHINE_SYNCOPATION",
        "planning_bass_onset_mask": bass_attack,
    }


class StructuralSeparationTest(unittest.TestCase):
    def test_inactive_axes_do_not_create_false_idea_diversity(self) -> None:
        first = row(identity=1, attempt=0, melodic_rhythm="4", motif="5")
        second = row(identity=1, attempt=1, melodic_rhythm="99", motif="88")

        self.assertEqual(c0r6.idea_signature(first), c0r6.idea_signature(second))

    def test_attempt_topology_change_is_take_variation_not_selection_drift(self) -> None:
        rows = [
            row(identity=1, attempt=0, bass_attack="0x1111"),
            row(identity=1, attempt=1, bass_attack="0x2222"),
            row(identity=1, attempt=2, bass_attack="0x1111"),
            row(identity=1, attempt=3, bass_attack="0x2222"),
        ]

        result = c0r6.analyze_group(rows)

        self.assertFalse(result.selection_drift)
        self.assertEqual(result.unique_take_signatures, 2)
        self.assertTrue(result.attempt_changes_topology)

    def test_attempt_changing_active_axis_is_selection_drift(self) -> None:
        rows = [
            row(identity=1, attempt=0, bass_rhythm="2"),
            row(identity=1, attempt=1, bass_rhythm="7"),
            row(identity=1, attempt=2, bass_rhythm="2"),
            row(identity=1, attempt=3, bass_rhythm="2"),
        ]

        result = c0r6.analyze_group(rows)

        self.assertTrue(result.selection_drift)

    def test_identical_four_take_structures_across_identities_are_collision(self) -> None:
        rows = []
        for identity in (1, 2):
            for attempt, mask in enumerate(("0x1111", "0x2222", "0x3333", "0x4444")):
                rows.append(row(identity=identity, attempt=attempt, bass_attack=mask))

        collisions = c0r6.structural_collision_groups(rows)

        self.assertEqual(len(collisions), 1)
        self.assertEqual(collisions[0], (1, 2))


if __name__ == "__main__":
    unittest.main()
