#!/usr/bin/env python3
from pathlib import Path
import re
import sys


def find_row(text: str, symbol: str) -> tuple[int, str]:
    for line in text.splitlines():
        if symbol not in line:
            continue
        match = re.search(r"\t(\d+)\t(static|dynamic,bounded|dynamic)$", line)
        if not match:
            continue
        return int(match.group(1)), line
    raise SystemExit(f"missing stack-usage symbol: {symbol}")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: test_0_9_9_e2a_stack_usage.py <stack.su>")
    text = Path(sys.argv[1]).read_text(encoding="utf-8")
    producer, producer_row = find_row(text, "produceRhythmMutationCandidates")
    topology, topology_row = find_row(text, "topologyCandidateSafe")
    if producer > 1024:
        raise SystemExit(f"producer host stack regression: {producer} B > 1024 B")
    if topology > 1024:
        raise SystemExit(f"topology helper host stack regression: {topology} B > 1024 B")
    print(f"E2A host stack producer={producer}B topology={topology}B")
    print(producer_row)
    print(topology_row)
    print("Host GCC .su only; not ESP32-S3 runtime HWM")


if __name__ == "__main__":
    main()
