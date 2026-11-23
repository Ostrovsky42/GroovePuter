"""Smoke test for the numpy fallback transcribers on rendered drums/bass.

Real music is messier; this proves the plumbing (onsets -> grid -> masks) is
correct, not that the fallback matches ADTOF on real records.
"""
import numpy as np

from gplab.adapters.bass import transcribe_simple
from gplab.adapters.drums import transcribe_bands
from gplab.features import mask_of
from gplab.grid import events_to_bars

SR = 44100
STEP = 0.125            # 120 bpm, 16ths
BARS = 6
rng = np.random.default_rng(0)


def band_noise(n, lo, hi):
    spec = np.fft.rfft(rng.standard_normal(n))
    f = np.fft.rfftfreq(n, 1 / SR)
    spec[(f < lo) | (f > hi)] = 0
    y = np.fft.irfft(spec, n)
    return y / (np.abs(y).max() + 1e-9)


def env(n, tau):
    return np.exp(-np.arange(n) / SR / tau)


def kick():
    n = int(0.3 * SR)
    t = np.arange(n) / SR
    f = 50 + 100 * np.exp(-t / 0.03)
    return np.sin(2 * np.pi * np.cumsum(f) / SR) * env(n, 0.12)


def snare():
    n = int(0.2 * SR)
    t = np.arange(n) / SR
    return 0.7 * band_noise(n, 900, 6000) * env(n, 0.06) + 0.3 * np.sin(2 * np.pi * 200 * t) * env(n, 0.04)


def hat(tau):
    n = int(0.4 * SR)
    return 0.5 * band_noise(n, 7500, 18000) * env(n, tau)


def render(pattern):
    y = np.zeros(int((BARS * 16 + 8) * STEP * SR))
    for b in range(BARS):
        for name, steps in pattern.items():
            for s in steps:
                x = {"kick": kick, "snare": snare, "hat": lambda: hat(0.02),
                     "ohat": lambda: hat(0.25)}[name]()
                i = int(((b * 16 + s) * STEP) * SR)
                y[i:i + len(x)] += x[:len(y) - i]
    return y / np.abs(y).max() * 0.9


def grid():
    beats = np.arange(0, BARS * 2 + 2.01, 0.5)
    return beats, beats[::4]


def test_bands_transcriber_recovers_pattern():
    pat = {"kick": [0, 4, 8, 12], "snare": [4, 12], "hat": [2, 6, 10], "ohat": [14]}
    ev = transcribe_bands(render(pat), SR)
    beats, downs = grid()
    bars = events_to_bars(ev, beats, downs, src="t", variant="house.base")[:BARS]
    assert len(bars) == BARS
    for b in bars:
        for role, steps in pat.items():
            assert mask_of(b.role(role)) == sum(1 << s for s in steps), (role, b.bar_idx)


def test_hats_on_every_eighth_survive_kick_leak():
    """Regression: hats that ALWAYS coincide with kicks must not be filtered as kick clicks.
    A hat hidden under a snare may be lost (documented limit), so steps 4/12 are optional."""
    pat = {"kick": [0, 4, 8, 12], "snare": [4, 12], "hat": [0, 2, 4, 6, 8, 10, 12, 14]}
    y = render(pat)
    y = y + 0.001 * rng.standard_normal(len(y))        # not digital silence
    ev = transcribe_bands(y, SR)
    beats, downs = grid()
    for b in events_to_bars(ev, beats, downs, src="t", variant="house.base")[:BARS]:
        assert mask_of(b.role("kick")) == 0x1111
        assert mask_of(b.role("snare")) == (1 << 4) | (1 << 12)
        got = mask_of(b.role("hat")) | mask_of(b.role("ohat"))
        must = sum(1 << s for s in (0, 2, 6, 8, 10, 14))
        assert got & must == must, (b.bar_idx, bin(got))
        assert got & ~0x5555 == 0, (b.bar_idx, bin(got))   # no hats off the 8ths


def test_simple_bass_recovers_notes():
    notes = [(2, 36), (6, 43), (10, 36), (14, 48)]
    y = np.zeros(int((BARS * 16 + 8) * STEP * SR))
    for b in range(BARS):
        for s, p in notes:
            n = int(1.5 * STEP * SR)
            t = np.arange(n) / SR
            f0 = 440 * 2 ** ((p - 69) / 12)
            x = (np.sin(2 * np.pi * f0 * t) + 0.4 * np.sin(4 * np.pi * f0 * t)) * env(n, 0.15)
            x[-200:] *= np.linspace(1, 0, 200)
            i = int((b * 16 + s) * STEP * SR)
            y[i:i + n] += x
    ev = {"bass": transcribe_simple(y * 0.5, SR)}
    beats, downs = grid()
    bars = events_to_bars(ev, beats, downs, src="t", variant="house.base")[:BARS]
    for b in bars:
        got = [(round(e[0]), int(e[2])) for e in b.role("bass")]
        assert got == notes, (b.bar_idx, got)
