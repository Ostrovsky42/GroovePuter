#!/usr/bin/env python3
"""The checked-in bootstrap header must match deterministic registry generation."""

import subprocess
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/harmony/build_bootstrap_tables.py"
CHECKED_IN = ROOT / "src/generation/harmony/generated/harmonic_vocabulary.h"


def main():
    with tempfile.TemporaryDirectory(prefix="harmony-vocab-") as temporary:
        generated = Path(temporary) / "harmonic_vocabulary.h"
        subprocess.run(
            ["python3", str(SCRIPT), "--out", str(generated)],
            check=True,
            cwd=ROOT,
        )
        if generated.read_bytes() != CHECKED_IN.read_bytes():
            raise SystemExit("generated harmonic vocabulary differs from the checked-in table")
    print("harmonic registry generation byte parity: PASS")


if __name__ == "__main__":
    main()
