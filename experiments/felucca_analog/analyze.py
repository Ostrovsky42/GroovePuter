#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# DSP-B analysis, standard library only (no numpy in this environment). Reads the 32-bit float WAVs the driver writes.
#   analyze.py sustain <dir> <fs> <midi> [bright]   pitch error, harmonic slope, inharmonic share, attack time
#   analyze.py melody  <dir> <fs>          level, clicks at note edges, matched copies (RMS = SH101's)
import cmath
import math
import os
import struct
import sys

ENGINES = ["FELUCCA_ANALOG", "SH101", "WAVEMORPH"]


def read_wav(path):
    with open(path, "rb") as f:
        data = f.read()
    assert data[:4] == b"RIFF" and data[8:12] == b"WAVE"
    pos = 12
    fs = None
    while pos < len(data):
        cid = data[pos:pos + 4]
        size = struct.unpack("<I", data[pos + 4:pos + 8])[0]
        body = data[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fs = struct.unpack("<I", body[4:8])[0]
        elif cid == b"data":
            n = size // 4
            return fs, list(struct.unpack("<%df" % n, body[:n * 4]))
        pos += 8 + size + (size & 1)
    raise ValueError(path)


def write_wav(path, fs, x):
    n = len(x)
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + n * 4) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 3, 1, fs, fs * 4, 4, 32))
        f.write(b"data" + struct.pack("<I", n * 4))
        f.write(struct.pack("<%df" % n, *x))


def fft(a):
    n = len(a)
    if n == 1:
        return a
    ev = fft(a[0::2])
    od = fft(a[1::2])
    out = [0j] * n
    for k in range(n // 2):
        t = cmath.exp(-2j * math.pi * k / n) * od[k]
        out[k] = ev[k] + t
        out[k + n // 2] = ev[k] - t
    return out


def rms(x):
    return math.sqrt(sum(v * v for v in x) / len(x)) if x else 0.0


def spectral_freq(x, fs, a, b, N=8192, fmin=40.0):
    """strongest spectral peak in 40 Hz .. 0.45 fs, parabolic interpolation. (Zero crossings were tried first: a
    waveform with a strong 2nd harmonic crosses zero twice a period and reads an octave high.)"""
    seg = x[a:a + N] if b - a >= N else x[a:b]
    n = len(seg)
    seg = [seg[i] * (0.5 - 0.5 * math.cos(2 * math.pi * i / n)) for i in range(n)]
    N2 = 1 << (n - 1).bit_length()
    seg += [0.0] * (N2 - n)
    mag = [abs(c) for c in fft(seg)[: N2 // 2]]
    hz = fs / N2
    lo, hi = int(fmin / hz), int(0.45 * fs / hz)
    k = max(range(lo, hi), key=lambda j: mag[j])
    alpha, beta, gamma = mag[k - 1], mag[k], mag[k + 1]
    delta = 0.5 * (alpha - gamma) / (alpha - 2 * beta + gamma)
    return (k + delta) * hz


def sustain(d, fs, midi, bright):
    f0 = 440.0 * 2 ** ((midi - 69) / 12)
    tag = "_bright" if bright else ""
    print(f"sustained note midi {midi:g} = {f0:.2f} Hz at fs {fs}{' (filter open)' if bright else ''}; window 0.40 .. 0.90 s, pitch by spectral peak")
    print("| engine | measured Hz | error cents | attack 10..90 % (ms) | harmonic slope 2..8 (dB/oct) | inharmonic share (%) |")
    print("|---|---|---|---|---|---|")
    for e in ENGINES:
        _, x = read_wav(os.path.join(d, f"sustain_{e}_{fs}_m{int(midi)}{tag}.wav"))
        a, b = int(0.40 * fs), int(0.90 * fs)
        f = spectral_freq(x, fs, a, b)
        cents = 1200 * math.log2(f / f0) if f == f == f else float("nan")
        # attack: 5 ms RMS envelope, time from 10 % to 90 % of its peak in the first 0.4 s
        win = max(1, int(0.005 * fs))
        env = []
        for i in range(0, int(0.4 * fs) - win, win // 2):
            env.append(rms(x[i:i + win]))
        pk = max(env)
        t10 = next(i for i, v in enumerate(env) if v >= 0.1 * pk)
        t90 = next(i for i, v in enumerate(env) if v >= 0.9 * pk)
        attack_ms = (t90 - t10) * (win // 2) / fs * 1000
        # spectrum: Hann window of 4096 at 0.5 s
        N = 4096
        s0 = int(0.5 * fs)
        seg = [x[s0 + i] * (0.5 - 0.5 * math.cos(2 * math.pi * i / N)) for i in range(N)]
        mag = [abs(c) for c in fft(seg)[: N // 2]]
        bin_hz = fs / N
        def band_peak(hz):
            k = int(round(hz / bin_hz))
            return max(mag[max(0, k - 2): k + 3])
        harm = []
        h = 1
        while h * f0 < fs / 2 * 0.95 and h <= 8:
            harm.append(band_peak(h * f0))
            h += 1
        slope = float("nan")
        if len(harm) >= 3:
            # least squares of dB against log2(harmonic number) for harmonics 2..n
            pts = [(math.log2(k + 1), 20 * math.log10(max(v, 1e-12))) for k, v in enumerate(harm) if k >= 1]
            mx = sum(p[0] for p in pts) / len(pts)
            my = sum(p[1] for p in pts) / len(pts)
            den = sum((p[0] - mx) ** 2 for p in pts)
            slope = sum((p[0] - mx) * (p[1] - my) for p in pts) / den if den else float("nan")
        # inharmonic share: energy outside +-3 bins of any harmonic of f0 (alias and noise proxy)
        total = sum(v * v for v in mag[1:])
        near = [False] * len(mag)
        for k in range(1, int(fs / 2 / f0) + 1):
            c = int(round(k * f0 / bin_hz))
            for j in range(max(0, c - 3), min(len(mag), c + 4)):
                near[j] = True
        inh = sum(mag[j] ** 2 for j in range(1, len(mag)) if not near[j])
        share = 100.0 * inh / total if total else float("nan")
        print(f"| {e} | {f:.2f} | {cents:+.2f} | {attack_ms:.1f} | {slope:+.1f} | {share:.2f} |")


def melody(d, fs):
    print(f"melody at fs {fs}: level, clicks, matched copies")
    print("| engine | RMS | peak | crest (dB) | max step / peak | matched gain to SH101 RMS (dB) |")
    print("|---|---|---|---|---|---|")
    data = {}
    for e in ENGINES:
        _, x = read_wav(os.path.join(d, f"melody_{e}_{fs}_p0.wav"))
        data[e] = x
    if fs == 22050 and os.path.exists(os.path.join(d, "melody_FELUCCA_ANALOG_22050_p0_dec.wav")):
        _, data["FELUCCA_DEC44"] = read_wav(os.path.join(d, "melody_FELUCCA_ANALOG_22050_p0_dec.wav"))
    ref = rms(data["SH101"])
    for e in data:
        x = data[e]
        r = rms(x)
        pk = max(abs(v) for v in x)
        step = max(abs(x[i] - x[i - 1]) for i in range(1, len(x)))
        crest = 20 * math.log10(pk / r) if r else float("nan")
        gain = 20 * math.log10(ref / r) if r else float("nan")
        print(f"| {e} | {r:.4f} | {pk:.3f} | {crest:.1f} | {step / pk:.3f} | {gain:+.2f} |")
        g = ref / r
        write_wav(os.path.join(d, f"matched_{e}_{fs}_p0.wav"), fs, [v * g for v in x])
    print("matched copies written as matched_<engine>_<fs>.wav (RMS equal to SH101's; listening only)")


def sweep(d, fs, lo, hi, bright_tag, preset, suffix=""):
    """Felucca only: pitch error and inharmonic share per MIDI note. A note above the playable top (0.45 FS) is
    expected at the top note: the adapter holds it there and counts it, so its error is measured against that."""
    top = min(127, math.floor(69 + 12 * math.log2(0.45 * fs / 440.0)))
    print(f"sweep fs {fs}{' (filter open)' if bright_tag else ''}, preset {preset}, playable top MIDI {top}")
    print("| MIDI | expected Hz | measured Hz | error cents | inharmonic share (%) | note |")
    print("|---|---|---|---|---|---|")
    worst = 0.0
    shares = []
    for m in range(lo, hi + 1):
        path = os.path.join(d, f"sweep_{fs}_m{m}{bright_tag}_p{preset}{suffix}.wav")
        _, x = read_wav(path)
        a = int(0.1 * fs)
        b = a + 8192
        eff = min(m, top)
        f0 = 440.0 * 2 ** ((eff - 69) / 12)
        f = spectral_freq(x, fs, a, b, N=8192, fmin=20.0)
        cents = 1200 * math.log2(f / f0)
        N = 8192
        seg = [x[a + i] * (0.5 - 0.5 * math.cos(2 * math.pi * i / N)) for i in range(N)]
        mag = [abs(c) for c in fft(seg)[: N // 2]]
        hz = fs / N
        near = [False] * len(mag)
        for k in range(1, int(0.45 * fs / f0) + 1):
            c = int(round(k * f0 / hz))
            for j in range(max(0, c - 3), min(len(mag), c + 4)):
                near[j] = True
        total = sum(v * v for v in mag[1:])
        share = 100.0 * sum(mag[j] ** 2 for j in range(1, len(mag)) if not near[j]) / total if total else float("nan")
        worst = max(worst, abs(cents))
        shares.append(share)
        note = "held at top (clamped)" if m > top else ""
        print(f"| {m} | {f0:.1f} | {f:.1f} | {cents:+.2f} | {share:.3f} | {note} |")
    print(f"\nworst |error| {worst:.2f} cents")
    print(f"mean inharmonic share {sum(shares) / len(shares):.3f} %")


def kaiser_fir(ntaps, fc_hz, fs_in, beta):
    """linear-phase lowpass: Kaiser-windowed sinc, normalised to unity DC gain. ntaps odd."""
    M = (ntaps - 1) / 2.0
    wc = 2 * math.pi * fc_hz / fs_in
    def i0(x):
        t, s_ = 1.0, 1.0
        for k in range(1, 60):
            t *= (x / (2 * k)) ** 2
            s_ += t
        return s_
    h = []
    for n in range(ntaps):
        t = n - M
        sinc = wc / math.pi if t == 0 else math.sin(wc * t) / (math.pi * t)
        w = i0(beta * math.sqrt(max(0.0, 1 - (t / M) ** 2))) / i0(beta)
        h.append(sinc * w)
    g = sum(h)
    return [v / g for v in h]


def decimate2(x, h):
    """2:1 decimation: the FIR runs at the input rate, only every second output is computed (polyphase by skipping)."""
    L = len(h)
    pad = [0.0] * (L - 1) + x
    return [sum(h[k] * pad[2 * n + L - 1 - k] for k in range(L)) for n in range(len(x) // 2)]


def fir_report(h, fs_in):
    """passband ripple up to 9.9 kHz and stopband attenuation from 11.025 kHz (the output's Nyquist)."""
    def resp(f):
        w = 2 * math.pi * f / fs_in
        re = sum(h[k] * math.cos(w * k) for k in range(len(h)))
        im = -sum(h[k] * math.sin(w * k) for k in range(len(h)))
        return math.hypot(re, im)
    pb = [20 * math.log10(max(resp(f), 1e-12)) for f in range(20, 9901, 50)]
    sb = max(20 * math.log10(max(resp(f), 1e-12)) for f in range(11025, fs_in // 2 + 1, 50))
    return max(abs(v) for v in pb), sb


def decimate(in_path, out_path, ntaps=145, fc=10474.0, beta=5.65):
    fs_in, x = read_wav(in_path)
    h = kaiser_fir(ntaps, fc, fs_in, beta)
    y = decimate2(x, h)
    write_wav(out_path, fs_in // 2, y)
    ripple, stop = fir_report(h, fs_in)
    print(f"{os.path.basename(out_path)}: {ntaps} taps, fc {fc:.0f} Hz at {fs_in} Hz, passband ripple <= {ripple:.3f} dB (20 Hz .. 9.9 kHz), "
          f"stopband max {stop:.1f} dB (>= 11.025 kHz); {ntaps} MAC per output sample")


def levels(d, fs):
    """RMS and peak of each engine's held note, per preset and MIDI note, and the Felucca offset to SH101 (dB)."""
    names = ["INIT", "DUAL SAW", "DEEP BASS", "ACID LEAD"]
    print(f"levels at fs {fs} (raw engine output, gain 65536 unless noted; RMS over 1 s held note)")
    print("| preset | MIDI | Felucca RMS | SH101 RMS | Felucca/SH101 (dB) | Felucca peak |")
    print("|---|---|---|---|---|---|")
    means = {}
    for p in range(4):
        dbs = []
        for m in (36, 48, 60, 72, 84):
            fr, fp = _rms_peak(os.path.join(d, f"sustain_FELUCCA_ANALOG_{fs}_m{m}_p{p}.wav"))
            sr, _ = _rms_peak(os.path.join(d, f"sustain_SH101_{fs}_m{m}_p{p}.wav"))
            db = 20 * math.log10(fr / sr)
            dbs.append(db)
            print(f"| {names[p]} | {m} | {fr:.3f} | {sr:.3f} | {db:+.1f} | {fp:.3f} |")
        means[names[p]] = sum(dbs) / len(dbs)
    print()
    for n in names:
        print(f"{n}: mean {means[n]:+.1f} dB vs SH101")
    print(f"mean over presets: {sum(means.values()) / len(means):+.1f} dB -> gain {10 ** (-sum(means.values()) / len(means) / 20):.3f}")


def _rms_peak(path):
    _, x = read_wav(path)
    return rms(x), max(abs(v) for v in x)


if __name__ == "__main__":
    mode, d = sys.argv[1], sys.argv[2]
    fs = int(sys.argv[3]) if len(sys.argv) > 3 and sys.argv[3].isdigit() else 0
    if mode == "sustain":
        sustain(d, fs, float(sys.argv[4]), len(sys.argv) > 5 and sys.argv[5] == "bright")
    elif mode == "sweep":         # analyze.py sweep <dir> <fs> <lo> <hi> [bright] [preset]
        lo, hi = int(sys.argv[4]), int(sys.argv[5])
        bright_tag = "_bright" if "bright" in sys.argv[6:] else ""
        preset = 0
        for a in sys.argv[6:]:
            if a.startswith("preset="):
                preset = int(a[7:])
        suffix = "_dec" if "dec" in sys.argv[6:] else ""
        sweep(d, fs, lo, hi, bright_tag, preset, suffix)
    elif mode == "levels":        # analyze.py levels <dir> <fs> : table from sustain files named *_p<N>.wav made by run_all.sh
        levels(d, fs)
    elif mode == "decimate":      # analyze.py decimate <in44.1.wav> <out22.05.wav>
        decimate(d, sys.argv[3])
    elif mode == "melody":
        melody(d, fs)
    elif mode == "harmonics":     # analyze.py harmonics <dir> <fs> <file-stem>: h2..h8 relative to h1 (dB), one file
        stem = sys.argv[4]
        f0 = float(sys.argv[5])
        _, x = read_wav(os.path.join(d, stem + ".wav"))
        N = 4096
        s0 = int(0.42 * fs)
        seg = [x[s0 + i] * (0.5 - 0.5 * math.cos(2 * math.pi * i / N)) for i in range(N)]
        mag = [abs(c) for c in fft(seg)[: N // 2]]
        hz = fs / N

        def pk(f):
            k = int(round(f / hz))
            return max(mag[k - 2:k + 3])

        h1 = pk(f0)
        print(stem, " ".join("h%d %+.1f" % (h, 20 * math.log10(max(pk(h * f0), 1e-12) / h1))
                             for h in range(2, 9) if h * f0 < fs / 2))
    else:
        sys.exit(__doc__)
