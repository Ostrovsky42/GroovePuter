"""Integration: corpus selection logic + full analyze_track without ML packages."""
import json

import numpy as np

from conftest import CONFIG
from gplab.analyze import SR, analyze_track
from gplab.audio_io import save_wav
from gplab.config import load_config
from gplab.corpus import select
from gplab.features import mask_of
from gplab.schema import read_bars
from gplab.synth import synth_corpus
from test_audio import STEP, kick, snare, hat

CFG = load_config(CONFIG)


def test_select_cross_checks_tags_and_classifier(tmp_path):
    cands = [
        {"track_id": "t1", "artist_id": "a1", "path": "1.mp3", "duration": 200, "tags": ["deephouse"]},
        {"track_id": "t2", "artist_id": "a1", "path": "2.mp3", "duration": 200, "tags": ["deephouse"]},
        {"track_id": "t3", "artist_id": "a2", "path": "3.mp3", "duration": 200, "tags": ["rock"]},
        {"track_id": "t4", "artist_id": "a3", "path": "4.mp3", "duration": 200, "tags": ["house"]},
    ]
    probs = {
        "t1": {"Electronic---Deep House": .6, "Electronic---House": .2},
        "t2": {"Electronic---Deep House": .30, "Electronic---House": .28},  # margin too small
        "t3": {"Electronic---Deep House": .9},                            # tag disagrees
        "t4": {"Electronic---House": .5, "Electronic---Deep House": .1},
    }
    styles = tmp_path / "styles.jsonl"
    styles.write_text("\n".join(json.dumps({"track_id": k, "probs": v}) for k, v in probs.items()))
    manifest, rep = select(cands, styles, CFG)
    got = {m["track_id"]: m["variant"] for m in manifest}
    assert got == {"t1": "house.deep", "t4": "house.base"}
    assert rep["variants"]["house.deep"]["go"] is False      # 1 < min_tracks


def test_holdout_calibration_runs(tmp_path):
    from gplab.cli import main
    from gplab.schema import write_bars
    from gplab.synth import synth_corpus
    write_bars(tmp_path / "bars" / "c.jsonl", synth_corpus(n_sources=14, bars_per_source=6))
    main(["holdout", "--bars", str(tmp_path / "bars"), "--config", str(CONFIG),
          "--out", str(tmp_path / "cal"), "--seeds", "2", "--n", "4"])
    cal = json.loads((tmp_path / "cal" / "calibration.json").read_text())
    assert set(cal) == {"house.base", "house.deep", "house.acid"}
    assert "p90" in cal["house.deep"]["fidelity"]


def test_half_bar_gate_rejects_symmetric_variant():
    """Regression: EM-style half-bar refinement invented structure on symmetric
    content (acid). The permutation gate must keep it off there, on for deep."""
    from collections import defaultdict
    from gplab.grid import align_source
    from gplab.profile import has_half_bar_structure
    by = defaultdict(lambda: defaultdict(list))
    for b in synth_corpus(rotate_share=0.0):
        by[b.variant][b.src].append(b)
    aligned = {v: {s: align_source(sb, CFG["anchors"])[1] for s, sb in d.items()}
               for v, d in by.items()}
    assert has_half_bar_structure(aligned["house.deep"]) is True
    assert has_half_bar_structure(aligned["house.acid"]) is False


def test_analyze_track_end_to_end_without_ml(tmp_path):
    bpm_step = STEP                                      # 120 bpm
    bars, total = 24, 24 * 16 * STEP
    y = np.zeros(int((total + 1) * SR))
    for b in range(bars):
        for s, fn in [(0, kick), (4, kick), (8, kick), (12, kick), (4, snare), (12, snare),
                      (2, lambda: hat(.02)), (6, lambda: hat(.02)), (10, lambda: hat(.02)),
                      (14, lambda: hat(.02))]:
            x = fn()
            i = int((b * 16 + s) * bpm_step * SR)
            y[i:i + len(x)] += x[:len(y) - i]
    y = y / np.abs(y).max() * .9 + 0.001 * np.random.default_rng(1).standard_normal(len(y))
    root = tmp_path / "audio"
    save_wav(root / "x.wav", y, SR)
    beats = np.arange(0, total + 0.01, 0.5)
    (root / "x.wav.beats.tsv").write_text(
        "\n".join(f"{t:.4f}\t{i % 4 + 1}" for i, t in enumerate(beats)))

    row = {"track_id": "x", "variant": "house.base", "path": "x.wav", "duration": str(total)}
    cfg = {**CFG, "segment": {"start_frac": 0.25, "max_seconds": 30}}
    res = analyze_track(row, root, tmp_path / "bars", cfg, drums="bands", bass="simple",
                        beats="file", separate=False)
    assert res["status"] == "ok", res
    out = list(read_bars(tmp_path / "bars"))
    assert len(out) >= 8
    hits = sum(mask_of(b.role("kick")) == 0x1111 and
               mask_of(b.role("snare")) == (1 << 4) | (1 << 12) for b in out)
    assert hits / len(out) >= 0.9
    again = analyze_track(row, root, tmp_path / "bars", cfg, beats="file", separate=False)
    assert again["status"] == "cached"
