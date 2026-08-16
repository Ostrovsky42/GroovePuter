#!/usr/bin/env python3
from pathlib import Path
r=Path('src/input/midi_input_router.h').read_text()
for t in ('case MidiInputMessageType::ControlChange:','case MidiInputMessageType::PitchBend:','++diagnostics_.ignoredUnsupported;'):
 assert t in r,t
inc='\n'.join(x for x in r.splitlines() if x.lstrip().startswith('#include'))
for f in ('miniacid_engine','output_ownership','midi_device_profile','Preferences','ui/'):
 assert f not in inc,f
assert 'sustain' not in r.lower()
print('0.9.10 R6 controller source boundary: PASS')
