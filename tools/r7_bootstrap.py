from pathlib import Path

def rep(path, old, new):
    p=Path(path); s=p.read_text();
    if s.count(old)!=1: raise SystemExit(f'{path}: anchor {s.count(old)}')
    p.write_text(s.replace(old,new,1))

rep('src/platform/cardputer_midi_settings_session.h',
    '#include "src/midi/midi_device_profile_runtime.h"\n',
    '#include "src/midi/midi_device_profile_runtime.h"\n#include "src/input/midi_input_router.h"\n')
rep('src/platform/cardputer_midi_settings_session.h',
    'bool cardputerMidiDeviceProfileRestartRequired();\n#else',
    'bool cardputerMidiDeviceProfileRestartRequired();\nvoid initializeCardputerMidiInputSettings(MidiInputRouter& router);\nMidiInputRoutingConfig cardputerMidiInputRoutingConfig();\nbool setCardputerMidiInputRoutingConfig(const MidiInputRoutingConfig& config);\n#else')
rep('src/platform/cardputer_midi_settings_session.h',
    'inline bool cardputerMidiDeviceProfileRestartRequired() {\n    return pendingCardputerMidiDeviceProfile() !=\n           GroovePuterMidi::midiDeviceProfileRuntime().profile();\n}\n#endif',
    'inline bool cardputerMidiDeviceProfileRestartRequired() {\n    return pendingCardputerMidiDeviceProfile() !=\n           GroovePuterMidi::midiDeviceProfileRuntime().profile();\n}\ninline MidiInputRoutingConfig& desktopMidiInputConfig() { static MidiInputRoutingConfig c{}; return c; }\ninline void initializeCardputerMidiInputSettings(MidiInputRouter& router) { (void)router.setConfig(desktopMidiInputConfig()); }\ninline MidiInputRoutingConfig cardputerMidiInputRoutingConfig() { return desktopMidiInputConfig(); }\ninline bool setCardputerMidiInputRoutingConfig(const MidiInputRoutingConfig& config) { if (!MidiInputRouter::isValidConfig(config)) return false; desktopMidiInputConfig()=config; return true; }\n#endif')

rep('src/platform/cardputer_midi_settings_session.cpp',
    '#include "src/midi/transport_clock_runtime.h"\n',
    '#include "src/midi/transport_clock_runtime.h"\n#include "src/input/midi_input_settings.h"\n')
rep('src/platform/cardputer_midi_settings_session.cpp',
    'namespace GroovePuterPlatform {\nnamespace {\n',
    'namespace GroovePuterPlatform {\nnamespace {\nconstexpr const char* kMidiInputNamespace = "grooveputer";\nconstexpr const char* kMidiInputKey = "midi_in";\n')
rep('src/platform/cardputer_midi_settings_session.cpp',
    '    bool persist(GroovePuterMidi::TransportClockSource source,',
    '''    void initializeInput(MidiInputRouter& router) {
        if (!initialized_) initialize();
        MidiInputRoutingConfig loaded = GroovePuterMidiInput::defaultRoutingConfig();
        Preferences preferences;
        bool decoded = false;
        if (preferences.begin(kMidiInputNamespace, true)) {
            if (preferences.isKey(kMidiInputKey)) {
                decoded = GroovePuterMidiInput::decodeRoutingConfig(
                    preferences.getUInt(kMidiInputKey, 0u), loaded);
            }
            preferences.end();
        }
        inputRouter_ = &router;
        inputConfig_ = decoded ? loaded : GroovePuterMidiInput::defaultRoutingConfig();
        (void)inputRouter_->setConfig(inputConfig_);
    }

    MidiInputRoutingConfig inputConfig() const { return inputConfig_; }

    bool setInputConfig(const MidiInputRoutingConfig& config) {
        if (!MidiInputRouter::isValidConfig(config) || inputRouter_ == nullptr) return false;
        Preferences preferences;
        if (!preferences.begin(kMidiInputNamespace, false)) return false;
        const size_t written = preferences.putUInt(kMidiInputKey, GroovePuterMidiInput::encodeRoutingConfig(config));
        preferences.end();
        if (written != sizeof(uint32_t)) return false;
        if (!inputRouter_->setConfig(config)) return false;
        inputConfig_ = config;
        return true;
    }

    bool persist(GroovePuterMidi::TransportClockSource source,''')
rep('src/platform/cardputer_midi_settings_session.cpp',
    '    bool initialized_{false};\n};',
    '    bool initialized_{false};\n    MidiInputRouter* inputRouter_{nullptr};\n    MidiInputRoutingConfig inputConfig_{};\n};')
rep('src/platform/cardputer_midi_settings_session.cpp',
    'bool cardputerMidiDeviceProfileRestartRequired() {\n    return settingsSession().restartRequired();\n}\n',
    '''bool cardputerMidiDeviceProfileRestartRequired() {
    return settingsSession().restartRequired();
}
void initializeCardputerMidiInputSettings(MidiInputRouter& router) { settingsSession().initializeInput(router); }
MidiInputRoutingConfig cardputerMidiInputRoutingConfig() { return settingsSession().inputConfig(); }
bool setCardputerMidiInputRoutingConfig(const MidiInputRoutingConfig& config) { return settingsSession().setInputConfig(config); }
''')
rep('GroovePuter.ino',
    '  GroovePuterPlatform::initializeCardputerMidiSettingsSession();\n\n  // Start the dispatcher',
    '  GroovePuterPlatform::initializeCardputerMidiSettingsSession();\n  GroovePuterPlatform::initializeCardputerMidiInputSettings(g_midiInputRouter);\n\n  // Start the dispatcher')
Path('tools/r7_bootstrap.py').unlink()
