#!/usr/bin/env python3
r"""Same idiom metrics for the reference MIDI corpus and for generator output.

Usage (dependencies only via uv, nothing is installed):
  uv run -q --with mido python -I tools/gen_probe/idiom_metrics.py corpus  [--midi-dir DIR]
  uv run -q --with mido python -I tools/gen_probe/idiom_metrics.py gen     run.tsv
  uv run -q --with mido python -I tools/gen_probe/idiom_metrics.py report  run.tsv [--midi-dir DIR] [--json out.json]
  uv run -q --with mido python -I tools/gen_probe/idiom_metrics.py grid FILE.mid TRACK[,TRACK] BAR0 BAR1

`gen` input is the TSV printed by idiomprobe.cpp (G on MATERIAL, one row per genre/press/voice/bar);
lines that are not 22 tab-separated fields are ignored, so the raw probe stdout can be piped in.
Reproduce:
  tools/gen_probe/build.sh tools/gen_probe/idiomprobe.cpp /tmp/idiomprobe
  /tmp/idiomprobe 10 > tools/gen_probe/idiomprobe_run.tsv     # six genres; drop engine chatter with grep -P "^(#|[A-Za-z]+\t\d+\t[AB]\t)"
  uv run -q --with mido python -I tools/gen_probe/idiom_metrics.py report tools/gen_probe/idiomprobe_run.tsv

Model. A voice is a list of onsets (t, pitch, dur): t = absolute 16th step (bar = 16 steps), pitch =
the lowest note of a simultaneous group for bass and the highest for lead (so every voice is one
monophonic line), dur = length in steps (a tie in generator output extends it). The same code
computes every metric for both sources.

Normalisation of MIDI: file BPM > 180 means the file was written at double speed; one generator
step is then tpb/2 ticks (a file 8th), otherwise tpb/4 (a file 16th). 4/4 is assumed (checked: all
12 files are 4/4).

Segments. A metric is pooled over segments. Generator: one TAKE (4 bars) is one segment. Corpus:
either the whole track ("piece") or non-overlapping 4-bar windows ("win4"); win4 is the fair
comparison to a 4-bar TAKE, because a period of 4 or 8 bars cannot be seen inside a TAKE.
"""
import collections
import json
import sys

BEATS = (0, 4, 8, 12)
OFFB = (2, 6, 10, 14)
THRESH = 0.6

# (label, group, file, bass track, lead track, note)
CORPUS = [
    ("Nightcall", "Outrun", "_Kavinsky - Nightcall.mid", 2, 4, ""),
    ("Musikk per Automatikk", "Darksynth", "Elliott Berlin - Musikk per Automatikk (Hotline Miami OST).mid", 1, 0, ""),
    ("She Swallowed", "Darksynth", "El Trigr3 - She Swallowed Burning Coals (Hotline Miami 2_ Wrong Number OST).mid", 1, 5, ""),
    ("Run", "Darksynth", "Iamthekidyouknowwhatimean - Run (Hotline Miami 2 OST).mid", 1, 0, "piano bass, 8ths"),
    ("Run (16th pulse bass)", "Darksynth", "Iamthekidyouknowwhatimean - Run (Hotline Miami 2 OST).mid", 3, 0, "same lead, bass = synth pulse"),
    ("Hydrogen", "Darksynth", "M_O_O_N - Hydrogen (Hotline Miami OST).mid", 0, 3, ""),
    ("Dust", "Darksynth", "hotline Miami- Dust.mid", 1, 0, ""),
    ("Around the World (bass + vocoder)", "House", "Daft Punk - Around the World (BEST FULL VERSION DONE).mid", 12, 9, ""),
    ("Around the World (funk bass + 8-bit)", "House", "Daft Punk - Around the World (BEST FULL VERSION DONE).mid", 10, 0, ""),
    ("One More Time", "House", "Daft Punk \u2014 One More Time [MIDIfind.com].mid", 5, 0, "lead = chordal horn"),
    ("Harder Better (vocal chop)", "House", "Harder, Better, Faster, Stronger _ Daft Punk.mid", 6, 3, "3 notes per 2 bars"),
    ("Harder Better (8-bit arp)", "House", "Harder, Better, Faster, Stronger _ Daft Punk.mid", 6, 8, "line below the bass"),
    ("Get Lucky (Rhodes vocal)", "FunkSoul", "_Daft Punk - Get Lucky (BEST FULL DONE).mid", 1, 8, ""),
    ("Get Lucky (guitar comping)", "FunkSoul", "_Daft Punk - Get Lucky (BEST FULL DONE).mid", 1, 0, "not a lead: 16th chord comping"),
    ("Robot Rock", "other", "Daft Punk - Robot Rock (FULL DONE).mid", 2, 8, "no generator genre"),
    ("Hydrogen (3-3-2)", "Techno", "M_O_O_N - Hydrogen (Hotline Miami OST).mid", 0, 3, ""),
]


# ---------------------------------------------------------------- loading

def load_midi_voices(path, bass_tr, lead_tr):
    import mido
    m = mido.MidiFile(path, clip=True)
    bpm = None
    for t in m.tracks:
        for x in t:
            if x.type == "set_tempo":
                bpm = mido.tempo2bpm(x.tempo)
                break
        if bpm:
            break
    tpb = m.ticks_per_beat
    step = tpb / 2 if (bpm or 0) > 180 else tpb / 4
    out = {}
    for name, ti, pick in (("bass", bass_tr, min), ("lead", lead_tr, max)):
        notes = []
        ab = 0
        on = {}
        for x in m.tracks[ti]:
            ab += x.time
            if x.type == "note_on" and x.velocity > 0:
                on[x.note] = ab
            elif x.type in ("note_off", "note_on") and x.note in on:
                st = on.pop(x.note)
                notes.append((int(round(st / step)), max(1, int(round((ab - st) / step))), x.note))
        by_t = collections.defaultdict(list)
        for t, d, n in notes:
            by_t[t].append((n, d))
        voice = []
        poly = 0
        for t in sorted(by_t):
            grp = by_t[t]
            if len(grp) > 1:
                poly += 1
            n = pick(g[0] for g in grp)
            d = max(g[1] for g in grp)
            voice.append((t, n, min(d, 16)))
        out[name] = (voice, poly)
    return out, bpm, step


def load_gen_tsv(path):
    """-> {(genre, press): {'A': [bar cells], 'B': [...]}}, cells = list of 16 (pitch, slide, accent)|None"""
    takes = collections.OrderedDict()
    meta = {}
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("#"):
            continue
        f = line.split("\t")
        if len(f) < 22 or not f[1].isdigit():  # engine init chatter shares stdout
            continue
        genre, press, voice, bar, req, got = f[0], int(f[1]), f[2], int(f[3]), int(f[4]), int(f[5])
        cells = []
        for c in f[6:22]:
            if c == ".":
                cells.append(None)
                continue
            accent = c.endswith("!")
            c = c.rstrip("!")
            slide = c.endswith("~")
            c = c.rstrip("~")
            cells.append((int(c), slide, accent))
        takes.setdefault((genre, press), {}).setdefault(voice, []).append(cells)
        meta[(genre, press)] = (req, got)
    return takes, meta


def gen_voice(bars):
    """Bars of 16 cells -> onsets. slide on the same pitch as the previous step = tie (extends)."""
    onsets = []
    slides = accents = 0
    for b, cells in enumerate(bars):
        last = None
        for i, c in enumerate(cells):
            if c is None:
                last = None
                continue
            pitch, slide, accent = c
            if slide and last is not None and cells[i - 1] is not None and cells[i - 1][0] == pitch:
                t0, p0, d0 = onsets[last]
                onsets[last] = (t0, p0, d0 + 1)
                continue
            onsets.append((b * 16 + i, pitch, 1))
            last = len(onsets) - 1
            slides += bool(slide)
            accents += bool(accent)
    return onsets, slides, accents


# ---------------------------------------------------------------- metrics

def split_windows(voice, nbars, width):
    segs = []
    for w0 in range(0, nbars, width):
        seg = [(t - w0 * 16, p, d) for t, p, d in voice if w0 * 16 <= t < (w0 + width) * 16]
        segs.append((seg, min(width, nbars - w0)))
    return segs


def bar_contents(seg, nbars, with_pitch):
    bars = [[] for _ in range(nbars)]
    for t, p, _d in seg:
        k = t // 16
        if 0 <= k < nbars:
            bars[k].append((t % 16, p) if with_pitch else t % 16)
    return [tuple(sorted(b)) for b in bars]


def loop_pairs(segs, with_pitch, P):
    same = tot = 0
    for seg, nb in segs:
        bars = bar_contents(seg, nb, with_pitch)
        for k in range(nb - P):
            a, b = bars[k], bars[k + P]
            if not a and not b:
                continue
            tot += 1
            same += a == b
    return same, tot


def voice_metrics(segs, periods=(1, 2, 4, 8)):
    """segs: list of (onsets, nbars). Returns dict of metrics (pooled)."""
    att_bars = []
    pcs_bars = []
    steps = collections.Counter()
    durs = []
    iv = collections.Counter()
    total_bars = 0
    for seg, nb in segs:
        total_bars += nb
        per = collections.defaultdict(list)
        for t, p, d in seg:
            per[t // 16].append(p)
            steps[t % 16] += 1
            durs.append(d)
        for k in range(nb):
            if per.get(k):
                att_bars.append(len(per[k]))
                pcs_bars.append(len({p % 12 for p in per[k]}))
        seq = [p for _t, p, _d in sorted(seg)]
        for a, b in zip(seq, seq[1:]):
            d = abs(b - a)
            iv["n"] += 1
            if d == 0:
                iv["rep"] += 1
            elif d <= 2:
                iv["step"] += 1
            elif d <= 4:
                iv["mid"] += 1
            else:
                iv["leap"] += 1
            if d in (12, 24):
                iv["oct"] += 1
    n = sum(steps.values())
    moves = [0, 0]  # bar-to-bar change of the lowest pitch class (harmony proxy), consecutive active bars
    for seg, nb in segs:
        low = {}
        for t, p, _d in seg:
            low[t // 16] = min(low.get(t // 16, 999), p)
        for k in range(nb - 1):
            if k in low and k + 1 in low:
                moves[1] += 1
                moves[0] += (low[k] % 12) != (low[k + 1] % 12)
    dist = []
    for seg, nb in segs:
        bc = [b for b in bar_contents(seg, nb, True) if b]
        if bc and nb == 4:
            dist.append(len(set(bc)))
    r = {"bars": total_bars, "active_bars": len(att_bars), "onsets": n}
    r["root_moves"] = moves[0] / moves[1] if moves[1] else None  # share of bar->bar steps whose lowest pitch class changes
    r["distinct4"] = sum(dist) / len(dist) if dist else None  # distinct non-empty bars per 4-bar window
    r["silent_bar_share"] = 1 - len(att_bars) / total_bars if total_bars else None
    r["att_per_bar"] = sum(att_bars) / len(att_bars) if att_bars else None
    r["pcs_per_bar"] = sum(pcs_bars) / len(pcs_bars) if pcs_bars else None
    r["on_beat"] = sum(steps[s] for s in BEATS) / n if n else None
    r["off8"] = sum(steps[s] for s in OFFB) / n if n else None
    r["odd"] = sum(v for s, v in steps.items() if s % 2) / n if n else None
    r["dur"] = sum(durs) / len(durs) if durs else None
    ni = iv["n"]
    for k in ("rep", "step", "mid", "leap", "oct"):
        r[k] = iv[k] / ni if ni else None
    for with_pitch, tag in ((True, "n"), (False, "r")):
        for P in periods:
            s, t = loop_pairs(segs, with_pitch, P)
            r[f"{tag}{P}"] = s / t if t else None
            r[f"{tag}{P}_pairs"] = t
        lab = "none"
        for P in periods:
            s, t = loop_pairs(segs, with_pitch, P)
            if t and s / t >= THRESH:
                lab = str(P)
                break
        r[f"{tag}_period"] = lab
    return r


def pair_metrics(bass_segs, lead_segs):
    co = lead_n = above = above_n = holes = bass_bars = 0
    for (bs, nb), (ls, _nl) in zip(bass_segs, lead_segs):
        bset = collections.defaultdict(set)
        bmax = {}
        for t, p, _d in bs:
            bset[t // 16].add(t % 16)
            bmax[t // 16] = max(bmax.get(t // 16, -1), p)
        lbars = collections.Counter(t // 16 for t, _p, _d in ls)
        for t, p, _d in ls:
            k = t // 16
            lead_n += 1
            co += (t % 16) in bset.get(k, ())
            if k in bmax:
                above_n += 1
                above += p > bmax[k]
        for k in range(nb):
            if bset.get(k):
                bass_bars += 1
                holes += lbars.get(k, 0) == 0
    return {
        "lead_on_bass": co / lead_n if lead_n else None,
        "lead_above": above / above_n if above_n else None,
        "answer_holes": holes / bass_bars if bass_bars else None,
    }


# ---------------------------------------------------------------- sources

# rows that are kept in the tables but excluded from corpus aggregates and distances
EXCLUDE = {"Get Lucky (guitar comping)", "Harder Better (8-bit arp)", "Run (16th pulse bass)",
           "Hydrogen (3-3-2)"}  # last one duplicates Hydrogen (counted once, as Darksynth)
GEN_GROUP = {"Outrun": "Outrun", "Darksynth": "Darksynth", "House": "House", "FunkSoul": "FunkSoul",
             "Techno": "Techno", "Rave": None}
DIST_METRICS = ["att_per_bar", "pcs_per_bar", "on_beat", "off8", "odd", "rep", "step", "leap", "n2", "r1", "distinct4"]


def _vec(m):
    return [m[k] if m.get(k) is not None else 0.0 for k in DIST_METRICS]


def distances(corpus, gen):
    """Mean |g - ref| / scale over DIST_METRICS. scale = max(std over corpus voices, 0.05 * |median|, 0.02)."""
    import statistics
    out = {}
    for voice in ("bass", "lead"):
        ref = [(r["label"], r["group"], _vec(r[f"win4_{voice}"])) for r in corpus if r["label"] not in EXCLUDE]
        refg = [(r["label"], r["group"], _vec(r[f"win4_{voice}"])) for r in corpus
                if r["label"] not in EXCLUDE - {"Hydrogen (3-3-2)"}]  # group means keep the Techno copy of Hydrogen
        cols = list(zip(*[v for _l, _g, v in ref]))
        med = [statistics.median(c) for c in cols]
        sc = [max(statistics.pstdev(c), 0.05 * abs(m), 0.02) for c, m in zip(cols, med)]

        def d(a, b):
            return sum(abs(x - y) / s for x, y, s in zip(a, b, sc)) / len(sc)
        for g in gen:
            v = _vec(g[voice])
            nearest = min(((d(v, rv), lab) for lab, _grp, rv in ref))
            grp = GEN_GROUP.get(g["label"])
            own = [rv for _l, gg, rv in refg if gg == grp] if grp else []
            own_d = d(v, [sum(c) / len(c) for c in zip(*own)]) if own else None
            out[(g["label"], voice)] = {"to_median": d(v, med), "nearest": nearest, "to_group": own_d,
                                        "per_metric_z": {k: (x - m) / s for k, x, m, s in zip(DIST_METRICS, v, med, sc)}}
    return out


def corpus_rows(midi_dir):
    rows = []
    for label, group, fn, bt, lt, note in CORPUS:
        voices, bpm, step = load_midi_voices(f"{midi_dir}/{fn}", bt, lt)
        (bass, bpoly), (lead, lpoly) = voices["bass"], voices["lead"]
        nb = (max([t for t, _p, _d in bass + lead] or [0]) // 16) + 1
        row = {"label": label, "group": group, "file": fn, "bass_track": bt, "lead_track": lt,
               "bpm_file": round(bpm), "doubled": bpm > 180, "bars_total": nb,
               "bass_poly": bpoly / max(1, len(bass)), "lead_poly": lpoly / max(1, len(lead))}
        for mode, width in (("win4", 4), ("piece", nb)):
            bsegs = split_windows(bass, nb, width)
            lsegs = split_windows(lead, nb, width)
            periods = (1, 2) if mode == "win4" else (1, 2, 4, 8)
            row[f"{mode}_bass"] = voice_metrics(bsegs, periods)
            row[f"{mode}_lead"] = voice_metrics(lsegs, periods)
            row[f"{mode}_pair"] = pair_metrics(bsegs, lsegs)
        rows.append(row)
    return rows


def gen_rows(tsv):
    takes, meta = load_gen_tsv(tsv)
    genres = list(collections.OrderedDict.fromkeys(g for g, _p in takes))
    rows = []
    for g in genres:
        bsegs, lsegs = [], []
        extra = {"slides_A": 0, "slides_B": 0, "accents_A": 0, "accents_B": 0}
        patterns = collections.defaultdict(list)
        for (gg, press), vs in takes.items():
            if gg != g:
                continue
            nb = len(vs["A"])
            b, s1, a1 = gen_voice(vs["A"])
            l, s2, a2 = gen_voice(vs["B"])
            bsegs.append((b, nb))
            lsegs.append((l, nb))
            extra["slides_A"] += s1
            extra["slides_B"] += s2
            extra["accents_A"] += a1
            extra["accents_B"] += a2
            # bar identity = (step, pitch) only, the same notion of "same bar" as the loop metrics
            for v in ("A", "B"):
                patterns[v].append(tuple(tuple((i, c[0]) for i, c in enumerate(bar) if c) for bar in vs[v]))
        def forms(voice):
            c = collections.Counter()
            for t in patterns[voice]:
                seen = {}
                c["".join(seen.setdefault(bar, "ABCDEFGH"[len(seen)]) for bar in t)] += 1
            return c
        row_forms = {"A": forms("A"), "B": forms("B")}
        reqs = {meta[k][0] for k in meta if k[0] == g}
        gots = {meta[k][1] for k in meta if k[0] == g}
        n = len(bsegs)
        row = {"label": g, "group": g, "takes": n, "requested_bars": sorted(reqs), "got_bars": sorted(gots),
               "unique_takes_A": len(set(patterns["A"])), "unique_takes_B": len(set(patterns["B"])),
               "distinct_bars_A": len({b for t in patterns["A"] for b in t}),
               "distinct_bars_B": len({b for t in patterns["B"] for b in t}),
               "slides_per_take_A": extra["slides_A"] / n, "slides_per_take_B": extra["slides_B"] / n,
               "accents_per_take_A": extra["accents_A"] / n, "accents_per_take_B": extra["accents_B"] / n,
               "forms_A": row_forms["A"].most_common(4), "forms_B": row_forms["B"].most_common(4),
               "distinct_per_take_A": sum(len(set(t)) for t in patterns["A"]) / n,
               "distinct_per_take_B": sum(len(set(t)) for t in patterns["B"]) / n,
               "bass": voice_metrics(bsegs, (1, 2)), "lead": voice_metrics(lsegs, (1, 2)),
               "pair": pair_metrics(bsegs, lsegs)}
        # per-press spread of the headline metrics
        per = []
        for (gg, press), vs in takes.items():
            if gg != g:
                continue
            nb = len(vs["A"])
            b, _, _ = gen_voice(vs["A"])
            l, _, _ = gen_voice(vs["B"])
            per.append((voice_metrics([(b, nb)], (1, 2)), voice_metrics([(l, nb)], (1, 2)), pair_metrics([(b, nb)], [(l, nb)])))
        row["per_press"] = [{"bass": a, "lead": b, "pair": c} for a, b, c in per]
        rows.append(row)
    return rows


# ---------------------------------------------------------------- printing

COLS = [("att_per_bar", "att/bar", "{:.1f}"), ("pcs_per_bar", "pcs/bar", "{:.1f}"),
        ("on_beat", "0/4/8/12", "{:.0%}"), ("off8", "2/6/10/14", "{:.0%}"), ("odd", "odd", "{:.0%}"),
        ("dur", "dur", "{:.1f}"), ("rep", "rep", "{:.0%}"), ("step", "step1-2", "{:.0%}"),
        ("leap", "leap>=5", "{:.0%}"), ("oct", "oct", "{:.0%}"),
        ("distinct4", "uniq bars/4", "{:.1f}"), ("root_moves", "low-pc moves", "{:.0%}"), ("n1", "N P=1", "{:.2f}"), ("n2", "N P=2", "{:.2f}"), ("r1", "R P=1", "{:.2f}"), ("r2", "R P=2", "{:.2f}")]


def fmt(v, f):
    return "-" if v is None else f.format(v)


def md_table(title, rows):
    head = ["source"] + [c[1] for c in COLS]
    out = [f"{title}", "", "| " + " | ".join(head) + " |", "|" + "---|" * len(head)]
    for lab, m in rows:
        out.append("| " + " | ".join([lab] + [fmt(m.get(k), f) for k, _h, f in COLS]) + " |")
    return "\n".join(out)


def main(argv):
    import os
    cmd = argv[1] if len(argv) > 1 else ""
    midi_dir = "/home/gg/pro/arduino.ESP32-S3-Touch-AMOLED-1.75/Cardputer/miniacid/docs/midi"
    if "--midi-dir" in argv:
        i = argv.index("--midi-dir")
        midi_dir = argv[i + 1]
        del argv[i:i + 2]
    jpath = None
    if "--json" in argv:
        i = argv.index("--json")
        jpath = argv[i + 1]
        del argv[i:i + 2]
    if cmd == "grid":
        grid(argv[2], [int(x) for x in argv[3].split(",")], int(argv[4]), int(argv[5]))
        return
    corpus = gen = None
    if cmd in ("corpus", "report"):
        corpus = corpus_rows(midi_dir)
    if cmd in ("gen", "report"):
        gen = gen_rows(argv[2])
    if jpath:
        json.dump({"corpus": corpus, "gen": gen}, open(jpath, "w"), indent=1, ensure_ascii=False, default=str)
    for rows, tag in ((corpus, "corpus"), (gen, "gen")):
        if not rows:
            continue
        for voice in ("bass", "lead"):
            if tag == "corpus":
                sel = [(r["label"] + f" [{r['group']}]", r[f"win4_{voice}"]) for r in rows]
            else:
                sel = [(r["label"], r[voice]) for r in rows]
            print(md_table(f"### {tag} {voice} ({'win4' if tag == 'corpus' else 'TAKE 4B'})", sel), "\n")
        print("### pair", tag)
        for r in rows:
            p = r["win4_pair"] if tag == "corpus" else r["pair"]
            print(f"{r['label']}: lead-on-bass {fmt(p['lead_on_bass'], '{:.0%}')}, lead above {fmt(p['lead_above'], '{:.0%}')}, "
                  f"answer holes {fmt(p['answer_holes'], '{:.0%}')}")
        print()
    if corpus and gen:
        dist = distances(corpus, gen)
        print("### distance to the corpus (mean |g-ref|/scale over %s; lower = closer)" % ",".join(DIST_METRICS))
        print("| genre | voice | to corpus median | nearest track (dist) | to own-group mean |")
        print("|---|---|---|---|---|")
        for (g, voice), d in dist.items():
            print(f"| {g} | {voice} | {d['to_median']:.2f} | {d['nearest'][1]} ({d['nearest'][0]:.2f}) | "
                  f"{fmt(d['to_group'], '{:.2f}')} |")
        print()
        print("### per-press spread of a TAKE (min..max over presses)")
        for g in gen:
            def rng(voice, key):
                vals = [p[voice][key] for p in g["per_press"] if p[voice].get(key) is not None]
                return f"{min(vals):.2f}..{max(vals):.2f}" if vals else "-"
            print(f"{g['label']}: takes {g['takes']}, unique A {g['unique_takes_A']} B {g['unique_takes_B']}, "
                  f"distinct bars A {g['distinct_bars_A']} B {g['distinct_bars_B']}; lead N P=2 {rng('lead','n2')}, "
                  f"lead R P=1 {rng('lead','r1')}, bass N P=2 {rng('bass','n2')}, bass step {rng('bass','step')}, "
                  f"lead step {rng('lead','step')}")
            print(f"   distinct bars per TAKE: bass {g['distinct_per_take_A']:.1f}, lead {g['distinct_per_take_B']:.1f}; "
                  f"forms bass {g['forms_A']}, lead {g['forms_B']}")
        print()
        print("### corpus aggregate (median over %d voices, win4)" % len([r for r in corpus if r['label'] not in EXCLUDE]))
        import statistics
        for voice in ("bass", "lead"):
            ref = [r[f"win4_{voice}"] for r in corpus if r["label"] not in EXCLUDE]
            print(voice, " ".join(f"{k}={statistics.median([m[k] for m in ref if m.get(k) is not None]):.2f}" for k in DIST_METRICS))
        print()
    if corpus:
        print("### corpus whole-piece periods (notes/rhythm, share and label)")
        for r in corpus:
            for v in ("bass", "lead"):
                m = r[f"piece_{v}"]
                print(f"{r['label']} {v}: notes P1 {fmt(m['n1'], '{:.2f}')} P2 {fmt(m['n2'], '{:.2f}')} "
                      f"P4 {fmt(m['n4'], '{:.2f}')} P8 {fmt(m['n8'], '{:.2f}')} -> {m['n_period']} | rhythm "
                      f"P1 {fmt(m['r1'], '{:.2f}')} P2 {fmt(m['r2'], '{:.2f}')} P4 {fmt(m['r4'], '{:.2f}')} "
                      f"P8 {fmt(m['r8'], '{:.2f}')} -> {m['r_period']}; bars {m['bars']} active {m['active_bars']}")


def grid(path, tracks, b0, b1):
    import mido
    names = "C C# D D# E F F# G G# A A# B".split()
    m = mido.MidiFile(path, clip=True)
    bpm = next((mido.tempo2bpm(x.tempo) for t in m.tracks for x in t if x.type == "set_tempo"), 120)
    step = m.ticks_per_beat / (2 if bpm > 180 else 4)
    print(f"# bpm {bpm:.0f} step {step:.0f} ticks; bar = 16 steps")
    for ti in tracks:
        ab = 0
        on = {}
        ev = []
        for x in m.tracks[ti]:
            ab += x.time
            if x.type == "note_on" and x.velocity > 0:
                on[x.note] = ab
            elif x.type in ("note_off", "note_on") and x.note in on:
                st = on.pop(x.note)
                ev.append((st, ab - st, x.note))
        print("== track", ti, m.tracks[ti].name)
        for bar in range(b0, b1):
            cells = ["."] * 16
            for st, d, n in ev:
                k = round(st / step) - bar * 16
                if 0 <= k < 16:
                    lab = names[n % 12] + str(n // 12 - 1) + "~" * min(3, max(0, round(d / step) - 1))
                    cells[k] = lab if cells[k] == "." else cells[k] + "+"
            print(f"{bar:3d}| " + " ".join(f"{c:5s}" for c in cells))


if __name__ == "__main__":
    main(sys.argv)
