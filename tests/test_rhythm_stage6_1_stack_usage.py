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
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_rhythm_stage6_1_stack_usage.py <bar_evolution.su>")

    path = pathlib.Path(sys.argv[1])
    if not path.is_file():
        fail(f"stack-usage file missing: {path}")

    entries = parse_stack_usage(path)
    evolve_symbol, evolve_bytes, evolve_kind = find(entries, "evolveRhythmPhrase")
    drop_symbol, drop_bytes, drop_kind = find(entries, "dropOneStructuralEvent")

    # Host GCC is not the ESP32-S3 ABI, so these are regression ceilings rather
    # than hardware high-water measurements. They catch accidental frame growth
    # before the mandatory Cardputer probe at first production wiring.
    require_bounded(evolve_symbol, evolve_bytes, evolve_kind, 4096)
    require_bounded(drop_symbol, drop_bytes, drop_kind, 2048)

    print(
        "Groove Vocabulary Stage 6.1 stack usage: "
        f"evolve={evolve_bytes}B({evolve_kind}) "
        f"drop={drop_bytes}B({drop_kind})"
    )


if __name__ == "__main__":
    main()
