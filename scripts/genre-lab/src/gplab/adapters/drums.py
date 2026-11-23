"""Drum transcription -> {role: [(time_s, vel)]}.

bands  numpy fallback for a SEPARATED drum stem of electronic music. Known limits:
       a hat hidden under a snare hit is usually lost; ride/crash -> ohat or missed.
       Good enough for step-probability statistics, not for exact transcription.
midi   read any ADT tool's MIDI output (ADTOF recommended). GM pitch map below.
"""
from __future__ import annotations

import numpy as np

from .dsp import band_db, frame_time, pick_peaks, rise_db, stft_mag

GM_MAP = {35: "kick", 36: "kick", 37: "snare", 38: "snare", 39: "snare", 40: "snare",
          42: "hat", 44: "hat", 46: "ohat", 49: "ohat", 51: "ohat", 57: "ohat", 59: "ohat"}

BANDS = {"low": (30, 150), "mid": (1000, 4500), "high": (7000, 16000)}


def transcribe_bands(y: np.ndarray, sr: int, *, rise_thr: float = 6.0, range_db: float = 60.0,
                     gate_db: float = 24.0, leak_ms: float = 15.0, leak_drop_db: float = 18.0,
                     open_hat_ms: float = 120.0) -> dict[str, list[tuple[float, float]]]:
    """Band-onset transcriber with three safeguards learned from failures:
    * floor each band at (its 99th pct - range_db): a rise out of digital silence
      in a separated stem is not an event;
    * level gate: an event must reach (band 99th pct - gate_db);
    * kick leak: a kick's click also lights up mid/high bands. On a kick frame,
      mid/high counts as snare/hat only if it is still within leak_drop_db of its
      peak leak_ms after the analysis window — a click is gone by then.
    """
    n_fft, hop = 1024, 256
    mag = stft_mag(y, n_fft, hop)
    fps = sr / hop
    raw = {k: band_db(mag, sr, n_fft, lo, min(hi, sr / 2 - 1)) for k, (lo, hi) in BANDS.items()}
    ref = {k: float(np.percentile(v, 99)) for k, v in raw.items()}
    db = {k: np.maximum(v, ref[k] - range_db) for k, v in raw.items()}
    odf = {k: np.maximum(0, np.diff(v, prepend=v[0])) for k, v in db.items()}
    gap = max(1, int(0.06 * fps))
    back = fwd = max(1, int(0.03 * fps))
    t = lambda i: max(0.0, frame_time(i, sr, n_fft, hop))
    vel = lambda r: round(min(1.0, r / 30.0), 3)
    level = lambda k, i: float(db[k][i:i + fwd + 1].max())

    def onsets(band):
        return [i for i in pick_peaks(odf[band], rise_thr / 2, gap)
                if rise_db(db[band], i, back, fwd) >= rise_thr
                and level(band, i) >= ref[band] - gate_db]

    out: dict[str, list[tuple[float, float]]] = {"kick": [], "snare": [], "hat": [], "ohat": []}
    kicks = onsets("low")
    for i in kicks:
        out["kick"].append((t(i), vel(rise_db(db["low"], i, back, fwd))))

    sustain = max(2, int(leak_ms / 1000 * fps))

    def is_kick_leak(band, i):
        """On a kick frame, a click dies within a few ms; a real snare/hat rings on.
        (An earlier ratio-to-kick-baseline rule dropped every hat that always
        coincides with the kick — e.g. hats on all 8ths — so it was replaced.)"""
        if not any(abs(k - i) <= back for k in kicks):
            return False
        j = i + fwd + sustain
        return j >= len(db[band]) or db[band][j] < level(band, i) - leak_drop_db

    snares = []
    for i in onsets("mid"):
        rm, rh = rise_db(db["mid"], i, back, fwd), rise_db(db["high"], i, back, fwd)
        if rm >= rh - 3.0 or level("mid", i) - ref["mid"] > level("high", i) - ref["high"]:
            if not is_kick_leak("mid", i):
                snares.append(i)
                out["snare"].append((t(i), vel(rm)))

    hold = int(open_hat_ms / 1000 * fps)
    for i in onsets("high"):
        rh, rm = rise_db(db["high"], i, back, fwd), rise_db(db["mid"], i, back, fwd)
        if any(abs(i - s) <= back for s in snares) and rh - rm < 6.0:
            continue                         # it is the snare's own noise
        if is_kick_leak("high", i):
            continue
        peak = level("high", i)
        tail = db["high"][i:i + hold + 1]
        role = "ohat" if len(tail) > hold and tail[-1] > peak - 12.0 else "hat"
        out[role].append((t(i), vel(rh)))
    return out


def transcribe_midi(path, pitch_map: dict[int, str] = GM_MAP):
    import mido  # lazy: requirements-ml
    out: dict[str, list[tuple[float, float]]] = {"kick": [], "snare": [], "hat": [], "ohat": []}
    now = 0.0
    for msg in mido.MidiFile(str(path)):
        now += msg.time
        if msg.type == "note_on" and msg.velocity > 0 and msg.note in pitch_map:
            out[pitch_map[msg.note]].append((now, msg.velocity / 127.0))
    return out
