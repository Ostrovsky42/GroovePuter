#!/usr/bin/env python3
"""DSP-A deterministic metrics using the stdlib (no host DSP dependency install)."""
import argparse
import array
import cmath
import csv
import hashlib
import json
import math
from pathlib import Path


def fft(data):
    n = len(data)
    a = [complex(x) for x in data]
    j = 0
    for i in range(1, n):
        bit = n >> 1
        while j & bit:
            j ^= bit
            bit >>= 1
        j ^= bit
        if i < j:
            a[i], a[j] = a[j], a[i]
    width = 2
    while width <= n:
        root = cmath.exp(-2j * math.pi / width)
        for start in range(0, n, width):
            w = 1+0j
            for i in range(width//2):
                left, right = a[start+i], w*a[start+i+width//2]
                a[start+i], a[start+i+width//2] = left+right, left-right
                w *= root
        width *= 2
    return a


def spectral(samples, sr):
    n = 4096
    # A fixed window gives comparable spectra; sequence cases contain changing notes.
    segment = samples[int(.2*sr):int(.2*sr)+n]
    mean = sum(segment)/n
    window = [(x-mean)*(.5-.5*math.cos(2*math.pi*i/(n-1))) for i,x in enumerate(segment)]
    spectrum = fft(window)
    power = [abs(x)**2 for x in spectrum[:n//2+1]]
    power[0] = 0
    total = sum(power)
    dominant = max(range(1,len(power)), key=power.__getitem__)
    return dict(fft_size=n, resolution_hz=sr/n, dominant_hz=dominant*sr/n,
                power_centroid_hz=sum(i*sr/n*x for i,x in enumerate(power))/total if total else 0,
                high_band_fraction=sum(power[int(.35*n):])/total if total else 0)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('render_dir', type=Path)
    p.add_argument('--output', type=Path)
    args = p.parse_args()
    result = dict(protocol='dry voice, mono 22050 Hz; common event schedule; no output-chain model', cases={})
    with (args.render_dir/'metrics.csv').open() as f:
        for row in csv.DictReader(f):
            name, sr = row['case'], int(row['sample_rate'])
            data = array.array('f')
            data.frombytes((args.render_dir/(name+'.f32')).read_bytes())
            assert all(math.isfinite(x) for x in data)
            off = int(row['last_note_off_frame'])
            last = next((i for i in range(len(data)-1, off-1, -1) if abs(data[i])>1e-5),off)
            sustain = data[int(.15*sr):int(.45*sr)]
            rms = math.sqrt(sum(x*x for x in sustain)/len(sustain))
            entry = {k:(v if k in ('case','engine') else float(v)) for k,v in row.items()}
            entry.update(sustain_rms=rms, sustain_dbfs=20*math.log10(max(1e-12,rms)),
                         dc_mean=sum(sustain)/len(sustain),
                         release_below_minus100db_ms=1000*(last-off)/sr,
                         max_adjacent_step=max(abs(data[i]-data[i-1]) for i in range(1,len(data))),
                         wav_sha256=hashlib.sha256((args.render_dir/(name+'.wav')).read_bytes()).hexdigest(),
                         f32_sha256=hashlib.sha256(data.tobytes()).hexdigest(),
                         **spectral(data,sr))
            result['cases'][name] = entry
    output = args.output or args.render_dir/'analysis.json'
    output.write_text(json.dumps(result,indent=2)+'\n')
    print(f'DSP-A analysis: {len(result["cases"])} cases -> {output}')


if __name__=='__main__':
    main()
