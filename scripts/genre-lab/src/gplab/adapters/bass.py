"""Bass transcription -> [(time_s, vel, midi_pitch, dur_s)], monophonic.

basicpitch  Spotify basic-pitch note events (recommended).
simple      numpy fallback: low-band onsets + autocorrelation pitch. Works on a
            clean separated bass stem; weak on heavily distorted or chorded bass.
"""
from __future__ import annotations

import numpy as np

from .dsp import band_db, frame_time, pick_peaks, rise_db, stft_mag

MAX_BASS_PITCH = 60  # C4: above this it is not the bass line


def monophonize(notes):
    notes = sorted(notes, key=lambda n: (n[0], -n[1]))
    out = []
    for n in notes:
        if out and n[0] - out[-1][0] < 0.03:      # simultaneous: keep louder
            if n[1] > out[-1][1]:
                out[-1] = n
            continue
        if out and out[-1][0] + out[-1][3] > n[0]:  # overlap: cut previous
            p = out[-1]
            out[-1] = (p[0], p[1], p[2], max(0.01, n[0] - p[0]))
        out.append(n)
    return out


def transcribe_basicpitch(audio_path):
    from basic_pitch.inference import predict  # lazy: requirements-ml
    _, _, events = predict(str(audio_path))
    notes = [(float(s), float(a), int(p), float(e - s))
             for s, e, p, a, *_ in events if p <= MAX_BASS_PITCH]
    return monophonize(notes)


def _pitch_autocorr(seg: np.ndarray, sr: int, fmin=30.0, fmax=330.0) -> float | None:
    seg = seg - seg.mean()
    if np.sqrt((seg ** 2).mean()) < 1e-3:
        return None
    ac = np.correlate(seg, seg, "full")[len(seg) - 1:]
    lo, hi = int(sr / fmax), min(len(ac) - 1, int(sr / fmin))
    if hi <= lo:
        return None
    lag = lo + int(np.argmax(ac[lo:hi]))
    if ac[lag] < 0.3 * ac[0]:
        return None
    f0 = sr / lag
    return 69 + 12 * np.log2(f0 / 440.0)


def transcribe_simple(y: np.ndarray, sr: int, rise_thr: float = 6.0):
    n_fft, hop = 2048, 256
    db = band_db(stft_mag(y, n_fft, hop), sr, n_fft, 30, 400)
    odf = np.maximum(0, np.diff(db, prepend=db[0]))
    fps = sr / hop
    peaks = [i for i in pick_peaks(odf, rise_thr / 2, max(1, int(0.07 * fps)))
             if rise_db(db, i, 4, 4) >= rise_thr]
    notes = []
    for k, i in enumerate(peaks):
        t0 = max(0.0, frame_time(i, sr, n_fft, hop))
        end_i = peaks[k + 1] if k + 1 < len(peaks) else len(db) - 1
        # note ends when energy falls 18 dB below its peak (or at next onset)
        pk = db[i:i + 6].max()
        j = i + 1
        while j < end_i and db[j] > pk - 18:
            j += 1
        a, b = int((t0 + 0.02) * sr), int((t0 + 0.10) * sr)
        p = _pitch_autocorr(y[a:b], sr)
        if p is None or p > MAX_BASS_PITCH:
            continue
        notes.append((t0, round(min(1.0, rise_db(db, i, 4, 4) / 30), 3), round(p), (j - i) * hop / sr))
    return monophonize(notes)
