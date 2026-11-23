"""Per-bar musical features. Pure functions of a Bar; identical for corpus and takes."""
from __future__ import annotations

from dataclasses import dataclass, field

import numpy as np

from .grid import quantize
from .schema import ROLES, STEPS, Bar

# Metrical weights for a 16-step 4/4 bar (higher = stronger position).
METRIC_W = np.array([5, 1, 2, 1, 3, 1, 2, 1, 4, 1, 2, 1, 3, 1, 2, 1], float)
INTERVAL_BUCKETS = ("rep", "step", "third", "fifth", "octave", "other")


def popcount(m: int) -> int:
    return bin(m).count("1")


def hamming(a: int, b: int) -> int:
    return popcount(a ^ b)


def mask_of(events: list[list[float]]) -> int:
    m = 0
    for e in events:
        q = quantize(e[0])
        if q is not None:
            m |= 1 << q
    return m


def mask_to_steps(m: int) -> list[int]:
    return [s for s in range(STEPS) if m >> s & 1]


def syncopation(mask: int) -> float:
    """Simplified Longuet-Higgins & Lee: onset on weak step followed by a rest
    on the next stronger step contributes the weight difference."""
    total = 0.0
    for i in mask_to_steps(mask):
        for k in range(1, STEPS):
            j = (i + k) % STEPS
            if METRIC_W[j] > METRIC_W[i]:
                if not mask >> j & 1:
                    total += METRIC_W[j] - METRIC_W[i]
                break
    return total


def interval_bucket(d: int) -> str:
    d = abs(d)
    if d == 0:
        return "rep"
    if d <= 2:
        return "step"
    if d <= 4:
        return "third"
    if d in (5, 7):
        return "fifth"
    if d % 12 == 0:
        return "octave"
    return "other"


@dataclass
class BarFeatures:
    masks: dict[str, int]
    density: dict[str, int]
    sync: dict[str, float]
    swing_offsets: list[float] = field(default_factory=list)   # in steps, odd 16ths only
    even_offsets: list[float] = field(default_factory=list)    # in steps, even 16ths (latency ref)
    bass_rest: float | None = None        # fraction of steps with no sounding bass
    bass_dur: float | None = None         # mean note length, steps
    bass_intervals: list[str] = field(default_factory=list)
    bass_pitches: list[float] = field(default_factory=list)
    bass_kick_overlap: float | None = None  # share of bass onsets on a kick step


def bar_features(bar: Bar) -> BarFeatures:
    masks = {r: mask_of(bar.role(r)) for r in ROLES}
    density = {r: popcount(m) for r, m in masks.items()}
    sync = {r: syncopation(m) for r, m in masks.items()}

    odd, even = [], []
    for r in ("hat", "ohat", "snare", "kick"):
        for e in bar.role(r):
            q = quantize(e[0])
            if q is not None:
                (odd if q % 2 else even).append(e[0] - q)

    f = BarFeatures(masks=masks, density=density, sync=sync,
                    swing_offsets=odd, even_offsets=even)
    bass = sorted(bar.role("bass"), key=lambda e: e[0])
    if bass:
        covered = np.zeros(STEPS, bool)
        for e in bass:
            q = quantize(e[0])
            if q is None:
                continue
            dur = max(1, int(round(e[3]))) if len(e) > 3 else 1
            covered[q:min(STEPS, q + dur)] = True
        f.bass_rest = 1.0 - covered.mean()
        f.bass_dur = float(np.mean([e[3] for e in bass if len(e) > 3] or [1.0]))
        pitches = [e[2] for e in bass if len(e) > 2]
        f.bass_pitches = pitches
        f.bass_intervals = [interval_bucket(int(round(b - a)))
                            for a, b in zip(pitches[:-1], pitches[1:])]
        kick = masks["kick"]
        onsets = mask_to_steps(masks["bass"])
        if onsets:
            f.bass_kick_overlap = sum(1 for s in onsets if kick >> s & 1) / len(onsets)
    return f


def swing_pct(offsets: list[float], even: list[float] | None = None) -> float | None:
    """Offbeat 16th delayed by d steps -> swing % = 50 * (1 + d). 50 straight, 66 triplet.

    d is measured RELATIVE to on-beat hits (median odd offset - median even offset),
    so constant latency of a transcriber or beat tracker cancels out."""
    if not offsets:
        return None
    ref = float(np.median(even)) if even else 0.0
    return 50.0 * (1.0 + float(np.median(offsets)) - ref)
