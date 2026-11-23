"""Source separation with the demucs CLI (htdemucs, 4 stems).

The CLI is the most stable interface across demucs releases, so we shell out.
Output: <out>/htdemucs/<audio stem>/{drums,bass,other,vocals}.wav
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def separate(audio_path, out_dir, model: str = "htdemucs", device: str = "cpu") -> dict[str, Path]:
    audio_path, out_dir = Path(audio_path), Path(out_dir)
    stem_dir = out_dir / model / audio_path.stem
    want = {s: stem_dir / f"{s}.wav" for s in ("drums", "bass")}
    if all(p.exists() for p in want.values()):
        return want
    subprocess.run([sys.executable, "-m", "demucs", "-n", model, "-d", device,
                    "-o", str(out_dir), str(audio_path)], check=True)
    missing = [p for p in want.values() if not p.exists()]
    if missing:
        raise RuntimeError(f"demucs finished but stems are missing: {missing}")
    return want
