#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/ui/pages/project_page.h").read_text(encoding="utf-8")
page = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")
ui = (ROOT / "src/ui/midi_input_ui.h").read_text(encoding="utf-8")
settings = (ROOT / "src/platform/cardputer_midi_settings_session.h").read_text(encoding="utf-8")

for token in (
    "MidiInputEnabled", "MidiInputChannel", "MidiInputTarget",
    "bool adjustMidiInput(int delta);",
):
    assert token in header, token

for token in (
    'case MainFocus::MidiInputEnabled:',
    'case MainFocus::MidiInputChannel:',
    'case MainFocus::MidiInputTarget:',
    '"MIDI Input  <%s>"',
    '"Input Ch    <%s>"',
    '"Input To    <%s>"',
    'GroovePuterPlatform::cardputerMidiInputRoutingConfig()',
    'GroovePuterPlatform::setCardputerMidiInputRoutingConfig(next)',
    'InputUi::stepEnabled', 'InputUi::stepChannel', 'InputUi::stepTarget',
    'MainFocus::MidiInputTarget',
    'OUT DEV:', 'IN:%s CH:%s', 'TARGET:%s',
):
    assert token in page, token

assert 'last = (int)ProjectPage::MainFocus::MidiInputTarget;' in page
assert 'f <= ProjectPage::MainFocus::MidiInputTarget' in page
assert 'MainFocus::MidiInputTarget' in page
assert 'MainFocus { Load = 0' in header
assert 'ProjectSection { Scenes = 0, Groove, Led, Midi }' in header

# Input UI must use the R7 input settings owner, never OutputOwnership or DeviceProfile mutation.
method_start = page.index('bool ProjectPage::adjustMidiInput(int delta)')
method_end = page.index('bool ProjectPage::handleEvent', method_start)
method = page[method_start:method_end]
assert 'setCardputerMidiInputRoutingConfig(next)' in method
for forbidden in ('setOutputOwnership', 'selectCardputerMidiDeviceProfileForNextBoot', 'markSceneMutated'):
    assert forbidden not in method, forbidden

# The pure UI projection remains independent of platform persistence and outbound MIDI semantics.
includes = '\n'.join(line for line in ui.splitlines() if line.lstrip().startswith('#include'))
for forbidden in ('Preferences', 'cardputer_midi_settings_session', 'output_ownership', 'midi_device_profile'):
    assert forbidden not in includes, forbidden

for token in ('cardputerMidiInputRoutingConfig', 'setCardputerMidiInputRoutingConfig'):
    assert token in settings, token

print('0.9.10 R8 Project MIDI input UI/source boundaries: PASS')
