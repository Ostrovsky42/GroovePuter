"""Synthetic corpus + synthetic takes with KNOWN ground truth.

Used by tests and `gplab demo` to prove the pipeline recovers grammars and that
the validator catches the failure mode from the 2026-11-10 genre audit
("rerolls change pitches but keep the same rhythm mask").

These grammars are illustrative test fixtures, NOT musical claims about House.
"""
from __future__ import annotations

import random

from .schema import STEPS, Bar

GRAMMARS = {
    "house.base": {
        "swing": 54,
        "kick": {0: 1.0, 4: 1.0, 8: 1.0, 12: 1.0},
        "snare": {4: .92, 12: .92},
        "hat": {2: .92, 6: .92, 10: .92, 14: .92, 7: .2, 15: .25},
        "ohat": {6: .15, 14: .15},
        "bass": {2: .65, 6: .55, 10: .65, 14: .55, 3: .2, 11: .2},
        "bass_dur": (1, 2), "bass_iv": [0, 0, 7, 12, -5],
    },
    "house.deep": {
        "swing": 58,
        "kick": {0: 1.0, 4: 1.0, 8: 1.0, 12: 1.0},
        "snare": {4: .8, 12: .85},
        "hat": {2: .85, 6: .85, 10: .85, 14: .85},
        "ohat": {2: .35, 10: .35},
        "bass": {0: .15, 3: .6, 6: .45, 7: .2, 10: .6, 13: .35},
        "bass_dur": (2, 4), "bass_iv": [0, 3, 5, 7, -2],
    },
    "house.acid": {
        "swing": 51,
        "kick": {0: 1.0, 4: 1.0, 8: 1.0, 12: 1.0},
        "snare": {4: .7, 12: .7},
        "hat": {s: (.85 if s % 2 == 0 else .55) for s in range(STEPS)},
        "ohat": {},
        "bass": {s: .75 for s in range(STEPS)},
        "bass_dur": (1, 1), "bass_iv": [0, 1, 3, 12, -12, 0, 2],
    },
}


def _sample_masks(g: dict, rng: random.Random) -> dict[str, list[int]]:
    return {r: [s for s, p in g[r].items() if rng.random() < p]
            for r in ("kick", "snare", "hat", "ohat", "bass")}


def _bar_from_steps(steps: dict[str, list[int]], g, rng, *, src, variant, idx,
                    root: int, jitter: float, take=None) -> Bar:
    d = (g["swing"] / 50.0) - 1.0       # offbeat delay in steps
    ev: dict[str, list[list[float]]] = {}
    pitch = root
    for r, ss in steps.items():
        rows = []
        for s in sorted(ss):
            pos = s + (d if s % 2 else 0.0) + rng.gauss(0, jitter)
            vel = round(min(1.0, max(0.1, rng.gauss(0.8, 0.1))), 3)
            if r == "bass":
                pitch = root + rng.choice(g["bass_iv"])
                rows.append([round(pos, 4), vel, float(pitch), float(rng.randint(*g["bass_dur"]))])
            else:
                rows.append([round(pos, 4), vel])
        if rows:
            ev[r] = rows
    return Bar(src=src, variant=variant, events=ev, bar_idx=idx, bpm=124.0, take=take)


def synth_corpus(n_sources: int = 30, bars_per_source: int = 12, seed: int = 7,
                 drop: float = .05, spurious: float = .02, rotate_share: float = .2,
                 jitter: float = .05) -> list[Bar]:
    """Loop-based 'tracks': one core loop per track, light per-bar mutation,
    transcription noise, and some sources with a wrong downbeat (rotated)."""
    from .grid import rotate_bar
    rng = random.Random(seed)
    bars = []
    for v, g in GRAMMARS.items():
        for k in range(n_sources):
            core = _sample_masks(g, rng)
            root = rng.randint(33, 40)
            rot = rng.choice((4, 8, 12)) if rng.random() < rotate_share else 0
            for i in range(bars_per_source):
                steps = {r: list(s) for r, s in core.items()}
                if rng.random() < .15:                      # bar variation
                    fresh = _sample_masks(g, rng)
                    r = rng.choice(list(steps))
                    steps[r] = fresh[r]
                for r in steps:                             # transcription noise
                    steps[r] = [s for s in steps[r] if rng.random() > drop]
                    for s in range(STEPS):
                        if rng.random() < spurious / 4 and s not in steps[r]:
                            steps[r].append(s)
                b = _bar_from_steps(steps, g, rng, src=f"synth:{v}:{k}", variant=v,
                                    idx=i, root=root, jitter=jitter)
                bars.append(rotate_bar(b, -rot) if rot else b)
    return bars


def synth_takes(mode: str = "good", takes: int = 8, phrase_bars: int = 4,
                seed: int = 11, variants=None) -> list[Bar]:
    """mode: good  - each take samples a fresh loop from the variant grammar
             stuck - every take reuses one rhythm; only pitches change (audit bug)
             wrong - every variant is generated from house.acid's grammar"""
    rng = random.Random(seed)
    out = []
    for v in variants or GRAMMARS:
        g_src = GRAMMARS["house.acid"] if mode == "wrong" else GRAMMARS[v]
        fixed = _sample_masks(g_src, rng)
        for t in range(takes):
            core = fixed if mode == "stuck" else _sample_masks(g_src, rng)
            root = rng.randint(33, 40)
            for i in range(phrase_bars):
                steps = {r: list(s) for r, s in core.items()}
                if mode != "stuck" and i == phrase_bars - 1 and rng.random() < .5:
                    steps["bass"] = _sample_masks(g_src, rng)["bass"]   # phrase ending
                out.append(_bar_from_steps(steps, g_src, rng, src=f"take:{v}:{t}", variant=v,
                                           idx=i, root=root, jitter=0.0, take=t))
    return out
