"""Distances in ONE metric space for corpus bars, profiles and generated takes."""
from __future__ import annotations

import math

import numpy as np

from .features import BarFeatures, hamming
from .profile import VariantProfile
from .schema import ROLES, STEPS

DEFAULT_W = {"kick": 1.0, "snare": 1.0, "hat": 0.5, "ohat": 0.5, "bass": 1.5}


def bar_hits_distance(a: BarFeatures, b: BarFeatures, w: dict = DEFAULT_W) -> float:
    """Weighted count of differing hits. 2.0 ~ "two meaningful hits differ"."""
    return sum(w.get(r, 0) * hamming(a.masks[r], b.masks[r]) for r in ROLES)


def phrase_distance(pa: list[BarFeatures], pb: list[BarFeatures], w: dict = DEFAULT_W) -> float:
    n = min(len(pa), len(pb))
    if n == 0:
        return float("inf")
    return sum(bar_hits_distance(pa[i], pb[i], w) for i in range(n)) / n


def profile_distance(a: VariantProfile, b: VariantProfile, w: dict = DEFAULT_W) -> float:
    """0 = identical grammar. Step-probability L1 dominates; swing and density refine."""
    wsum = sum(w.get(r, 0) for r in ROLES)
    steps = sum(w.get(r, 0) * float(np.mean(np.abs(np.array(a.step_prob[r]) -
                                                     np.array(b.step_prob[r]))))
                for r in ROLES) / wsum
    dens = float(np.mean([min(1.0, abs(a.density[r][0] - b.density[r][0]) / 4.0) for r in ROLES]))
    sw = 0.0
    if a.swing[0] is not None and b.swing[0] is not None:
        sw = min(1.0, abs(a.swing[0] - b.swing[0]) / 10.0)
    return round(steps + 0.25 * dens + 0.25 * sw, 5)


def bar_loglik(f: BarFeatures, prof: VariantProfile, w: dict = DEFAULT_W, eps: float = 0.03) -> float:
    """Independent-Bernoulli log-likelihood of a bar's onset masks under a profile."""
    ll = 0.0
    for r in ROLES:
        wr = w.get(r, 0)
        if not wr:
            continue
        m = f.masks[r]
        for s in range(STEPS):
            p = min(1 - eps, max(eps, prof.step_prob[r][s]))
            ll += wr * (math.log(p) if m >> s & 1 else math.log(1 - p))
    return ll
