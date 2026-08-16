#!/usr/bin/env python3
from pathlib import Path
h=Path('src/input/midi_input_settings.h').read_text(); ph=Path('src/platform/cardputer_midi_settings_session.h').read_text(); pc=Path('src/platform/cardputer_midi_settings_session.cpp').read_text(); app=Path('GroovePuter.ino').read_text()
for f in ('Preferences','Arduino','NVS','ui/','midi_device_profile'):
 assert f not in '\n'.join(x for x in h.splitlines() if x.lstrip().startswith('#include')),f
for t in ('kSettingsVersion','kSettingsMagic','encodeRoutingConfig','decodeRoutingConfig','defaultRoutingConfig'):
 assert t in h,t
for t in ('initializeCardputerMidiInputSettings','cardputerMidiInputRoutingConfig','setCardputerMidiInputRoutingConfig'):
 assert t in ph and t in pc,t
assert 'kMidiInputKey = "midi_in"' in pc
assert 'putUInt(kMidiInputKey' in pc and 'getUInt(kMidiInputKey' in pc
assert app.index('initializeCardputerMidiInputSettings(g_midiInputRouter)') < app.index('registerCardputerUsbMidiSink(')
for f in ('sessionId','overflowEpoch','activeNote','droppedOverflow'):
 assert f not in h,f
print('0.9.10 R7 persistence/source boundaries: PASS')
