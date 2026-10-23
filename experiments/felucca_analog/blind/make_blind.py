#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# DSP-B blind listening package. Takes the rendered stimuli (run_blind.sh) and writes:
#   <pack>/      WAV files named by random ids only, 16-bit PCM 22050 Hz mono, peak <= -1 dBFS. No key here.
#   <key>/       answer_key.json (id -> material, engine), the loudness table, and the random seed.
# Loudness: each material's four stimuli get one common gain so their RMS match (the same gain for all four, so
# the one with the highest peak sets the level). RMS is not loudness and not timbre: listening decides.
#   make_blind.py <stim_dir> <pack_dir> <key_dir> <seed>
import json
import math
import os
import random
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import analyze as A  # noqa: E402

MATERIALS = {
    "bass": ["FELUCCA_NATIVE_22050", "FELUCCA_RESAMPLED_44100", "SH101", "WAVEMORPH"],
    "lead": ["FELUCCA_NATIVE_22050", "FELUCCA_RESAMPLED_44100", "SH101", "WAVEMORPH"],
    "high": ["FELUCCA_NATIVE_22050", "FELUCCA_RESAMPLED_44100", "SH101", "WAVEMORPH"],
}
PEAK_CEILING = 10 ** (-1 / 20)     # -1 dBFS


def write_pcm16(path, fs, x):
    with open(path, "wb") as f:
        n = len(x)
        f.write(b"RIFF" + struct.pack("<I", 36 + 2 * n) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 1, 1, fs, fs * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", 2 * n))
        f.write(struct.pack("<%dh" % n, *[max(-32767, min(32767, int(round(v * 32767)))) for v in x]))


def main(stim, pack, key, seed):
    os.makedirs(pack, exist_ok=True)
    os.makedirs(key, exist_ok=True)
    rng = random.Random(seed)
    answers = {}
    table = []
    ids = set()
    for mat, engines in MATERIALS.items():
        files = {e: os.path.join(stim, f"{mat}_{e}.wav") for e in engines}
        data = {}
        for e, p in files.items():
            fs, x = A.read_wav(p)
            data[e] = (fs, x)
        # one gain for the four: the RMS match, limited so the loudest one stays under the -1 dBFS ceiling
        rms = {e: A.rms(x) for e, (_, x) in data.items()}
        peak = {e: max(abs(v) for v in x) for e, (_, x) in data.items()}
        target = min(rms[e] * PEAK_CEILING / peak[e] for e in engines)
        for e in engines:
            fs, x = data[e]
            g = target / rms[e]
            y = [v * g for v in x]
            ident = None
            while ident is None or ident in ids:
                ident = "%04x" % rng.randrange(0x10000)
            ids.add(ident)
            write_pcm16(os.path.join(pack, f"{ident}.wav"), fs, y)
            answers[ident] = {"material": mat, "engine": e}
            table.append({"material": mat, "engine": e, "rms_before": rms[e], "peak_before": peak[e],
                          "gain_db": 20 * math.log10(g), "peak_after": max(abs(v) for v in y)})
    rng2 = random.Random(seed + 1)
    order = sorted(answers, key=lambda k: rng2.random())
    with open(os.path.join(key, "answer_key.json"), "w") as f:
        json.dump({"seed": seed, "answers": answers, "playback_order_hint": order}, f, indent=1)
    with open(os.path.join(key, "loudness.md"), "w") as f:
        f.write("| material | engine | RMS before | peak before | gain (dB) | peak after |\n|---|---|---|---|---|---|\n")
        for r in table:
            f.write(f"| {r['material']} | {r['engine']} | {r['rms_before']:.4f} | {r['peak_before']:.3f} | "
                    f"{r['gain_db']:+.2f} | {r['peak_after']:.3f} |\n")
    print(f"pack: {len(answers)} files in {pack}; key in {key}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4]))
