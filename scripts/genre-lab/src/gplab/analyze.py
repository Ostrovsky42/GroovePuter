"""Audio -> bars.jsonl, one file per track (resumable, safe to re-run).

Per track: decode segment -> beats/downbeats on the mix -> tempo gate ->
separate drums/bass -> transcribe -> grid -> bars.
"""
from __future__ import annotations

import json
import time
import traceback
from pathlib import Path

from .audio_io import load_mono, save_wav
from .grid import events_to_bars
from .schema import write_bars

SR = 44100


def analyze_track(row: dict, audio_root, out_dir, cfg, *, drums="bands", bass="simple",
                  beats="beat_this", separate=True, work_dir=None) -> dict:
    t0 = time.time()
    tid, variant = row["track_id"], row["variant"]
    out_file = Path(out_dir) / variant / f"{tid.replace('/', '_')}.jsonl"
    if out_file.exists():
        return {"track_id": tid, "status": "cached"}

    seg = cfg.get("segment", {})
    dur = float(row["duration"])
    start = dur * seg.get("start_frac", 0.2)
    length = min(seg.get("max_seconds", 90), dur - start)
    work = Path(work_dir or Path(out_dir) / "_work") / tid.replace("/", "_")
    work.mkdir(parents=True, exist_ok=True)
    clip = work / "clip.wav"
    if not clip.exists():
        save_wav(clip, load_mono(Path(audio_root) / row["path"], SR, start, length), SR)

    from .adapters import beats as B
    if beats == "beat_this":
        bt, db = B.track_beat_this(clip)
    else:  # precomputed for the ORIGINAL file: shift into clip time
        bt, db = B.track_file(Path(audio_root) / row["path"])
        bt, db = bt - start, db - start
        bt, db = bt[(bt >= 0) & (bt <= length)], db[(db >= 0) & (db <= length)]
    ok, bpm = B.tempo_ok(bt, cfg.get("bpm_range", (60, 200)))
    if not ok:
        return {"track_id": tid, "status": "rejected_tempo", "bpm": round(bpm, 2)}

    if separate:
        from .adapters.separate import separate as sep
        stems = sep(clip, work / "stems")
        drum_src, bass_src = stems["drums"], stems["bass"]
    else:
        drum_src = bass_src = clip

    from .adapters import drums as D
    if drums == "midi":
        ev = D.transcribe_midi(Path(str(drum_src) + ".mid"))
    else:
        ev = D.transcribe_bands(load_mono(drum_src, SR), SR)

    from .adapters import bass as BS
    ev["bass"] = (BS.transcribe_basicpitch(bass_src) if bass == "basicpitch"
                  else BS.transcribe_simple(load_mono(bass_src, SR), SR))

    bars = events_to_bars(ev, bt, db, src=f"jamendo:{tid}", variant=variant)
    min_bars = cfg.get("min_bars_per_track", 8)
    if len(bars) < min_bars:
        return {"track_id": tid, "status": "rejected_bars", "bars": len(bars)}
    write_bars(out_file, bars)
    return {"track_id": tid, "status": "ok", "bars": len(bars), "bpm": round(bpm, 2),
            "sec": round(time.time() - t0, 1)}


def analyze_manifest(rows, audio_root, out_dir, cfg, limit=None, **kw) -> list[dict]:
    log_path = Path(out_dir) / "analyze_log.jsonl"
    log_path.parent.mkdir(parents=True, exist_ok=True)
    results = []
    for i, row in enumerate(rows):
        if limit is not None and i >= limit:
            break
        try:
            r = analyze_track(row, audio_root, out_dir, cfg, **kw)
        except Exception as e:  # one bad file must not kill a 10-hour run
            r = {"track_id": row["track_id"], "status": "error", "error": repr(e),
                 "trace": traceback.format_exc(limit=3)}
        r["variant"] = row["variant"]
        results.append(r)
        with open(log_path, "a", encoding="utf-8") as fh:
            fh.write(json.dumps(r) + "\n")
        print(f"[{i + 1}/{len(rows)}] {row['variant']} {row['track_id']}: {r['status']}", flush=True)
    return results
