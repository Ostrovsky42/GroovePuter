"""Beats and downbeats in seconds.

beat_this  CPJKU "Beat This!" (ISMIR 2024, MIT). pip install from its GitHub.
file       precomputed TSV next to the audio: <audio>.beats.tsv, "time<TAB>beat_number"
           (beat_number 1 = downbeat) — the format beat_this's CLI and many tools emit.
"""
from __future__ import annotations

from pathlib import Path

import numpy as np

_f2b = None


def track_beat_this(audio_path, device: str = "cpu"):
    global _f2b
    if _f2b is None:
        from beat_this.inference import File2Beats  # lazy: requirements-ml
        _f2b = File2Beats(checkpoint_path="final0", device=device, dbn=False)
    beats, downbeats = _f2b(str(audio_path))
    return np.asarray(beats, float), np.asarray(downbeats, float)


def track_file(audio_path):
    tsv = Path(str(audio_path) + ".beats.tsv")
    rows = [ln.split() for ln in tsv.read_text().splitlines() if ln.strip()]
    beats = np.array([float(r[0]) for r in rows])
    downbeats = np.array([float(r[0]) for r in rows if len(r) > 1 and int(float(r[1])) == 1])
    return beats, downbeats


def tempo_ok(beats: np.ndarray, bpm_range, max_cv: float = 0.05) -> tuple[bool, float]:
    """Reject tracks outside the family tempo range or with unstable tempo."""
    if len(beats) < 16:
        return False, 0.0
    ib = np.diff(beats)
    bpm = 60.0 / float(np.median(ib))
    cv = float(np.std(ib) / np.mean(ib))
    lo, hi = bpm_range
    # allow half/double-time tracker errors to be judged by the folded tempo
    folded = next((b for b in (bpm, bpm * 2, bpm / 2) if lo <= b <= hi), None)
    return folded is not None and cv <= max_cv, bpm
