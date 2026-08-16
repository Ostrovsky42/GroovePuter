from pathlib import Path

def rep(path, old, new):
    p=Path(path); s=p.read_text()
    if s.count(old)!=1: raise SystemExit(f'{path}: anchor count={s.count(old)}')
    p.write_text(s.replace(old,new,1))

rep('src/ui/pages/project_page.h',
    'enum class MainFocus { Load = 0, SaveAs, New, ImportMidi, ClearProject, VisualStyle, GrooveMode, GrooveFlavor, Volume, LedMode, LedSource, LedColor, LedBri, LedFlash, MidiDevice };',
    'enum class MainFocus { Load = 0, SaveAs, New, ImportMidi, ClearProject, VisualStyle, GrooveMode, GrooveFlavor, Volume, LedMode, LedSource, LedColor, LedBri, LedFlash, MidiDevice, MidiInputEnabled, MidiInputChannel, MidiInputTarget };')
rep('src/ui/pages/project_page.h',
    '  void autoRouteMidi();\n',
    '  void autoRouteMidi();\n  bool adjustMidiInput(int delta);\n')

rep('src/ui/pages/project_page.cpp',
    '#include "../midi_device_profile_ui.h"\n',
    '#include "../midi_device_profile_ui.h"\n#include "../midi_input_ui.h"\n')
rep('src/ui/pages/project_page.cpp',
    'namespace ProfileUi = GroovePuterUi::MidiDeviceProfileUi;\n',
    'namespace ProfileUi = GroovePuterUi::MidiDeviceProfileUi;\nnamespace InputUi = GroovePuterUi::MidiInputUi;\n')
rep('src/ui/pages/project_page.cpp',
    '      first = (int)ProjectPage::MainFocus::MidiDevice;\n      last = (int)ProjectPage::MainFocus::MidiDevice;\n',
    '      first = (int)ProjectPage::MainFocus::MidiDevice;\n      last = (int)ProjectPage::MainFocus::MidiInputTarget;\n')
rep('src/ui/pages/project_page.cpp',
    '  const int maxFocus = static_cast<int>(MainFocus::LedFlash);\n',
    '  const int maxFocus = static_cast<int>(MainFocus::MidiInputTarget);\n')

anchor='bool ProjectPage::handleEvent(UIEvent& ui_event) {'
method='''bool ProjectPage::adjustMidiInput(int delta) {
    const auto current = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
    MidiInputRoutingConfig next = current;
    switch (main_focus_) {
        case MainFocus::MidiInputEnabled:
            next = InputUi::stepEnabled(current);
            break;
        case MainFocus::MidiInputChannel:
            next = InputUi::stepChannel(current, delta);
            break;
        case MainFocus::MidiInputTarget:
            next = InputUi::stepTarget(current, delta);
            break;
        default:
            return false;
    }
    if (!GroovePuterPlatform::setCardputerMidiInputRoutingConfig(next)) {
        UI::showToast("MIDI input save failed", 1400);
    }
    return true;
}

'''
rep('src/ui/pages/project_page.cpp', anchor, method+anchor)

rep('src/ui/pages/project_page.cpp',
    '                midi_profile_preview_ = ProfileUi::encodePreview(next);\n                return true;\n            }\n            if (main_focus_ == MainFocus::Volume) {',
    '                midi_profile_preview_ = ProfileUi::encodePreview(next);\n                return true;\n            }\n            if (adjustMidiInput(right ? 1 : -1)) return true;\n            if (main_focus_ == MainFocus::Volume) {')
rep('src/ui/pages/project_page.cpp',
    "    if (key == '\\n' || key == '\\r') {\n        if (main_focus_ == MainFocus::MidiDevice) {",
    "    if (key == '\\n' || key == '\\r') {\n        if (adjustMidiInput(1)) return true;\n        if (main_focus_ == MainFocus::MidiDevice) {")

rep('src/ui/pages/project_page.cpp',
    '''      case MainFocus::MidiDevice: {
        const auto pending =
            GroovePuterPlatform::pendingCardputerMidiDeviceProfile();
        const auto selected = ProfileUi::profileFromPreview(
            midi_profile_preview_, pending);
        std::snprintf(line, sizeof(line), "Device     <%s>%s",
                      ProfileUi::shortName(selected),
                      selected != pending ? "*" : "");
        break;
      }
''',
    '''      case MainFocus::MidiDevice: {
        const auto pending = GroovePuterPlatform::pendingCardputerMidiDeviceProfile();
        const auto selected = ProfileUi::profileFromPreview(midi_profile_preview_, pending);
        std::snprintf(line, sizeof(line), "Device     <%s>%s", ProfileUi::shortName(selected), selected != pending ? "*" : "");
        break;
      }
      case MainFocus::MidiInputEnabled: {
        const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
        std::snprintf(line, sizeof(line), "MIDI Input  <%s>", InputUi::enabledName(input.enabled));
        break;
      }
      case MainFocus::MidiInputChannel: {
        const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
        char channel[8]{}; InputUi::formatChannel(input, channel, sizeof(channel));
        std::snprintf(line, sizeof(line), "Input Ch    <%s>", channel);
        break;
      }
      case MainFocus::MidiInputTarget: {
        const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
        std::snprintf(line, sizeof(line), "Input To    <%s>", InputUi::targetName(input.target));
        break;
      }
''')

old='''    std::snprintf(midi0, sizeof(midi0), "Saved:%s",
                  ProfileUi::shortName(pending));
    if (selected != pending) {
      std::snprintf(midi1, sizeof(midi1), "Apply:ENTER SAVE");
    } else if (GroovePuterPlatform::cardputerMidiDeviceProfileRestartRequired()) {
      std::snprintf(midi1, sizeof(midi1), "Apply:REBOOT");
    } else {
      std::snprintf(midi1, sizeof(midi1), "Apply:ACTIVE");
    }
    std::snprintf(midi2, sizeof(midi2), "Tab:Section  </>:Edit");
'''
new='''    const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
    char inputChannel[8]{};
    InputUi::formatChannel(input, inputChannel, sizeof(inputChannel));
    std::snprintf(midi0, sizeof(midi0), "OUT DEV:%s%s", ProfileUi::shortName(selected), selected != pending ? "*" : "");
    std::snprintf(midi1, sizeof(midi1), "IN:%s CH:%s", InputUi::enabledName(input.enabled), inputChannel);
    std::snprintf(midi2, sizeof(midi2), "TARGET:%s", InputUi::targetName(input.target));
'''
rep('src/ui/pages/project_page.cpp',old,new)
rep('src/ui/pages/project_page.cpp',
    '  if (sectionIdx == 3) return (int)ProjectPage::MainFocus::MidiDevice;\n  return 0;\n}\n\nbool ProjectPage::focusInSection',
    '  if (sectionIdx == 3) return (int)ProjectPage::MainFocus::MidiInputTarget;\n  return 0;\n}\n\nbool ProjectPage::focusInSection')
rep('src/ui/pages/project_page.cpp',
    '  if (sectionIdx == 3) return f == ProjectPage::MainFocus::MidiDevice;\n',
    '  if (sectionIdx == 3) return f >= ProjectPage::MainFocus::MidiDevice && f <= ProjectPage::MainFocus::MidiInputTarget;\n')

Path('tools/r8_bootstrap.py').unlink()
