#!/usr/bin/env python3
"""Generate an isolated next-table phase-alignment candidate, not a product edit."""
import argparse
from pathlib import Path
import difflib

p=argparse.ArgumentParser()
p.add_argument('output',type=Path)
a=p.parse_args()
root=Path(__file__).resolve().parents[2]
source=root/'src/dsp/wave_morph_synth_voice.cpp'
text=source.read_text()
needle='    const float b = tableSample(nextWave, phase_);'
assert text.count(needle)==1
replacement='''    // EXPERIMENT ONLY: align the two negatively correlated adjacent pairs.
    // This changes Morph=1 phase relative to the next Wave's Morph=0.
    float nextPhase = phase_;
    if (wave == 0) nextPhase += 0.5f;
    else if (wave == 2) nextPhase += 0.25f;
    if (nextPhase >= 1.0f) nextPhase -= 1.0f;
    const float b = tableSample(nextWave, nextPhase);'''
modified=text.replace(needle,replacement)
a.output.parent.mkdir(parents=True,exist_ok=True)
a.output.write_text(modified)
a.output.with_suffix('.patch').write_text(''.join(difflib.unified_diff(text.splitlines(True),modified.splitlines(True),fromfile='product/wave_morph_synth_voice.cpp',tofile='experiment/wave_morph_synth_voice.cpp')))
print(a.output)
