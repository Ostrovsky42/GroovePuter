"""Aggregate bars into a VariantProfile: the compact "grammar" of a variant.

Every source (track or take) gets equal total weight, so one long track or
one prolific artist cannot dominate the statistics.
"""
from __future__ import annotations

import json
from collections import defaultdict
from dataclasses import asdict, dataclass, field
from pathlib import Path

import numpy as np

from .features import INTERVAL_BUCKETS, bar_features, hamming, mask_of, swing_pct
from .grid import align_source, rotate_bar
from .schema import ROLES, STEPS, Bar


@dataclass
class VariantProfile:
    variant: str
    n_bars: int
    n_sources: int
    step_prob: dict[str, list[float]]            # role -> P(onset at step)
    density: dict[str, list[float]]              # role -> [mean, std] hits per bar
    sync: dict[str, list[float]]                 # role -> [mean, std]
    swing: list[float | None]                    # [median %, iqr %, n offsets]
    cells: dict[str, list[list[float]]]          # role -> [[mask, freq], ...]
    cell_coverage: dict[str, float]              # share of bars covered by kept cells
    protected: dict[str, list[int]]              # role -> steps with P >= threshold
    bass: dict[str, float | list[float] | None] = field(default_factory=dict)
    rotations: dict[str, int] = field(default_factory=dict)  # src -> applied rotation

    def to_json(self) -> dict:
        return asdict(self)

    @classmethod
    def from_json(cls, d: dict) -> "VariantProfile":
        return cls(**d)


def _wmean_std(values, weights):
    v = np.asarray(values, float)
    w = np.asarray(weights, float)
    if len(v) == 0 or w.sum() == 0:
        return [0.0, 0.0]
    m = float(np.average(v, weights=w))
    s = float(np.sqrt(np.average((v - m) ** 2, weights=w)))
    return [round(m, 4), round(s, 4)]


def merge_cells(mask_weights: dict[int, float], k: int, radius: int):
    """Greedy medoid merge: frequent masks absorb near-duplicates (transcription noise)."""
    total = sum(mask_weights.values()) or 1.0
    kept: list[list[float]] = []
    for m, w in sorted(mask_weights.items(), key=lambda kv: (-kv[1], kv[0])):
        for cell in kept:
            if hamming(int(cell[0]), m) <= radius:
                cell[1] += w
                break
        else:
            kept.append([m, w])
    kept.sort(key=lambda c: -c[1])
    top = kept[:k]
    cov = sum(c[1] for c in top) / total
    norm = sum(c[1] for c in top) or 1.0
    return [[int(m), round(w / norm, 4)] for m, w in top], round(cov, 4)


def wilson_low(p: float, n: int, z: float = 1.645) -> float:
    """One-sided 95% Wilson lower bound. 'Protected' needs evidence, not a lucky sample."""
    if n <= 0:
        return 0.0
    d = 1 + z * z / n
    c = p + z * z / (2 * n)
    return (c - z * np.sqrt(p * (1 - p) / n + z * z / (4 * n * n))) / d


HALF_BAR_W = {"kick": 1.0, "snare": 1.0, "hat": 0.5, "ohat": 0.5, "bass": 1.5}


def _masks(b: Bar) -> dict[str, int]:
    return {r: mask_of(b.role(r)) for r in ROLES}


def source_hits(sb: list[Bar]) -> np.ndarray:
    """Mean hit vector of one source: [roles, 16]."""
    H = np.zeros((len(ROLES), STEPS))
    for b in sb:
        m = _masks(b)
        for i, r in enumerate(ROLES):
            H[i] += np.array([m[r] >> s & 1 for s in range(STEPS)])
    return H / max(1, len(sb))


def source_step_prob(by_src: dict[str, list[Bar]], exclude: str | None = None) -> dict[str, np.ndarray]:
    """Step probabilities with equal weight per source (optionally leave one out)."""
    hs = [source_hits(sb) for s, sb in by_src.items() if s != exclude]
    P = np.mean(hs, axis=0) if hs else np.zeros((len(ROLES), STEPS))
    return {r: P[i] for i, r in enumerate(ROLES)}


_WVEC = np.array([HALF_BAR_W[r] for r in ROLES])


def _asym(H: np.ndarray) -> float:
    p = H.mean(0)
    return float((np.abs(p - np.roll(p, 8, axis=-1)) * _WVEC[:, None]).sum() / 2)


def has_half_bar_structure(by_src: dict[str, list[Bar]], n_perm: int = 200,
                           alpha: float = 0.01, seed: int = 0) -> bool:
    """Permutation test: is the variant's content different between bar halves beyond
    what random half-bar flips of its sources produce? If not, half-bar alignment is
    both impossible and harmless (the content is symmetric), so we skip it.
    Without this gate, refinement turned sampling noise into fake structure."""
    H = np.stack([source_hits(sb) for sb in by_src.values()])
    obs = _asym(H)
    rng = np.random.default_rng(seed)
    rolled = np.roll(H, 8, axis=-1)
    null = [_asym(np.where(rng.random(len(H))[:, None, None] < .5, rolled, H)) for _ in range(n_perm)]
    return obs > float(np.quantile(null, 1 - alpha))


def masks_loglik(bars: list[Bar], p: dict[str, np.ndarray], eps: float = 0.03) -> float:
    ll = 0.0
    for b in bars:
        m = _masks(b)
        for r in ROLES:
            q = np.clip(p[r], eps, 1 - eps)
            hit = np.array([m[r] >> s & 1 for s in range(STEPS)], bool)
            ll += HALF_BAR_W[r] * float(np.where(hit, np.log(q), np.log(1 - q)).sum())
    return ll


def refine_half_bar(sb: list[Bar], p: dict[str, np.ndarray],
                    margin_per_bar: float = 2.0) -> tuple[int, list[Bar]]:
    """Anchors cannot tell a half-bar (8-step) downbeat error under four-on-the-floor
    plus backbeat. Pick 0 or 8 by which explains the source better under profile p.

    The margin is per bar and deliberately large: with a near-uniform role (acid bass)
    a small margin lets every source flip toward the current estimate and the
    procedure invents structure from noise (seen in holdout: spurious 'protected'
    bass step). A real half-bar error on structured bass gains ~7 per bar."""
    rot = [rotate_bar(b, 8) for b in sb]
    if masks_loglik(rot, p) > masks_loglik(sb, p) + margin_per_bar * len(sb):
        return 8, rot
    return 0, sb


def build_profile(bars: list[Bar], variant: str, *, anchors: dict | None = None,
                  cell_k: int = 8, merge_radius: int = 1,
                  protected_threshold: float = 0.9, protected_min_low: float = 0.8,
                  half_bar_passes: int = 2) -> VariantProfile:
    by_src: dict[str, list[Bar]] = defaultdict(list)
    for b in bars:
        by_src[b.src].append(b)
    if not by_src:
        raise ValueError(f"no bars for variant {variant}")

    rotations = {}
    if anchors:
        for src in list(by_src):
            r, by_src[src] = align_source(by_src[src], anchors)
            if r:
                rotations[src] = r
        # half-bar refinement, only for variants with real half-bar structure;
        # each source is judged against the profile of the OTHER sources
        passes = half_bar_passes if len(by_src) >= 5 and has_half_bar_structure(by_src) else 0
        for _ in range(passes):
            changed = 0
            for src in list(by_src):
                p = source_step_prob(by_src, exclude=src)
                r, by_src[src] = refine_half_bar(by_src[src], p)
                if r:
                    rotations[src] = (rotations.get(src, 0) + r) % STEPS
                    changed += 1
            if not changed:
                break
        rotations = {k: v for k, v in rotations.items() if v}

    feats, weights = [], []
    for src, sb in by_src.items():
        w = 1.0 / len(sb)
        for b in sb:
            feats.append(bar_features(b))
            weights.append(w)
    W = np.asarray(weights)
    wsum = W.sum()

    step_prob, density, sync, cells, cov, protected = {}, {}, {}, {}, {}, {}
    for r in ROLES:
        hits = np.array([[f.masks[r] >> s & 1 for s in range(STEPS)] for f in feats], float)
        p = (hits * W[:, None]).sum(0) / wsum
        step_prob[r] = [round(float(x), 4) for x in p]
        density[r] = _wmean_std([f.density[r] for f in feats], W)
        sync[r] = _wmean_std([f.sync[r] for f in feats], W)
        mw: dict[int, float] = defaultdict(float)
        for f, w in zip(feats, W):
            if f.masks[r]:
                mw[f.masks[r]] += w
        cells[r], cov[r] = merge_cells(mw, cell_k, merge_radius) if mw else ([], 0.0)
        n_src = len(by_src)
        protected[r] = [s for s in range(STEPS) if p[s] >= protected_threshold
                        and wilson_low(float(p[s]), n_src) >= protected_min_low]

    offsets = [o for f in feats for o in f.swing_offsets]
    sw = swing_pct(offsets, [o for f in feats for o in f.even_offsets])
    if offsets:
        q75, q25 = np.percentile(offsets, [75, 25])
        swing = [round(sw, 2), round(50.0 * float(q75 - q25), 2), len(offsets)]
    else:
        swing = [None, None, 0]

    bass: dict = {}
    bf = [(f, w) for f, w in zip(feats, W) if f.bass_rest is not None]
    if bf:
        fw = [w for _, w in bf]
        bass["rest"] = _wmean_std([f.bass_rest for f, _ in bf], fw)[0]
        bass["dur"] = _wmean_std([f.bass_dur for f, _ in bf], fw)[0]
        ko = [(f.bass_kick_overlap, w) for f, w in bf if f.bass_kick_overlap is not None]
        bass["kick_overlap"] = _wmean_std([a for a, _ in ko], [w for _, w in ko])[0] if ko else None
        pitches = [p for f, _ in bf for p in f.bass_pitches]
        bass["median_pitch"] = float(np.median(pitches)) if pitches else None
        counts = {k: 0.0 for k in INTERVAL_BUCKETS}
        for f, w in bf:
            for iv in f.bass_intervals:
                counts[iv] += w
        tot = sum(counts.values()) or 1.0
        bass["intervals"] = [round(counts[k] / tot, 4) for k in INTERVAL_BUCKETS]

    return VariantProfile(variant=variant, n_bars=len(feats), n_sources=len(by_src),
                          step_prob=step_prob, density=density, sync=sync, swing=swing,
                          cells=cells, cell_coverage=cov, protected=protected,
                          bass=bass, rotations=rotations)


def build_profiles(bars: list[Bar], cfg: dict) -> dict[str, VariantProfile]:
    pc = cfg.get("profile", {})
    groups: dict[str, list[Bar]] = defaultdict(list)
    for b in bars:
        groups[b.variant].append(b)
    return {v: build_profile(bs, v, anchors=cfg.get("anchors"),
                             cell_k=pc.get("cell_k", 8),
                             merge_radius=pc.get("merge_radius", 1),
                             protected_threshold=pc.get("protected_threshold", 0.9),
                             protected_min_low=pc.get("protected_min_low", 0.8))
            for v, bs in sorted(groups.items())}


def save_profiles(path, profiles: dict[str, VariantProfile], meta: dict | None = None):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    data = {"meta": meta or {}, "variants": {k: p.to_json() for k, p in profiles.items()}}
    Path(path).write_text(json.dumps(data, indent=1), encoding="utf-8")


def load_profiles(path) -> tuple[dict[str, VariantProfile], dict]:
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    return ({k: VariantProfile.from_json(v) for k, v in data["variants"].items()},
            data.get("meta", {}))
