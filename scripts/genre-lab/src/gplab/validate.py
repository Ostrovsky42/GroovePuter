"""Validate generated takes against corpus profiles.

Five checks per variant (thresholds in config `validate:`):
  fidelity     takes' own profile is closest to its corpus variant, with margin
  accuracy     per-take nearest-variant classification (log-likelihood) is right
  diversity    enough structurally distinct takes (greedy, distance >= tau)
  identity     takes of one variant are closer to each other than to siblings
  protected    corpus "always" steps (P >= 0.9) are kept by takes
"""
from __future__ import annotations

import json
from collections import defaultdict
from pathlib import Path

import numpy as np

from .distance import DEFAULT_W, bar_loglik, phrase_distance, profile_distance
from .features import bar_features
from .profile import VariantProfile, build_profile
from .schema import ROLES, Bar

DEFAULTS = {
    "tau_distinct_hits": 2.0,
    "min_distinct": 4,
    "takes_expected": 8,
    "separation_ratio": 0.8,
    "min_take_accuracy": 0.75,
    "identity_ratio": 0.7,
    "protected_coverage": 0.75,
}


def _group_takes(bars: list[Bar]):
    takes: dict[str, dict[int, list[Bar]]] = defaultdict(lambda: defaultdict(list))
    for b in bars:
        if b.take is None:
            raise ValueError(f"take bars must have 'take' set: {b.src}")
        takes[b.variant][b.take].append(b)
    for v in takes:
        for t in takes[v]:
            takes[v][t].sort(key=lambda b: b.bar_idx)
    return takes


def distinct_count(phrases: list[list], tau: float, w) -> int:
    reps: list[list] = []
    for p in phrases:
        if all(phrase_distance(p, r, w) >= tau for r in reps):
            reps.append(p)
    return len(reps)


def validate(take_bars: list[Bar], corpus: dict[str, VariantProfile], cfg: dict) -> dict:
    vc = {**DEFAULTS, **cfg.get("validate", {})}
    w = {**DEFAULT_W, **vc.get("role_weights", {})}
    takes = _group_takes(take_bars)
    feats = {v: {t: [bar_features(b) for b in bs] for t, bs in ts.items()}
             for v, ts in takes.items()}
    report = {"thresholds": vc, "variants": {}}

    for v, ts in sorted(takes.items()):
        res: dict = {"takes": len(ts), "checks": {}}
        if v not in corpus:
            res["error"] = f"no corpus profile for {v}"
            report["variants"][v] = res
            continue
        chk = res["checks"]

        # 1. fidelity / separation
        tp = build_profile([b for bs in ts.values() for b in bs], v)
        d = {cv: profile_distance(tp, cp, w) for cv, cp in corpus.items()}
        ranked = sorted(d.items(), key=lambda kv: kv[1])
        nearest = ranked[0][0]
        second = next((x for x in ranked if x[0] != v), None)
        ratio = d[v] / second[1] if second and second[1] > 0 else 0.0
        chk["fidelity"] = {"pass": nearest == v and ratio <= vc["separation_ratio"],
                           "d_self": d[v], "nearest": nearest,
                           "ratio_to_runner_up": round(ratio, 3),
                           "runner_up": second[0] if second else None}

        # 2. per-take classification
        correct, preds = 0, {}
        for t, fs in feats[v].items():
            ll = {cv: sum(bar_loglik(f, cp, w) for f in fs) for cv, cp in corpus.items()}
            pred = max(ll, key=ll.get)
            preds[t] = pred
            correct += pred == v
        acc = correct / len(ts)
        chk["accuracy"] = {"pass": acc >= vc["min_take_accuracy"], "value": round(acc, 3),
                           "misclassified": {t: p for t, p in preds.items() if p != v}}

        # 3. diversity
        phrases = [feats[v][t] for t in sorted(feats[v])]
        dc = distinct_count(phrases, vc["tau_distinct_hits"], w)
        pair = [phrase_distance(a, b, w) for i, a in enumerate(phrases) for b in phrases[i + 1:]]
        need = vc["min_distinct"] * len(ts) / vc["takes_expected"]
        chk["diversity"] = {"pass": dc >= need, "distinct": dc, "of": len(ts),
                            "need": round(need, 2),
                            "mean_pair_hits": round(float(np.mean(pair)), 3) if pair else 0.0}

        # 4. identity vs sibling takes (only if siblings were generated too)
        others = [p for ov in feats if ov != v for p in feats[ov].values()]
        if others and pair:
            inter = float(np.mean([phrase_distance(a, b, w) for a in phrases for b in others]))
            intra = float(np.mean(pair))
            r = intra / inter if inter > 0 else 1.0
            chk["identity"] = {"pass": r <= vc["identity_ratio"], "intra": round(intra, 3),
                               "inter": round(inter, 3), "ratio": round(r, 3)}
        else:
            chk["identity"] = {"pass": None, "note": "needs takes of a sibling variant"}

        # 5. protected steps — counted per TAKE (a take keeps a step if it plays it
        #    in at least half its bars); per-bar counting over-penalises 8-take runs
        miss = {}
        for r in ROLES:
            for s in corpus[v].protected.get(r, []):
                kept = sum(1 for fs in feats[v].values()
                           if sum(f.masks[r] >> s & 1 for f in fs) * 2 >= len(fs))
                cov = kept / len(ts)
                if cov < vc["protected_coverage"]:
                    miss[f"{r}@{s}"] = round(cov, 3)
        chk["protected"] = {"pass": not miss, "missing": miss}

        res["pass"] = all(c["pass"] is not False for c in chk.values())
        report["variants"][v] = res

    report["pass"] = bool(report["variants"]) and all(
        r.get("pass", False) for r in report["variants"].values())
    return report


def render_markdown(report: dict) -> str:
    ok = lambda x: "✅" if x is True else ("❌" if x is False else "—")
    lines = [f"# Validation: {'PASS' if report['pass'] else 'FAIL'}", "",
             "| variant | fidelity | accuracy | diversity | identity | protected |",
             "|---|---|---|---|---|---|"]
    for v, r in report["variants"].items():
        if "error" in r:
            lines.append(f"| {v} | {r['error']} | | | | |")
            continue
        c = r["checks"]
        lines.append(
            f"| {v} | {ok(c['fidelity']['pass'])} {c['fidelity']['ratio_to_runner_up']} "
            f"| {ok(c['accuracy']['pass'])} {c['accuracy']['value']} "
            f"| {ok(c['diversity']['pass'])} {c['diversity']['distinct']}/{c['diversity']['of']} "
            f"| {ok(c['identity']['pass'])} {c['identity'].get('ratio', '')} "
            f"| {ok(c['protected']['pass'])} {len(c['protected']['missing'])} |")
    lines += ["", "fidelity: ratio d_self/d_runner_up (lower better); "
              "identity: intra/inter take distance (lower better)."]
    return "\n".join(lines) + "\n"


def write_report(report: dict, out_dir) -> None:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    (out / "validation.json").write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
    (out / "validation.md").write_text(render_markdown(report), encoding="utf-8")
