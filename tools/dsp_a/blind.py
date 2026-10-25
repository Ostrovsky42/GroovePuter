#!/usr/bin/env python3
"""Generate peak-safe RMS-matched blind files, keeping unscaled references separately."""
import argparse
import array
import hashlib
import json
import math
from pathlib import Path
import random
import struct
import wave

p=argparse.ArgumentParser()
p.add_argument('baseline',type=Path)
p.add_argument('candidate',type=Path)
p.add_argument('output',type=Path)
p.add_argument('--phase-experiment',type=Path)
p.add_argument('--seed',type=int,default=20261023)
a=p.parse_args()
a.output.mkdir(parents=True,exist_ok=True)
rng=random.Random(a.seed)
pairs=[('TB303-sequence','Acid sequence'),('TB303-f0-accent-extreme','Accented LP1 envelope'),('WAVEMORPH-retrigger','Repeated mono note')]
pairs += [(f'{e}-preset{i}',name) for e,names in [('TB303',['Classic Acid','Deep Bass','Aggressive Acid']),('WAVEMORPH',['Warm Pad-like Lead','Digital Bass','Bright Lead'])] for i,name in enumerate(names)]
manifest=dict(protocol='RMS-matched over full identical-duration render, peak <= .9, no limiter; labels randomized; do not open REVEAL until rating',pairs=[])
jobs=[(case,title,[('baseline',a.baseline),('candidate',a.candidate)]) for case,title in pairs]
if a.phase_experiment:
    jobs += [(f'WAVEMORPH-t{table}-ramp',f'Morph transition {table} (isolated experiment)',
              [('candidate',a.candidate),('phase_experiment',a.phase_experiment)]) for table in (0,2)]
for index,(case,title,inputs) in enumerate(jobs,1):
    versions=[]
    for label,root in inputs:
        raw=(root/(case+'.f32')).read_bytes()
        pcm=array.array('f');pcm.frombytes(raw)
        rms=math.sqrt(sum(x*x for x in pcm)/len(pcm))
        peak=max(abs(x) for x in pcm)
        versions.append(dict(version=label,samples=pcm,rms=rms,peak=peak,source_sha256=hashlib.sha256(raw).hexdigest()))
    assert len(versions[0]['samples'])==len(versions[1]['samples'])
    target=min(.12,*[v['rms']*.9/max(v['peak'],1e-12) for v in versions])
    rng.shuffle(versions)
    entry=dict(id=f'{index:02d}',case=case,title=title,target_rms=target,files=[])
    for letter,v in zip('AB',versions):
        gain=target/max(v['rms'],1e-12)
        normalized=[x*gain for x in v['samples']]
        assert max(abs(x) for x in normalized)<=.90001
        filename=f'{index:02d}-{letter}.wav'
        with wave.open(str(a.output/filename),'wb') as f:
            f.setparams((1,2,22050,0,'NONE','not compressed'))
            f.writeframes(b''.join(struct.pack('<h',round(x*32767)) for x in normalized))
        entry['files'].append(dict(file=filename,version=v['version'],gain=gain,source_sha256=v['source_sha256'],wav_sha256=hashlib.sha256((a.output/filename).read_bytes()).hexdigest()))
    manifest['pairs'].append(entry)
(a.output/'REVEAL.json').write_text(json.dumps(manifest,indent=2)+'\n')
text='''# Blind DSP-A listening

Listen to each A/B pair at unchanged playback volume. Label order is randomized independently per pair; RMS is matched and peaks are kept below 0.9 without limiting. The source WAVs are unscaled in the baseline/candidate directories. Do not open REVEAL.json until writing preferences.

For each pair record: A/B/no preference; clicks or roughness; note endings; tonal expression. Repeat on headphones/line output and the built-in speaker if available. Host playback alone does not establish Cardputer output quality. Mono pad-like presets do not create polyphony or a slow attack control.

| Pair | Material | A | B | Preference / notes |
|---|---|---|---|---|
'''
for e in manifest['pairs']:
    text+=f"| {e['id']} | {e['title']} | [{e['id']}-A.wav]({e['id']}-A.wav) | [{e['id']}-B.wav]({e['id']}-B.wav) | |\n"
(a.output/'README.md').write_text(text)
print(f'Blind files written: {len(manifest["pairs"])} RMS-matched pairs. Mapping not printed.')
