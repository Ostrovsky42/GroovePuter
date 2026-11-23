"""Beat grid -> 16-step bars, plus downbeat-rotation alignment.

Audio side: beats/downbeats come from a beat tracker (seconds). We keep only
4/4 bars (exactly 4 beats between consecutive downbeats) and map each event
time to a fractional step with piecewise-linear interpolation between beats,
so tempo drift inside a bar does not smear the grid.
"""
from __future__ import annotations

import numpy as np

from .schema import STEPS, Bar

HALF = 0.5  # half a step: early hits up to this much before step 0 belong here


def bars_from_beats(beats, downbeats, beats_per_bar: int = 4) -> list[np.ndarray]:
    """Return one array of 5 beat times [b0..b4] per valid 4/4 bar."""
    beats = np.asarray(beats, float)
    downbeats = np.asarray(downbeats, float)
    out = []
    for d0, d1 in zip(downbeats[:-1], downbeats[1:]):
        inside = beats[(beats >= d0 - 1e-3) & (beats < d1 - 1e-3)]
        if len(inside) != beats_per_bar:
            continue
        out.append(np.append(inside, d1))
    return out


def time_to_pos(t: float, bt: np.ndarray) -> float | None:
    """Map time to fractional step in the bar defined by beat times bt[0..4]."""
    first = (bt[1] - bt[0]) / 4.0
    last = (bt[4] - bt[3]) / 4.0
    if t < bt[0] - HALF * first or t >= bt[4] - HALF * last:
        return None
    if t < bt[0]:
        return (t - bt[0]) / first          # small negative -> step 0
    k = int(np.searchsorted(bt, t, side="right") - 1)
    k = min(max(k, 0), 3)
    return 4.0 * k + 4.0 * (t - bt[k]) / (bt[k + 1] - bt[k])


def quantize(pos: float) -> int | None:
    q = int(np.floor(pos + 0.5))
    return q if 0 <= q < STEPS else None


def events_to_bars(events: dict[str, list[tuple]], beats, downbeats, *,
                   src: str, variant: str) -> list[Bar]:
    """events: role -> [(time_s, vel)] for drums, [(time_s, vel, pitch, dur_s)] for bass."""
    bars = []
    for i, bt in enumerate(bars_from_beats(beats, downbeats)):
        bpm = 60.0 * 4 / (bt[4] - bt[0])
        step_s = (bt[4] - bt[0]) / STEPS
        ev: dict[str, list[list[float]]] = {}
        for role, items in events.items():
            out = []
            for it in items:
                p = time_to_pos(it[0], bt)
                if p is None:
                    continue
                row = [round(p, 4), round(float(it[1]), 3)]
                if role == "bass":
                    row += [float(it[2]), round(float(it[3]) / step_s, 3)]
                out.append(row)
            if out:
                ev[role] = out
        bars.append(Bar(src=src, variant=variant, events=ev, bar_idx=i, bpm=bpm))
    return bars


def rotate_bar(bar: Bar, r: int) -> Bar:
    """Shift so that observed step r becomes step 0 (wraps inside the bar)."""
    if r % STEPS == 0:
        return bar
    ev = {}
    for role, items in bar.events.items():
        rot = []
        for e in items:
            p = (e[0] - r) % STEPS
            if p >= STEPS - HALF:     # keep early hits on step 0 after wrap
                p -= STEPS
            rot.append([round(p, 4)] + list(e[1:]))
        ev[role] = rot
    return Bar(src=bar.src, variant=bar.variant, events=ev, bar_idx=bar.bar_idx,
               bpm=bar.bpm, take=bar.take)


def best_rotation(bars: list[Bar], anchors: dict[str, list[int]],
                  candidates=(0, 4, 8, 12)) -> int:
    """Pick the beat-level rotation that best matches the family's anchor steps.

    Downbeat trackers mostly err by whole beats, so we only test rotations by 4.
    Ties resolve to 0 (trust the tracker).
    """
    hist = {role: np.zeros(STEPS) for role in anchors}
    for b in bars:
        for role in anchors:
            for e in b.role(role):
                q = quantize(e[0])
                if q is not None:
                    hist[role][q] += 1
    best, best_score = 0, -1.0
    for r in candidates:
        score = 0.0
        for role, steps in anchors.items():
            h = hist[role]
            tot = h.sum()
            if tot == 0:
                continue
            score += sum(h[(s + r) % STEPS] for s in steps) / tot
        if score > best_score + 1e-9:
            best, best_score = r, score
    return best


def align_source(bars: list[Bar], anchors: dict[str, list[int]]) -> tuple[int, list[Bar]]:
    r = best_rotation(bars, anchors)
    return r, [rotate_bar(b, r) for b in bars]
