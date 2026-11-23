"""gplab command line. Run `python -m gplab -h`."""
from __future__ import annotations

import argparse
import json
import sys
from datetime import date
from pathlib import Path

from .schema import read_bars, write_bars


def _cfg(a):
    from .config import load_config
    return load_config(a.config)


def cmd_tags(a):
    from .corpus import load_jamendo, tag_counts
    for t, n in tag_counts(load_jamendo(a.tsv)).most_common():
        print(f"{n:6d}  {t}")


def cmd_candidates(a):
    from .corpus import candidates, load_jamendo, write_csv
    rows = candidates(load_jamendo(a.tsv), _cfg(a))
    write_csv(a.out, rows, ["track_id", "artist_id", "path", "duration", "tags"])
    print(f"{len(rows)} candidates -> {a.out}")


def cmd_styles(a):
    from .corpus import read_csv, run_styles
    n = run_styles(read_csv(a.candidates), a.audio_root, a.models, a.out, _cfg(a), a.limit)
    print(f"classified {n} new tracks -> {a.out}")


def cmd_select(a):
    from .corpus import (MANIFEST_FIELDS, read_csv, render_selection_report, select,
                         write_csv)
    manifest, rep = select(read_csv(a.candidates), a.styles, _cfg(a))
    out = Path(a.out)
    write_csv(out / "manifest.csv", manifest, MANIFEST_FIELDS)
    (out / "selection_report.md").write_text(render_selection_report(rep), encoding="utf-8")
    (out / "selection_report.json").write_text(json.dumps(rep, indent=1), encoding="utf-8")
    print(render_selection_report(rep))


def cmd_selfcheck(a):
    cfg = _cfg(a)
    ok = True
    if a.models:
        from .adapters.style import load_labels
        labels = set(load_labels(a.models))
        for v, spec in cfg["variants"].items():
            hit = spec["discogs_style"] in labels
            ok &= hit
            print(f"{'OK ' if hit else 'MISSING'} {v}: {spec['discogs_style']}")
        if not ok:
            near = sorted(l for l in labels if "House" in l or "Techno" in l)
            print("labels containing House/Techno:", ", ".join(near))
    for mod in ("beat_this", "demucs", "basic_pitch", "essentia", "mido"):
        try:
            __import__(mod)
            print(f"OK  import {mod}")
        except Exception as e:  # report every missing piece at once
            print(f"--  import {mod}: {type(e).__name__}")
    sys.exit(0 if ok else 1)


def cmd_analyze(a):
    from .analyze import analyze_manifest
    from .corpus import read_csv
    rows = read_csv(a.manifest)
    if a.variant:
        rows = [r for r in rows if r["variant"] == a.variant]
    res = analyze_manifest(rows, a.audio_root, a.out, _cfg(a), limit=a.limit,
                           drums=a.drums, bass=a.bass, beats=a.beats,
                           separate=not a.no_separate)
    from collections import Counter
    print(Counter(r["status"] for r in res))


def cmd_profile(a):
    from .profile import build_profiles, save_profiles
    cfg = _cfg(a)
    bars = [b for b in read_bars(a.bars) if b.take is None]
    profiles = build_profiles(bars, cfg)
    meta = {"family": cfg["family"], "built": date.today().isoformat(),
            "corpus": a.corpus_label or str(a.bars)}
    save_profiles(a.out, profiles, meta)
    for v, p in profiles.items():
        rot = len(p.rotations)
        print(f"{v}: {p.n_sources} sources, {p.n_bars} bars, swing {p.swing[0]}, "
              f"rotated sources {rot}, cells kick/bass {len(p.cells['kick'])}/{len(p.cells['bass'])}")


def cmd_codegen(a):
    from .codegen import write_header
    from .profile import load_profiles
    profiles, meta = load_profiles(a.profiles)
    n = write_header(profiles, meta, a.out)
    print(f"wrote {a.out} ({n} bytes of source)")


def cmd_validate(a):
    from .profile import load_profiles
    from .validate import render_markdown, validate, write_report
    profiles, _ = load_profiles(a.profiles)
    rep = validate(list(read_bars(a.takes)), profiles, _cfg(a))
    write_report(rep, a.out)
    print(render_markdown(rep))
    sys.exit(0 if rep["pass"] else 1)


def holdout(bars, n_takes: int, phrase: int, seed: int, anchors=None):
    """Hold out n_takes sources per variant as pseudo-takes (phrase bars each);
    the rest builds the profiles. Calibrates thresholds on REAL music without leakage.
    Held-out sources get the same downbeat alignment as training sources."""
    import random
    from collections import defaultdict
    from .grid import align_source, rotate_bar
    from .schema import Bar
    rng = random.Random(seed)
    by = defaultdict(lambda: defaultdict(list))
    for b in bars:
        by[b.variant][b.src].append(b)
    train, takes = [], []
    held_src = {}
    for v, srcs in by.items():
        names = sorted(srcs)
        rng.shuffle(names)
        held = [s for s in names if len(srcs[s]) >= phrase][:n_takes]
        for t, s in enumerate(held):
            sb = sorted(srcs[s], key=lambda b: b.bar_idx)
            if anchors:
                sb = align_source(sb, anchors)[1]
            held_src[(v, t)] = sb
            start = rng.randrange(0, len(sb) - phrase + 1)
            for i, b in enumerate(sb[start:start + phrase]):
                takes.append(Bar(src=f"take:{v}:{t}", variant=v, events=b.events,
                                 bar_idx=i, bpm=b.bpm, take=t))
        tr = {s: srcs[s] for s in names if s not in held}
        if anchors and len(tr) >= 5:
            from .profile import has_half_bar_structure, refine_half_bar, source_step_prob
            aligned = {s: align_source(sb, anchors)[1] for s, sb in tr.items()}
            p = source_step_prob(aligned)
            for t in range(len(held) if has_half_bar_structure(aligned) else 0):
                r, _ = refine_half_bar(held_src[(v, t)], p)
                if r:
                    for b in takes:
                        if b.variant == v and b.take == t:
                            b.events = rotate_bar(b, r).events
        train += [b for sb in tr.values() for b in sb]
    return train, takes


CAL_METRICS = {  # check -> (metric key, direction: "max" = lower is better)
    "fidelity": ("ratio_to_runner_up", "max"),
    "accuracy": ("value", "min"),
    "diversity": ("distinct", "min"),
    "identity": ("ratio", "max"),
}


def cmd_holdout(a):
    """Judge REAL held-out corpus tracks as if they were takes, over many seeds.
    A check real music fails often is a miscalibrated threshold, not a generator bug."""
    import numpy as np
    from collections import defaultdict
    from .profile import build_profiles, save_profiles
    from .validate import render_markdown, validate, write_report
    cfg = _cfg(a)
    corpus = [b for b in read_bars(a.bars) if b.take is None]
    out = Path(a.out)
    passed = defaultdict(lambda: defaultdict(list))
    values = defaultdict(lambda: defaultdict(list))
    for k in range(a.seeds):
        seed = a.seed + k
        train, takes = holdout(corpus, a.n, a.phrase, seed, cfg.get("anchors"))
        profiles = build_profiles(train, cfg)
        rep = validate(takes, profiles, cfg)
        if k == 0:
            save_profiles(out / "profiles_train.json", profiles, {"corpus": f"holdout of {a.bars}"})
            write_bars(out / "holdout_takes.jsonl", takes)
            write_report(rep, out)
        for v, r in rep["variants"].items():
            for chk, c in r.get("checks", {}).items():
                if c["pass"] is not None:
                    passed[v][chk].append(bool(c["pass"]))
                if chk in CAL_METRICS:
                    values[v][chk].append(c[CAL_METRICS[chk][0]])
    if a.seeds == 1:
        print(render_markdown(rep))
        return
    cal = {}
    lines = [f"# Holdout calibration: {a.seeds} seeds, {a.n} held-out tracks/variant", "",
             "Pass rate per check (real music judged as takes). Target: >= 90%.", "",
             "| variant | " + " | ".join(["fidelity", "accuracy", "diversity", "identity", "protected"]) + " |",
             "|---|---|---|---|---|---|"]
    for v in sorted(passed):
        row = []
        for chk in ("fidelity", "accuracy", "diversity", "identity", "protected"):
            xs = passed[v].get(chk, [])
            rate = sum(xs) / len(xs) if xs else None
            row.append("—" if rate is None else f"{'✅' if rate >= .9 else '⚠️'} {rate:.0%}")
        lines.append(f"| {v} | " + " | ".join(row) + " |")
        cal[v] = {chk: {"p10": round(float(np.percentile(vals, 10)), 3),
                        "p50": round(float(np.percentile(vals, 50)), 3),
                        "p90": round(float(np.percentile(vals, 90)), 3)}
                  for chk, vals in values[v].items() if vals}
    lines += ["", "Suggested thresholds = what real music reaches in 90% of seeds:", ""]
    for chk, (_, direction) in CAL_METRICS.items():
        q = "p90" if direction == "max" else "p10"
        vals = [cal[v][chk][q] for v in cal if chk in cal[v]]
        if vals:
            worst = max(vals) if direction == "max" else min(vals)
            lines.append(f"- {chk}: {'<=' if direction == 'max' else '>='} {worst} (worst variant {q})")
    text = "\n".join(lines) + "\n"
    out.mkdir(parents=True, exist_ok=True)
    (out / "calibration.md").write_text(text, encoding="utf-8")
    (out / "calibration.json").write_text(json.dumps(cal, indent=1), encoding="utf-8")
    print(text)


def cmd_ab_plan(a):
    from .ab import plan_abx, plan_pref
    if a.mode == "abx":
        info = plan_abx(a.items, a.out, a.trials, a.seed)
    else:
        info = plan_pref(a.items, a.baseline, a.out, a.trials, a.seed)
    print(json.dumps(info, indent=1, default=str))


def cmd_ab_score(a):
    from .ab import score
    print(json.dumps(score(a.plan, a.answers), indent=1))


def cmd_demo(a):
    """Synthetic end-to-end run: corpus -> profiles -> header -> 3 validations."""
    from .codegen import write_header
    from .profile import build_profiles, save_profiles
    from .synth import synth_corpus, synth_takes
    from .validate import render_markdown, validate, write_report
    from .config import load_config
    out = Path(a.out)
    cfg = load_config(a.config)
    corpus = synth_corpus()
    write_bars(out / "bars" / "synthetic.jsonl", corpus)
    profiles = build_profiles(corpus, cfg)
    save_profiles(out / "profiles.json", profiles, {"corpus": "synthetic fixtures (gplab.synth)"})
    write_header(profiles, {"corpus": "synthetic fixtures"}, out / "genre_lab_profiles.h")
    summary = {}
    for mode in ("good", "stuck", "wrong"):
        takes = synth_takes(mode)
        write_bars(out / f"takes_{mode}.jsonl", takes)
        rep = validate(takes, profiles, cfg)
        write_report(rep, out / f"report_{mode}")
        summary[mode] = rep["pass"]
        print(f"## takes={mode}\n" + render_markdown(rep))
    expected = {"good": True, "stuck": False, "wrong": False}
    print("expected:", expected, "got:", summary)
    sys.exit(0 if summary == expected else 1)


def cmd_diversity(a):
    from .diversity import diversity, render_markdown
    from .schema import ROLES, read_bars

    roles = a.roles.split(",") if a.roles else list(ROLES)
    exclude: dict[str, set[int]] = {}
    for token in a.exclude_steps:
        role, _, steps = token.partition(":")
        if not steps:
            raise SystemExit(f"--exclude-steps needs ROLE:STEPS, got {token!r}")
        exclude.setdefault(role, set()).update(int(x) for x in steps.split(","))
    profiles = None
    if a.profiles:
        from .profile import load_profiles
        profiles, _meta = load_profiles(a.profiles)
    res = diversity(read_bars(a.takes), roles, a.tau, exclude=exclude, profiles=profiles)
    text = render_markdown(res, roles)
    print(text, end="")
    if a.out:
        Path(a.out).write_text(text, encoding="utf-8")


def main(argv=None):
    p = argparse.ArgumentParser(prog="gplab", description="GroovePuter genre lab")
    sub = p.add_subparsers(dest="cmd", required=True)

    def add(name, fn, help_):
        sp = sub.add_parser(name, help=help_)
        sp.set_defaults(fn=fn)
        return sp

    s = add("tags", cmd_tags, "list MTG-Jamendo genre tags with counts")
    s.add_argument("--tsv", required=True)

    s = add("candidates", cmd_candidates, "filter Jamendo tracks by family tags")
    s.add_argument("--tsv", required=True); s.add_argument("--config", required=True)
    s.add_argument("--out", required=True)

    s = add("styles", cmd_styles, "Discogs-400 style probabilities (resumable)")
    s.add_argument("--candidates", required=True); s.add_argument("--audio-root", required=True)
    s.add_argument("--models", required=True); s.add_argument("--config", required=True)
    s.add_argument("--out", required=True); s.add_argument("--limit", type=int)

    s = add("select", cmd_select, "cross-check labels -> manifest + GO/NO-GO")
    s.add_argument("--candidates", required=True); s.add_argument("--styles", required=True)
    s.add_argument("--config", required=True); s.add_argument("--out", required=True)

    s = add("selfcheck", cmd_selfcheck, "verify style names and ML imports")
    s.add_argument("--config", required=True); s.add_argument("--models")

    s = add("analyze", cmd_analyze, "audio -> bars.jsonl (resumable)")
    s.add_argument("--manifest", required=True); s.add_argument("--audio-root", required=True)
    s.add_argument("--config", required=True); s.add_argument("--out", required=True)
    s.add_argument("--variant"); s.add_argument("--limit", type=int)
    s.add_argument("--drums", choices=["bands", "midi"], default="bands")
    s.add_argument("--bass", choices=["simple", "basicpitch"], default="basicpitch")
    s.add_argument("--beats", choices=["beat_this", "file"], default="beat_this")
    s.add_argument("--no-separate", action="store_true")

    s = add("profile", cmd_profile, "bars -> profiles.json")
    s.add_argument("--bars", required=True); s.add_argument("--config", required=True)
    s.add_argument("--out", required=True); s.add_argument("--corpus-label")

    s = add("codegen", cmd_codegen, "profiles.json -> constexpr C++ header")
    s.add_argument("--profiles", required=True); s.add_argument("--out", required=True)

    s = add("validate", cmd_validate, "takes.jsonl vs profiles -> report (exit 1 on FAIL)")
    s.add_argument("--takes", required=True); s.add_argument("--profiles", required=True)
    s.add_argument("--config", required=True); s.add_argument("--out", required=True)

    s = add("diversity", cmd_diversity, "distinct/N and mean_pair_hits per role, no profiles")
    s.add_argument("--takes", required=True)
    s.add_argument("--roles", default=None, help="comma list, default all roles (e.g. kick,snare,bass)")
    s.add_argument("--tau", type=float, default=None)
    s.add_argument("--exclude-steps", nargs="*", default=[], metavar="ROLE:STEPS",
                   help="e.g. kick:0,4,8,12 snare:4,12 (steps 0..15, ignored for every variant)")
    s.add_argument("--profiles", default=None,
                   help="profiles.json: its protected steps are ignored for each variant it has")
    s.add_argument("--out")

    s = add("holdout", cmd_holdout, "calibrate thresholds: held-out corpus tracks as takes")
    s.add_argument("--bars", required=True); s.add_argument("--config", required=True)
    s.add_argument("--out", required=True); s.add_argument("--n", type=int, default=8)
    s.add_argument("--phrase", type=int, default=4); s.add_argument("--seed", type=int, default=5)
    s.add_argument("--seeds", type=int, default=10, help="repeat with seed, seed+1, ...")

    s = add("ab-plan", cmd_ab_plan, "blind ABX / preference test plan")
    s.add_argument("--mode", choices=["abx", "pref"], required=True)
    s.add_argument("--items", required=True); s.add_argument("--baseline")
    s.add_argument("--out", required=True); s.add_argument("--trials", type=int, default=16)
    s.add_argument("--seed", type=int, default=1)

    s = add("ab-score", cmd_ab_score, "score answers.csv against the hidden key")
    s.add_argument("--plan", required=True); s.add_argument("--answers")

    s = add("demo", cmd_demo, "synthetic end-to-end proof run")
    s.add_argument("--config", default=str(Path(__file__).resolve().parents[2] / "config" / "pilot_house.yaml"))
    s.add_argument("--out", default="out/demo")

    a = p.parse_args(argv)
    if a.cmd == "ab-plan" and a.mode == "pref" and not a.baseline:
        p.error("--baseline is required for --mode pref")
    a.fn(a)


if __name__ == "__main__":
    main()
