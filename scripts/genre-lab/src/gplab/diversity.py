"""Diversity of takes without corpus profiles.

distinct/N : greedy count of takes whose 4-bar phrases differ by at least tau hits
             (the same count validate() uses for its diversity check).
mean_pair_hits : mean phrase distance over all take pairs, in hits. With one role
             selected, the weight is 1 for that role and 0 for the others.
"""
from __future__ import annotations

import numpy as np

from .distance import phrase_distance
from .features import bar_features
from .schema import ROLES
from .validate import DEFAULTS, _group_takes, distinct_count


def _weights(roles: list[str]) -> dict[str, float]:
    unknown = [r for r in roles if r not in ROLES]
    if unknown:
        raise ValueError(f"unknown roles {unknown}; known: {list(ROLES)}")
    return {r: (1.0 if r in roles else 0.0) for r in ROLES}


def diversity(bars, roles: list[str] | None = None, tau: float | None = None) -> dict:
    """Per variant: takes N, distinct over `roles` (default: all roles), and
    mean_pair_hits per role. Input: Bar records of takes (take index required)."""
    roles = list(roles) if roles else list(ROLES)
    tau = DEFAULTS["tau_distinct_hits"] if tau is None else tau
    takes = _group_takes(list(bars))
    out = {}
    for variant, by_take in sorted(takes.items()):
        phrases = [[bar_features(b) for b in by_take[t]] for t in sorted(by_take)]
        combined = _weights(roles)
        entry = {
            "takes": len(phrases),
            "distinct": distinct_count(phrases, tau, combined),
            "mean_pair_hits": {},
        }
        for role in roles:
            w = _weights([role])
            pairs = [phrase_distance(a, b, w) for i, a in enumerate(phrases) for b in phrases[i + 1:]]
            entry["mean_pair_hits"][role] = round(float(np.mean(pairs)), 3) if pairs else 0.0
        out[variant] = entry
    return out


def render_markdown(result: dict, roles: list[str]) -> str:
    head = "| variant | distinct/N | " + " | ".join(f"mean_pair_hits {r}" for r in roles) + " |"
    lines = [head, "|---|---|" + "---|" * len(roles)]
    for variant, e in result.items():
        cells = " | ".join(f"{e['mean_pair_hits'][r]:.3f}" for r in roles)
        lines.append(f"| {variant} | {e['distinct']}/{e['takes']} | {cells} |")
    return "\n".join(lines) + "\n"
