"""Blind listening tests that need no musical training.

ABX  — "is X the same variant as A or as B?"  Tests: are variants audibly different?
PREF — "which of 1/2 do you like more?" new generator vs baseline build, same variant.

Items are any files named  <variant>__<anything>.<ext>  (wav, mp3, mid, scene...).
plan  -> anonymized trial folders + answers.csv template + _key/key.json
score -> exact binomial test per pair.
"""
from __future__ import annotations

import csv
import json
import math
import random
import shutil
from collections import defaultdict
from itertools import combinations
from pathlib import Path


def _items(d) -> dict[str, list[Path]]:
    groups = defaultdict(list)
    for f in sorted(Path(d).iterdir()):
        if f.is_file() and "__" in f.name:
            groups[f.name.split("__", 1)[0]].append(f)
    return dict(groups)


def binom_p_ge(k: int, n: int, p: float = 0.5) -> float:
    """P(X >= k), X ~ Binomial(n, p). One-sided exact test."""
    return sum(math.comb(n, i) * p ** i * (1 - p) ** (n - i) for i in range(k, n + 1))


def min_correct(n: int, alpha: float = 0.05) -> int:
    for k in range(n + 1):
        if binom_p_ge(k, n) < alpha:
            return k
    return n + 1


def _copy(src: Path, dst_dir: Path, label: str) -> str:
    dst = dst_dir / f"{label}{src.suffix}"
    shutil.copy2(src, dst)
    return dst.name


def plan_abx(items_dir, out_dir, trials_per_pair: int = 16, seed: int = 1, pairs=None) -> dict:
    rng = random.Random(seed)
    groups = _items(items_dir)
    usable = {v: fs for v, fs in groups.items() if len(fs) >= 2}
    if len(usable) < 2:
        raise ValueError("need >= 2 variants with >= 2 items each")
    pairs = pairs or list(combinations(sorted(usable), 2))
    out = Path(out_dir)
    if out.exists():
        raise FileExistsError(f"{out} exists; refuse to overwrite a running test")
    (out / "_key").mkdir(parents=True)
    trials = []
    for va, vb in pairs:
        for _ in range(trials_per_pair):
            a, b = rng.choice(usable[va]), rng.choice(usable[vb])
            x_is = rng.choice("AB")
            pool = [f for f in usable[va if x_is == "A" else vb] if f not in (a, b)]
            x = rng.choice(pool or usable[va if x_is == "A" else vb])
            trials.append((va, vb, a, b, x, x_is))
    rng.shuffle(trials)
    key = {"mode": "abx", "trials": {}}
    for i, (va, vb, a, b, x, x_is) in enumerate(trials, 1):
        tid = f"trial_{i:03d}"
        td = out / tid
        td.mkdir()
        for lab, f in (("A", a), ("B", b), ("X", x)):
            _copy(f, td, lab)
        key["trials"][tid] = {"pair": [va, vb], "truth": x_is,
                              "files": {"A": a.name, "B": b.name, "X": x.name}}
    _write_answers(out, list(key["trials"]), "A or B")
    (out / "_key" / "key.json").write_text(json.dumps(key, indent=1), encoding="utf-8")
    return {"trials": len(trials), "pairs": pairs,
            "min_correct_per_pair": min_correct(trials_per_pair)}


def plan_pref(new_dir, base_dir, out_dir, trials_per_variant: int = 12, seed: int = 1) -> dict:
    rng = random.Random(seed)
    new, base = _items(new_dir), _items(base_dir)
    variants = sorted(set(new) & set(base))
    if not variants:
        raise ValueError("no common variants between new and baseline items")
    out = Path(out_dir)
    if out.exists():
        raise FileExistsError(f"{out} exists; refuse to overwrite a running test")
    (out / "_key").mkdir(parents=True)
    trials = [(v, rng.choice(new[v]), rng.choice(base[v]), rng.random() < 0.5)
              for v in variants for _ in range(trials_per_variant)]
    rng.shuffle(trials)
    key = {"mode": "pref", "trials": {}}
    for i, (v, n, b, new_first) in enumerate(trials, 1):
        tid = f"trial_{i:03d}"
        td = out / tid
        td.mkdir()
        first, second = (n, b) if new_first else (b, n)
        _copy(first, td, "1")
        _copy(second, td, "2")
        key["trials"][tid] = {"variant": v, "new_is": "1" if new_first else "2",
                              "files": {"1": first.name, "2": second.name}}
    _write_answers(out, list(key["trials"]), "1 or 2")
    (out / "_key" / "key.json").write_text(json.dumps(key, indent=1), encoding="utf-8")
    return {"trials": len(trials), "variants": variants}


def _write_answers(out: Path, tids, hint):
    with open(out / "answers.csv", "w", newline="", encoding="utf-8") as fh:
        w = csv.writer(fh)
        w.writerow(["trial", f"answer ({hint})", "note"])
        for t in tids:
            w.writerow([t, "", ""])


def score(plan_dir, answers_csv=None, alpha: float = 0.05) -> dict:
    plan = Path(plan_dir)
    key = json.loads((plan / "_key" / "key.json").read_text(encoding="utf-8"))
    answers = {}
    with open(answers_csv or plan / "answers.csv", encoding="utf-8") as fh:
        rows = csv.reader(fh)
        next(rows, None)
        for row in rows:
            if row and len(row) > 1 and row[1].strip():
                answers[row[0].strip()] = row[1].strip().upper()
    stats = defaultdict(lambda: [0, 0])  # group -> [hits, n]
    for tid, t in key["trials"].items():
        if tid not in answers:
            continue
        if key["mode"] == "abx":
            g = " vs ".join(t["pair"])
            hit = answers[tid] == t["truth"]
        else:
            g = t["variant"]
            hit = answers[tid] == t["new_is"]
        stats[g][0] += hit
        stats[g][1] += 1
    res = {"mode": key["mode"], "answered": len(answers), "total": len(key["trials"]), "groups": {}}
    for g, (k, n) in sorted(stats.items()):
        p = binom_p_ge(k, n)
        res["groups"][g] = {"hits": k, "n": n, "p_value": round(p, 4), "significant": p < alpha}
    return res
