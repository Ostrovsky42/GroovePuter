import shutil
import subprocess

import numpy as np
import pytest

from conftest import CONFIG
from gplab.ab import binom_p_ge, min_correct, plan_abx, plan_pref, score
from gplab.codegen import generate_header
from gplab.config import load_config
from gplab.features import bar_features, mask_of, swing_pct, syncopation
from gplab.grid import bars_from_beats, best_rotation, events_to_bars, rotate_bar, time_to_pos
from gplab.profile import build_profiles
from gplab.schema import Bar, read_bars, write_bars
from gplab.synth import GRAMMARS, synth_corpus, synth_takes
from gplab.validate import validate

CFG = load_config(CONFIG)


@pytest.fixture(scope="module")
def profiles():
    return build_profiles(synth_corpus(), CFG)


def test_grid_maps_times_to_steps():
    beats = np.arange(0, 8.01, 0.5)          # 120 bpm
    downbeats = beats[::4]
    bars = bars_from_beats(beats, downbeats)
    assert len(bars) == 4
    bt = bars[0]
    assert time_to_pos(0.0, bt) == 0
    assert time_to_pos(0.125, bt) == pytest.approx(1.0)
    assert time_to_pos(-0.03, bt) < 0          # early hit still in this bar
    assert time_to_pos(1.97, bt) is None       # belongs to the next bar


def test_events_to_bars_roundtrip(tmp_path):
    beats = np.arange(0, 4.01, 0.5)
    ev = {"kick": [(0.0, 1), (0.5, 1), (1.0, 1), (1.5, 1)],
          "bass": [(0.25, .8, 36, 0.25)]}
    bars = events_to_bars(ev, beats, beats[::4], src="t", variant="house.base")
    assert mask_of(bars[0].role("kick")) == 0x1111
    assert bars[0].role("bass")[0][3] == pytest.approx(2.0)   # 0.25 s = 2 steps
    write_bars(tmp_path / "b.jsonl", bars)
    back = list(read_bars(tmp_path / "b.jsonl"))
    assert back[0].events == bars[0].events


def test_rotation_recovers_wrong_downbeat():
    b = Bar("s", "house.base", {"kick": [[s, 1] for s in (0, 4, 8, 12)],
                                "snare": [[4, 1], [12, 1]]})
    shifted = rotate_bar(b, -4)               # tracker put the downbeat one beat late
    assert mask_of(shifted.role("snare")) == (1 << 0) | (1 << 8)
    r = best_rotation([shifted] * 4, CFG["anchors"])
    assert mask_of(rotate_bar(shifted, r).role("snare")) == (1 << 4) | (1 << 12)


def test_features():
    assert syncopation(0x1111) == 0               # four-on-floor is not syncopated
    assert syncopation(1 << 3) > 0                # lone offbeat 16th is
    assert swing_pct([0.16, 0.16]) == pytest.approx(58.0)


def test_profiles_recover_known_grammars(profiles):
    for v, g in GRAMMARS.items():
        p = profiles[v]
        assert abs(p.swing[0] - g["swing"]) < 1.0, v
        for s in (0, 4, 8, 12):
            assert p.step_prob["kick"][s] > 0.85, (v, s)
    # deep house bass lives on 3/10, acid bass everywhere
    assert profiles["house.deep"].step_prob["bass"][3] > 0.4
    assert profiles["house.deep"].step_prob["bass"][4] < 0.1
    assert np.mean(profiles["house.acid"].step_prob["bass"]) > 0.5


def test_validator_accepts_good_and_catches_audit_bug(profiles):
    assert validate(synth_takes("good"), profiles, CFG)["pass"] is True
    stuck = validate(synth_takes("stuck"), profiles, CFG)
    assert stuck["pass"] is False
    for v in GRAMMARS:                      # the 2026-11-10 audit failure mode
        assert stuck["variants"][v]["checks"]["diversity"]["pass"] is False
    wrong = validate(synth_takes("wrong"), profiles, CFG)
    assert wrong["variants"]["house.deep"]["checks"]["accuracy"]["pass"] is False


def test_codegen_compiles(profiles, tmp_path):
    hdr = tmp_path / "p.h"
    hdr.write_text(generate_header(profiles, {"corpus": "test"}))
    if not shutil.which("g++"):
        pytest.skip("g++ not available")
    src = tmp_path / "t.cpp"
    src.write_text('#include "p.h"\nint main(){return gp::genre_lab::kVariantCount==3?0:1;}\n')
    subprocess.run(["g++", "-std=c++17", "-Wall", "-Wextra", "-Werror", str(src),
                    "-o", str(tmp_path / "t")], check=True)
    assert subprocess.run([str(tmp_path / "t")]).returncode == 0


def test_binomial():
    assert binom_p_ge(16, 16) == pytest.approx(0.5 ** 16)
    assert min_correct(16) == 12


def _items(d, variants, n=3):
    d.mkdir()
    for v in variants:
        for i in range(n):
            (d / f"{v}__{i}.wav").write_bytes(f"{v}{i}".encode())


def test_abx_plan_and_score(tmp_path):
    _items(tmp_path / "items", ["house.base", "house.deep"])
    info = plan_abx(tmp_path / "items", tmp_path / "plan", trials_per_pair=16, seed=3)
    assert info["trials"] == 16
    import json
    key = json.loads((tmp_path / "plan" / "_key" / "key.json").read_text())
    # a perfect listener: answer from the key
    rows = ["trial,answer,note"] + [f"{t},{k['truth']}," for t, k in key["trials"].items()]
    (tmp_path / "ans.csv").write_text("\n".join(rows))
    res = score(tmp_path / "plan", tmp_path / "ans.csv")
    g = res["groups"]["house.base vs house.deep"]
    assert g["hits"] == 16 and g["significant"]
    with pytest.raises(FileExistsError):
        plan_abx(tmp_path / "items", tmp_path / "plan")


def test_pref_plan(tmp_path):
    _items(tmp_path / "new", ["house.deep"])
    _items(tmp_path / "old", ["house.deep"])
    info = plan_pref(tmp_path / "new", tmp_path / "old", tmp_path / "plan", 6, seed=2)
    assert info["trials"] == 6
    res = score(tmp_path / "plan")           # nothing answered yet
    assert res["answered"] == 0
