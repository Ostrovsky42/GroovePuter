"""Tiny numpy DSP helpers shared by fallback transcribers."""
from __future__ import annotations

import numpy as np


def stft_mag(y: np.ndarray, n_fft: int = 1024, hop: int = 256) -> np.ndarray:
    """Left-padded by n_fft zeros so a hit at t=0 still produces a rise.
    Use frame_time() to convert a frame index back to seconds."""
    y = np.concatenate([np.zeros(n_fft, dtype=y.dtype), y, np.zeros(n_fft, dtype=y.dtype)])
    win = np.hanning(n_fft).astype(np.float32)
    n = 1 + (len(y) - n_fft) // hop
    idx = np.arange(n_fft)[None, :] + hop * np.arange(n)[:, None]
    return np.abs(np.fft.rfft(y[idx] * win, axis=1))   # [frames, bins]


def frame_time(i: int, sr: int, n_fft: int, hop: int) -> float:
    """Centre of frame i in seconds of the ORIGINAL (unpadded) signal."""
    return (i * hop - n_fft / 2) / sr


def band_db(mag: np.ndarray, sr: int, n_fft: int, lo: float, hi: float) -> np.ndarray:
    freqs = np.fft.rfftfreq(n_fft, 1 / sr)
    sel = (freqs >= lo) & (freqs < hi)
    e = (mag[:, sel] ** 2).sum(1)
    return 10 * np.log10(e + 1e-10)


def pick_peaks(odf: np.ndarray, thr: float, min_gap: int) -> list[int]:
    """Local maxima of an onset function above thr, at least min_gap frames apart."""
    peaks, last = [], -10 ** 9
    for i in range(1, len(odf) - 1):
        if odf[i] >= thr and odf[i] >= odf[i - 1] and odf[i] > odf[i + 1]:
            if i - last >= min_gap:
                peaks.append(i)
                last = i
            elif odf[i] > odf[peaks[-1]]:
                peaks[-1] = i
                last = i
    return peaks


def rise_db(db: np.ndarray, i: int, back: int, fwd: int) -> float:
    lo = db[max(0, i - back):i + 1].min()
    hi = db[i:min(len(db), i + fwd + 1)].max()
    return float(hi - lo)
