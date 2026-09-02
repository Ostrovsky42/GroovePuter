#!/usr/bin/env python3
from __future__ import annotations

import pathlib
import sys


def fail(message: str) -> None:
    raise AssertionError(message)


def parse_stack_usage(path: pathlib.Path) -> dict[str, tuple[int, str]]:
    entries: dict[str, tuple[int, str]] = {}
    for raw in path.read_text(encoding="utf-8").splitlines():
        if not raw.strip():
            continue
        parts = raw.rsplit("\t", 2)
        if len(parts) != 3:
            continue
        symbol, bytes_text, kind = parts
        try:
            size = int(bytes_text)
        except ValueError:
            continue
        entries[symbol] = (size, kind)
    return entries


def find(entries: dict[str, tuple[int, str]], needle: str) -> tuple[str, int, str]:
    matches = [
        (symbol, size, kind)
        for symbol, (size, kind) in entries.items()
        if needle in symbol
    ]
    if not matches:
        fail(f"stack-usage entry not found for {needle}")
    return max(matches, key=lambda item: item[1])


def require_bounded(symbol: str, size: int, kind: str, ceiling: int) -> None:
    # GCC may report `dynamic,bounded` even when there is no C/C++ VLA. The
    # runner separately compiles with -Wvla -Werror. Here we require a numeric
    # compiler bound and reject unbounded `dynamic` classifications.
    if kind not in {"static", "dynamic,bounded"}:
        fail(f"stack usage is not compiler-bounded: {symbol} {kind}")
    if size > ceiling:
        fail(f"host stack frame too large: {symbol} {size} bytes > {ceiling}")


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit(
            "usage: test_rhythm_stage6_1_stack_usage.py "
            "<bar_evolution.su> <rhythm_realizer_evolution.su>"
        )

    planner_path = pathlib.Path(sys.argv[1])
    mutation_path = pathlib.Path(sys.argv[2])
    for path in (planner_path, mutation_path):
        if not path.is_file():
            fail(f"stack-usage file missing: {path}")

    planner_entries = parse_stack_usage(planner_path)
    mutation_entries = parse_stack_usage(mutation_path)

    evolve_symbol, evolve_bytes, evolve_kind = find(
        planner_entries, "evolveRhythmPhrase"
    )
    mutate_symbol, mutate_bytes, mutate_kind = find(
        mutation_entries, "applyRhythmBarFunctionMutation"
    )
    drop_symbol, drop_bytes, drop_kind = find(
        mutation_entries, "dropOneStructuralEvent"
    )

    # Host GCC is not the ESP32-S3 ABI, so these are regression ceilings rather
    # than hardware high-water measurements. They catch accidental frame growth
    # before/alongside the Cardputer runtime probe.
    require_bounded(evolve_symbol, evolve_bytes, evolve_kind, 4096)
    require_bounded(mutate_symbol, mutate_bytes, mutate_kind, 2048)
    require_bounded(drop_symbol, drop_bytes, drop_kind, 2048)

    print(
        "Groove Vocabulary Stage 6.1 stack usage: "
        f"planner={evolve_bytes}B({evolve_kind}) "
        f"mutation={mutate_bytes}B({mutate_kind}) "
        f"drop={drop_bytes}B({drop_kind})"
    )
    print("Host GCC .su only; not ESP32-S3 runtime HWM")


if __name__ == "__main__":
    main()
