"""Corpus selection without trusting any single source of labels.

A track enters variant V only if (label_policy=both):
  1. uploader genre tags on Jamendo contain one of V.jamendo_tags, AND
  2. the Discogs-400 classifier gives V.discogs_style >= min_style_prob, AND
  3. that style beats every sibling style of the family by >= min_margin.
Each track goes to at most one variant; per-artist cap prevents one producer
from defining a genre. Output: manifest.csv + selection_report.md (GO/NO-GO).
"""
from __future__ import annotations

import csv
import json
import random
from collections import Counter, defaultdict
from pathlib import Path


def load_jamendo(tsv_path) -> list[dict]:
    """MTG-Jamendo autotagging*.tsv: TRACK_ID ARTIST_ID ALBUM_ID PATH DURATION TAG..."""
    rows = []
    with open(tsv_path, encoding="utf-8") as fh:
        next(fh)  # header
        for line in fh:
            parts = line.rstrip("\n").split("\t")
            if len(parts) < 6:
                continue
            tags = {t.split("---", 1)[1] for t in parts[5:] if t.startswith("genre---")}
            rows.append({"track_id": parts[0], "artist_id": parts[1], "path": parts[3],
                         "duration": float(parts[4]), "tags": sorted(tags)})
    return rows


def tag_counts(rows) -> Counter:
    return Counter(t for r in rows for t in r["tags"])


def candidates(rows, cfg) -> list[dict]:
    want = {t for v in cfg["variants"].values() for t in v.get("jamendo_tags", [])}
    min_dur = cfg.get("corpus", {}).get("min_duration_s", 90)
    policy = cfg.get("corpus", {}).get("label_policy", "both")
    return [r for r in rows if r["duration"] >= min_dur and
            (policy == "classifier" or want & set(r["tags"]))]


def write_csv(path, rows, fields):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as fh:
        w = csv.DictWriter(fh, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            r = dict(r)
            if isinstance(r.get("tags"), list):
                r["tags"] = "|".join(r["tags"])
            w.writerow(r)


def read_csv(path) -> list[dict]:
    with open(path, encoding="utf-8") as fh:
        rows = list(csv.DictReader(fh))
    for r in rows:
        if "tags" in r:
            r["tags"] = [t for t in r["tags"].split("|") if t]
    return rows


def run_styles(cands, audio_root, models_dir, out_jsonl, cfg, limit=None) -> int:
    """Resumable: tracks already in out_jsonl are skipped."""
    from .adapters.style import StyleModel
    keep = {v["discogs_style"] for v in cfg["variants"].values()}
    out = Path(out_jsonl)
    done = set()
    if out.exists():
        done = {json.loads(l)["track_id"] for l in out.read_text(encoding="utf-8").splitlines() if l.strip()}
    model = StyleModel(models_dir)
    seg = cfg.get("segment", {})
    n = 0
    with open(out, "a", encoding="utf-8") as fh:
        for r in cands:
            if r["track_id"] in done:
                continue
            if limit is not None and n >= limit:
                break
            dur = float(r["duration"])
            start = dur * seg.get("start_frac", 0.2)
            probs = model.predict(Path(audio_root) / r["path"], start=start,
                                  duration=min(60.0, dur - start))
            top = dict(sorted(probs.items(), key=lambda kv: -kv[1])[:5])
            top.update({k: probs.get(k, 0.0) for k in keep})
            fh.write(json.dumps({"track_id": r["track_id"],
                                 "probs": {k: round(v, 4) for k, v in top.items()}}) + "\n")
            n += 1
    return n


def select(cands, styles_jsonl, cfg) -> tuple[list[dict], dict]:
    cc = cfg.get("corpus", {})
    policy = cc.get("label_policy", "both")
    min_p, margin = cc.get("min_style_prob", 0.25), cc.get("min_margin", 0.05)
    cap_artist, cap_total = cc.get("max_tracks_per_artist", 3), cc.get("max_tracks_per_variant", 150)
    styles = {}
    for line in Path(styles_jsonl).read_text(encoding="utf-8").splitlines():
        if line.strip():
            d = json.loads(line)
            styles[d["track_id"]] = d["probs"]
    variants = cfg["variants"]
    sty = {v: spec["discogs_style"] for v, spec in variants.items()}

    pool = defaultdict(list)
    reasons = Counter()
    for r in cands:
        probs = styles.get(r["track_id"])
        if probs is None:
            reasons["no_style_prediction"] += 1
            continue
        best_v, best_gap = None, -1.0
        for v, spec in variants.items():
            if policy == "both" and not set(spec.get("jamendo_tags", [])) & set(r["tags"]):
                continue
            p = probs.get(sty[v], 0.0)
            rival = max((probs.get(sty[o], 0.0) for o in variants if o != v), default=0.0)
            if p >= min_p and p - rival >= margin and p - rival > best_gap:
                best_v, best_gap = v, p - rival
        if best_v is None:
            reasons["rejected_by_labels"] += 1
            continue
        pool[best_v].append({**r, "variant": best_v, "style_prob": probs[sty[best_v]],
                             "margin": round(best_gap, 4)})

    rng = random.Random(cc.get("seed", 42))
    manifest, report = [], {"min_tracks": cc.get("min_tracks", 30), "variants": {},
                            "rejections": dict(reasons)}
    for v in variants:
        items = pool.get(v, [])
        rng.shuffle(items)
        items.sort(key=lambda x: -x["style_prob"])
        per_artist = Counter()
        chosen = []
        for it in items:
            if per_artist[it["artist_id"]] >= cap_artist:
                continue
            per_artist[it["artist_id"]] += 1
            chosen.append(it)
            if len(chosen) >= cap_total:
                break
        manifest += chosen
        report["variants"][v] = {"eligible": len(items), "selected": len(chosen),
                                 "artists": len(per_artist),
                                 "go": len(chosen) >= report["min_tracks"]}
    return manifest, report


def render_selection_report(rep: dict) -> str:
    lines = ["# Corpus selection", "", f"min_tracks per variant: {rep['min_tracks']}", "",
             "| variant | eligible | selected | artists | status |", "|---|---|---|---|---|"]
    for v, r in rep["variants"].items():
        lines.append(f"| {v} | {r['eligible']} | {r['selected']} | {r['artists']} | "
                     f"{'GO' if r['go'] else 'NO-GO'} |")
    lines += ["", f"rejections: {rep['rejections']}",
              "", "NO-GO variant: do not build its profile yet. Options: widen tags, lower "
              "min_style_prob a little, add another CC corpus — never fill the gap by hand-picking."]
    return "\n".join(lines) + "\n"


MANIFEST_FIELDS = ["variant", "track_id", "artist_id", "path", "duration", "tags",
                   "style_prob", "margin"]
