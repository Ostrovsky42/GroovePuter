#!/usr/bin/env python3
from __future__ import annotations

import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools" / "gf2_gate_b_finalize.py"
SPEC = importlib.util.spec_from_file_location("gf2_gate_b_finalize", MODULE_PATH)
if SPEC is None or SPEC.loader is None:
    raise RuntimeError("cannot load Gate B finalizer")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def raw_row(
    profile: str,
    seed: str,
    depth: str,
    *,
    role: str = "0",
    synth_b_onsets: str = "0000",
    density_min: str = "1",
    density_max: str = "8",
) -> dict[str, str]:
    return {
        "profile_id": profile,
        "seed": seed,
        "depth": depth,
        "synth_b_role": role,
        "synth_b_onsets": synth_b_onsets,
        "density_min": density_min,
        "density_max": density_max,
    }


def materialized_row(
    profile: str,
    seed: str,
    depth: str,
    *,
    rhythm: str,
    density: str,
    timing_events: str = "0",
    timing_max: str = "0",
    law: str = "LOOP",
    admitted: str = "YES",
    changed_bars: str = "0",
) -> dict[str, str]:
    return {
        "profile_id": profile,
        "seed": seed,
        "depth": depth,
        "rhythm_signature_id": rhythm,
        "resolved_density": density,
        "timing_displaced_events": timing_events,
        "max_timing_delta": timing_max,
        "declared_phrase_law": law,
        "phrase_admitted": admitted,
        "phrase_changed_bars": changed_bars,
    }


def test_depth_identity_is_separate_from_supporting_activity() -> None:
    rows = [
        raw_row("A/BASE", "0x1", "P1", role="0", synth_b_onsets="0000"),
        raw_row("A/BASE", "0x1", "P2", role="0", synth_b_onsets="0002"),
        raw_row("A/BASE", "0x1", "P3", role="0", synth_b_onsets="0002"),
    ]
    status = MODULE.depth_statuses(rows)[("A/BASE", "0x1")]
    assert status.role_identity_stable == "YES", status
    assert status.supporting_activity_stable == "NO", status


def test_real_role_reassignment_is_identity_change() -> None:
    rows = [
        raw_row("A/BASE", "0x1", "P1", role="0", synth_b_onsets="0002"),
        raw_row("A/BASE", "0x1", "P2", role="1", synth_b_onsets="0002"),
        raw_row("A/BASE", "0x1", "P3", role="1", synth_b_onsets="0002"),
    ]
    status = MODULE.depth_statuses(rows)[("A/BASE", "0x1")]
    assert status.role_identity_stable == "NO", status
    assert status.supporting_activity_stable == "YES", status


def test_feel_summary_uses_observed_tick_offsets_without_grid_scaling() -> None:
    rows = [
        materialized_row("A/BASE", "0x1", "P1", rhythm="R1", density="14"),
        materialized_row(
            "A/BASE",
            "0x1",
            "P2",
            rhythm="R1",
            density="14",
            timing_events="2",
            timing_max="1",
        ),
    ]
    result = MODULE.feel_summary(rows)
    assert result.effect_rows == 1, result
    assert result.zero_rows == 1, result
    assert result.max_abs_ticks == 1, result


def test_density_summary_does_not_confuse_corridor_minimum_with_resolved_target() -> None:
    raw = [
        raw_row("A/BASE", "0x1", "P1", density_min="1", density_max="8"),
        raw_row("A/BASE", "0x1", "P2", density_min="1", density_max="8"),
    ]
    materialized = [
        materialized_row("A/BASE", "0x1", "P1", rhythm="R1", density="14"),
        materialized_row("A/BASE", "0x1", "P2", rhythm="R2", density="14"),
    ]
    result = MODULE.density_summary(raw, materialized)
    assert result.corridor_min_values == (1,), result
    assert result.resolved_targets == (14,), result
    assert result.same_target_multi_topology_profiles == ("A/BASE",), result


def test_phrase_summary_counts_selected_weighted_law_per_realization() -> None:
    rows = [
        materialized_row(
            "A/BASE",
            "0x1",
            "P1",
            rhythm="R1",
            density="14",
            law="DEVELOP/RETURN",
            changed_bars="2",
        ),
        materialized_row(
            "A/BASE",
            "0x1",
            "P2",
            rhythm="R1",
            density="14",
            law="LOOP",
            changed_bars="0",
        ),
        materialized_row(
            "A/BASE",
            "0x1",
            "P3",
            rhythm="R1",
            density="14",
            law="DEVELOP/RETURN",
            changed_bars="1",
        ),
    ]
    result = MODULE.phrase_summary(rows)
    assert result.selected["DEVELOP/RETURN"] == 2, result
    assert result.admitted["DEVELOP/RETURN"] == 2, result
    assert result.changed["DEVELOP/RETURN"] == 2, result
    assert result.selected["LOOP"] == 1, result
    assert result.changed["LOOP"] == 0, result


def main() -> None:
    tests = [value for name, value in sorted(globals().items()) if name.startswith("test_")]
    for test in tests:
        test()
    print(f"GF2-C2 Gate B finalization regressions: {len(tests)} PASS")


if __name__ == "__main__":
    main()
