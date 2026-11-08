#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(path: str, old: str, new: str, label: str) -> None:
    p = ROOT / path
    s = p.read_text()
    if new in s and old not in s:
        return
    count = s.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one anchor, found {count}")
    p.write_text(s.replace(old, new, 1))


def ensure_file(path: str, content: str) -> None:
    p = ROOT / path
    if p.exists():
        if p.read_text() != content:
            raise SystemExit(f"{path}: exists with unexpected content")
        return
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(content)


# ---------------------------------------------------------------------------
# 1. USB/DIN cleanup debt survives disconnect and is reconciled before USB
#    regains authority.
# ---------------------------------------------------------------------------
replace_once(
    "src/midi/tee_midi_transport.h",
    '''        const bool primaryMounted = primary_.mounted();
        if (!primaryMounted) {
            diagnostics_.primaryConsecutiveRejects = 0;
            primaryCleanupDebtMask_ = 0;
        }

        bool wasStalled = primaryStalled();
        if (primaryMounted && wasStalled && primaryCleanupDebtMask_ != 0u) {
''',
    '''        const bool primaryMounted = primary_.mounted();
        if (!primaryMounted) {
            // A physical disconnect ends the current stall episode, but it does
            // not prove that USB received cleanup already accepted by DIN.
            // Preserve cleanup debt across reconnect so USB cannot regain
            // authority with stale note ownership.
            diagnostics_.primaryConsecutiveRejects = 0;
        }

        bool wasStalled = primaryStalled();
        if (primaryMounted && primaryCleanupDebtMask_ != 0u) {
''',
    "preserve cleanup debt across disconnect")
replace_once(
    "src/midi/tee_midi_transport.h",
    '''        if (!primaryResult && secondaryResult && cleanupCritical && primaryMounted) {
            primaryCleanupDebtMask_ |= channelMask(channel);
        }
''',
    '''        if (!primaryResult && secondaryResult && cleanupCritical) {
            // Even an unplugged USB primary may still own notes from before the
            // disconnect. DIN-only cleanup therefore creates debt that must be
            // reconciled if USB later reconnects.
            primaryCleanupDebtMask_ |= channelMask(channel);
        }
''',
    "record cleanup debt while primary is absent")

# ---------------------------------------------------------------------------
# 2. Persisted input codec. Invalid/corrupt state fails closed to OFF.
# ---------------------------------------------------------------------------
ensure_file(
    "src/midi/midi_input_settings.h",
    '''#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_SETTINGS_H
#define GROOVEPUTER_MIDI_INPUT_SETTINGS_H

#include <cstdint>

#include "midi_input_dispatcher.h"

namespace GroovePuterMidi::MidiInputSettings {

constexpr uint8_t kSettingsVersion = 1u;
constexpr uint8_t kSettingsMagic = 0xA7u;
constexpr uint32_t kKnownPayloadMask = 0x000000FFu;

inline MidiInputRoutingConfig defaultRoutingConfig() {
    return MidiInputRoutingConfig{};
}

inline uint32_t encodeRoutingConfig(const MidiInputRoutingConfig& config) {
    const uint8_t payload = static_cast<uint8_t>(
        (config.enabled ? 0x01u : 0u) |
        (config.channelMode == MidiInputChannelMode::Single ? 0x02u : 0u) |
        ((config.channel & 0x0Fu) << 2u) |
        ((static_cast<uint8_t>(config.target) & 0x03u) << 6u));
    return (static_cast<uint32_t>(kSettingsMagic) << 24u) |
           (static_cast<uint32_t>(kSettingsVersion) << 16u) |
           payload;
}

inline bool decodeRoutingConfig(uint32_t word, MidiInputRoutingConfig& out) {
    if (static_cast<uint8_t>(word >> 24u) != kSettingsMagic ||
        static_cast<uint8_t>(word >> 16u) != kSettingsVersion ||
        (word & 0x0000FF00u) != 0u) {
        out = defaultRoutingConfig();
        return false;
    }

    const uint8_t payload = static_cast<uint8_t>(word & kKnownPayloadMask);
    MidiInputRoutingConfig candidate{};
    candidate.enabled = (payload & 0x01u) != 0u;
    candidate.channelMode = (payload & 0x02u) != 0u
        ? MidiInputChannelMode::Single
        : MidiInputChannelMode::Omni;
    candidate.channel = static_cast<uint8_t>((payload >> 2u) & 0x0Fu);
    const uint8_t target = static_cast<uint8_t>((payload >> 6u) & 0x03u);
    if (target > static_cast<uint8_t>(MidiInputTarget::Drums)) {
        out = defaultRoutingConfig();
        return false;
    }
    candidate.target = static_cast<MidiInputTarget>(target);
    if (!MidiInputDispatcher::isValidConfig(candidate)) {
        out = defaultRoutingConfig();
        return false;
    }
    out = candidate;
    return true;
}

}  // namespace GroovePuterMidi::MidiInputSettings

#endif  // GROOVEPUTER_MIDI_INPUT_SETTINGS_H
''')

# ---------------------------------------------------------------------------
# 3. Compact user-facing value helpers.
# ---------------------------------------------------------------------------
ensure_file(
    "src/ui/midi_input_ui.h",
    '''#pragma once
#ifndef GROOVEPUTER_MIDI_INPUT_UI_H
#define GROOVEPUTER_MIDI_INPUT_UI_H

#include <cstdio>

#include "src/midi/midi_input_dispatcher.h"

namespace GroovePuterUi::MidiInputUi {

inline const char* enabledName(bool enabled) {
    return enabled ? "ON" : "OFF";
}

inline const char* targetName(GroovePuterMidi::MidiInputTarget target) {
    switch (target) {
        case GroovePuterMidi::MidiInputTarget::SynthA: return "SYN A";
        case GroovePuterMidi::MidiInputTarget::SynthB: return "SYN B";
        case GroovePuterMidi::MidiInputTarget::Drums: return "DRUMS";
    }
    return "SYN A";
}

inline void formatChannel(const GroovePuterMidi::MidiInputRoutingConfig& config,
                          char* out,
                          std::size_t size) {
    if (size == 0u) return;
    if (config.channelMode == GroovePuterMidi::MidiInputChannelMode::Omni) {
        std::snprintf(out, size, "OMNI");
        return;
    }
    std::snprintf(out, size, "CH%u", static_cast<unsigned>(config.channel + 1u));
}

inline GroovePuterMidi::MidiInputRoutingConfig stepEnabled(
        GroovePuterMidi::MidiInputRoutingConfig config) {
    config.enabled = !config.enabled;
    return config;
}

inline GroovePuterMidi::MidiInputRoutingConfig stepChannel(
        GroovePuterMidi::MidiInputRoutingConfig config,
        int delta) {
    int index = config.channelMode == GroovePuterMidi::MidiInputChannelMode::Omni
        ? 0
        : static_cast<int>(config.channel) + 1;
    index = (index + delta) % 17;
    if (index < 0) index += 17;
    if (index == 0) {
        config.channelMode = GroovePuterMidi::MidiInputChannelMode::Omni;
        config.channel = 0;
    } else {
        config.channelMode = GroovePuterMidi::MidiInputChannelMode::Single;
        config.channel = static_cast<uint8_t>(index - 1);
    }
    return config;
}

inline GroovePuterMidi::MidiInputRoutingConfig stepTarget(
        GroovePuterMidi::MidiInputRoutingConfig config,
        int delta) {
    int target = static_cast<int>(config.target);
    target = (target + delta) % 3;
    if (target < 0) target += 3;
    config.target = static_cast<GroovePuterMidi::MidiInputTarget>(target);
    return config;
}

}  // namespace GroovePuterUi::MidiInputUi

#endif  // GROOVEPUTER_MIDI_INPUT_UI_H
''')

# ---------------------------------------------------------------------------
# 4. Runtime config seam: UI/persistence requests are copied under a small
#    critical section, but owner release happens only on MidiDispatchTask.
# ---------------------------------------------------------------------------
replace_once(
    "src/platform/cardputer_usb_midi_service.h",
    '''GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig();
bool setCardputerMidiInputRoutingConfig(
    const GroovePuterMidi::MidiInputRoutingConfig& config);
''',
    '''GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRuntimeRoutingConfig();
bool applyCardputerMidiInputRuntimeRoutingConfig(
    const GroovePuterMidi::MidiInputRoutingConfig& config);
''',
    "rename runtime input config seam")

replace_once(
    "src/platform/cardputer_usb_midi_transport.cpp",
    '''GroovePuterMidi::MidiInputDispatcher g_inputDispatcher;
bool g_usbInputMounted = false;
''',
    '''GroovePuterMidi::MidiInputDispatcher g_inputDispatcher;
bool g_usbInputMounted = false;
portMUX_TYPE g_inputConfigMux = portMUX_INITIALIZER_UNLOCKED;
GroovePuterMidi::MidiInputRoutingConfig g_requestedInputConfig{};
uint32_t g_requestedInputConfigVersion = 0;
uint32_t g_appliedInputConfigVersion = 0;
''',
    "input config mailbox globals")

replace_once(
    "src/platform/cardputer_usb_midi_transport.cpp",
    '''void syncUsbMidiInputLifecycle() {
''',
    '''bool sameMidiInputConfig(const GroovePuterMidi::MidiInputRoutingConfig& lhs,
                         const GroovePuterMidi::MidiInputRoutingConfig& rhs) {
    return lhs.enabled == rhs.enabled &&
           lhs.channelMode == rhs.channelMode &&
           lhs.channel == rhs.channel &&
           lhs.target == rhs.target;
}

void applyPendingMidiInputConfig() {
    GroovePuterMidi::MidiInputRoutingConfig requested{};
    uint32_t requestedVersion = 0;
    portENTER_CRITICAL(&g_inputConfigMux);
    requested = g_requestedInputConfig;
    requestedVersion = g_requestedInputConfigVersion;
    portEXIT_CRITICAL(&g_inputConfigMux);
    if (requestedVersion == g_appliedInputConfigVersion) return;
    if (g_inputDispatcher.setConfig(requested)) {
        g_appliedInputConfigVersion = requestedVersion;
    }
}

void syncUsbMidiInputLifecycle() {
''',
    "dispatcher-owned input config apply")

replace_once(
    "src/platform/cardputer_usb_midi_transport.cpp",
    '''    syncUsbMidiInputLifecycle();
    midiEventPacket_t packet{};
''',
    '''    applyPendingMidiInputConfig();
    syncUsbMidiInputLifecycle();
    midiEventPacket_t packet{};
''',
    "apply input config before packet dispatch")

replace_once(
    "src/platform/cardputer_usb_midi_transport.cpp",
    '''GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig() {
    return g_inputDispatcher.config();
}

bool setCardputerMidiInputRoutingConfig(
        const GroovePuterMidi::MidiInputRoutingConfig& config) {
    return g_inputDispatcher.setConfig(config);
}
''',
    '''GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRuntimeRoutingConfig() {
    portENTER_CRITICAL(&g_inputConfigMux);
    const GroovePuterMidi::MidiInputRoutingConfig config = g_requestedInputConfig;
    portEXIT_CRITICAL(&g_inputConfigMux);
    return config;
}

bool applyCardputerMidiInputRuntimeRoutingConfig(
        const GroovePuterMidi::MidiInputRoutingConfig& config) {
    if (!GroovePuterMidi::MidiInputDispatcher::isValidConfig(config)) return false;
    bool changed = false;
    portENTER_CRITICAL(&g_inputConfigMux);
    if (!sameMidiInputConfig(g_requestedInputConfig, config)) {
        g_requestedInputConfig = config;
        ++g_requestedInputConfigVersion;
        if (g_requestedInputConfigVersion == 0u) ++g_requestedInputConfigVersion;
        changed = true;
    }
    portEXIT_CRITICAL(&g_inputConfigMux);
    if (changed) notifyDispatcher();
    return true;
}
''',
    "runtime input config mailbox seam")

# Keep the source-of-truth runtime wiring generator consistent with the
# published direct code so future idempotent replays cannot restore old names.
replace_once(
    "tools/apply_c9_midi_runtime_wiring.py",
    "GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig();\\n",
    "GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRuntimeRoutingConfig();\\n",
    "runtime generator getter name")
replace_once(
    "tools/apply_c9_midi_runtime_wiring.py",
    "bool setCardputerMidiInputRoutingConfig(\\n",
    "bool applyCardputerMidiInputRuntimeRoutingConfig(\\n",
    "runtime generator setter declaration")
replace_once(
    "tools/apply_c9_midi_runtime_wiring.py",
    "GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig() {\\n",
    "GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRuntimeRoutingConfig() {\\n",
    "runtime generator getter definition")
replace_once(
    "tools/apply_c9_midi_runtime_wiring.py",
    "bool setCardputerMidiInputRoutingConfig(\\n",
    "bool applyCardputerMidiInputRuntimeRoutingConfig(\\n",
    "runtime generator setter definition")

# ---------------------------------------------------------------------------
# 5. Durable input persistence. Save first, then schedule live application.
# ---------------------------------------------------------------------------
replace_once(
    "src/platform/cardputer_midi_settings_session.h",
    '''#include "src/midi/midi_device_profile_runtime.h"
''',
    '''#include "src/midi/midi_device_profile_runtime.h"
#include "src/midi/midi_input_dispatcher.h"
''',
    "input config type include")
replace_once(
    "src/platform/cardputer_midi_settings_session.h",
    '''#ifdef ARDUINO
GroovePuterMidi::MidiDeviceProfile pendingCardputerMidiDeviceProfile();
''',
    '''#ifdef ARDUINO
void initializeCardputerMidiInputSettings();
GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig();
bool setCardputerMidiInputRoutingConfig(
    const GroovePuterMidi::MidiInputRoutingConfig& config);

GroovePuterMidi::MidiDeviceProfile pendingCardputerMidiDeviceProfile();
''',
    "Arduino input settings API")
replace_once(
    "src/platform/cardputer_midi_settings_session.h",
    '''inline GroovePuterMidi::MidiDeviceProfile pendingCardputerMidiDeviceProfile() {
''',
    '''inline GroovePuterMidi::MidiInputRoutingConfig& desktopMidiInputRoutingConfig() {
    static GroovePuterMidi::MidiInputRoutingConfig config{};
    return config;
}
}  // namespace Detail

inline void initializeCardputerMidiInputSettings() {}

inline GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig() {
    return Detail::desktopMidiInputRoutingConfig();
}

inline bool setCardputerMidiInputRoutingConfig(
        const GroovePuterMidi::MidiInputRoutingConfig& config) {
    if (!GroovePuterMidi::MidiInputDispatcher::isValidConfig(config)) return false;
    Detail::desktopMidiInputRoutingConfig() = config;
    return true;
}

namespace Detail {
inline GroovePuterMidi::MidiDeviceProfile pendingProfileValue() {
    return desktopMidiProfileSelection().profile;
}
}  // namespace Detail

inline GroovePuterMidi::MidiDeviceProfile pendingCardputerMidiDeviceProfile() {
''',
    "desktop input settings API")
# The preceding insertion intentionally closes and reopens Detail. Existing
# function bodies still compile unchanged.

replace_once(
    "src/platform/cardputer_midi_settings_session.cpp",
    '''#include "src/midi/midi_device_profile_runtime.h"
''',
    '''#include "src/midi/midi_device_profile_runtime.h"
#include "src/midi/midi_input_settings.h"
''',
    "input settings codec include")
replace_once(
    "src/platform/cardputer_midi_settings_session.cpp",
    '''#include "src/midi/transport_clock_runtime.h"
''',
    '''#include "src/midi/transport_clock_runtime.h"
#include "src/platform/cardputer_usb_midi_service.h"
''',
    "input runtime seam include")
replace_once(
    "src/platform/cardputer_midi_settings_session.cpp",
    '''CardputerMidiSettingsSession& settingsSession() {
    static CardputerMidiSettingsSession session;
    return session;
}
''',
    '''CardputerMidiSettingsSession& settingsSession() {
    static CardputerMidiSettingsSession session;
    return session;
}

class CardputerMidiInputSettingsSession {
public:
    void initialize() {
        if (initialized_) return;
        GroovePuterMidi::MidiInputRoutingConfig loaded =
            GroovePuterMidi::MidiInputSettings::defaultRoutingConfig();
        bool decoded = false;
        Preferences preferences;
        if (preferences.begin(kNamespace, true)) {
            if (preferences.isKey(kKey)) {
                const uint32_t word = preferences.getUInt(kKey, 0u);
                decoded = GroovePuterMidi::MidiInputSettings::decodeRoutingConfig(
                    word, loaded);
            }
            preferences.end();
        }
        config_ = loaded;
        initialized_ = true;
        const bool applied = applyCardputerMidiInputRuntimeRoutingConfig(config_);
        Serial.printf(
            "[MIDI-IN] load=%u enabled=%u mode=%u ch=%u target=%u apply=%u\\n",
            static_cast<unsigned>(decoded ? 1 : 0),
            static_cast<unsigned>(config_.enabled ? 1 : 0),
            static_cast<unsigned>(config_.channelMode),
            static_cast<unsigned>(config_.channel + 1u),
            static_cast<unsigned>(config_.target),
            static_cast<unsigned>(applied ? 1 : 0));
    }

    GroovePuterMidi::MidiInputRoutingConfig config() {
        if (!initialized_) initialize();
        return config_;
    }

    bool set(const GroovePuterMidi::MidiInputRoutingConfig& config) {
        if (!GroovePuterMidi::MidiInputDispatcher::isValidConfig(config)) return false;
        if (!initialized_) initialize();
        if (same(config_, config)) return true;

        Preferences preferences;
        if (!preferences.begin(kNamespace, false)) return false;
        const uint32_t word =
            GroovePuterMidi::MidiInputSettings::encodeRoutingConfig(config);
        const std::size_t written = preferences.putUInt(kKey, word);
        preferences.end();
        if (written != sizeof(uint32_t)) return false;

        // Durable-before-live: if power is lost after this point, boot restores
        // the requested policy. Live owner release is performed later by the
        // sole MidiDispatchTask through its config mailbox.
        if (!applyCardputerMidiInputRuntimeRoutingConfig(config)) return false;
        config_ = config;
        Serial.printf(
            "[MIDI-IN] save enabled=%u mode=%u ch=%u target=%u\\n",
            static_cast<unsigned>(config_.enabled ? 1 : 0),
            static_cast<unsigned>(config_.channelMode),
            static_cast<unsigned>(config_.channel + 1u),
            static_cast<unsigned>(config_.target));
        return true;
    }

private:
    static bool same(const GroovePuterMidi::MidiInputRoutingConfig& lhs,
                     const GroovePuterMidi::MidiInputRoutingConfig& rhs) {
        return lhs.enabled == rhs.enabled &&
               lhs.channelMode == rhs.channelMode &&
               lhs.channel == rhs.channel &&
               lhs.target == rhs.target;
    }

    static constexpr const char* kNamespace = "grooveputer";
    static constexpr const char* kKey = "midi_in";
    GroovePuterMidi::MidiInputRoutingConfig config_{};
    bool initialized_{false};
};

CardputerMidiInputSettingsSession& inputSettingsSession() {
    static CardputerMidiInputSettingsSession session;
    return session;
}
''',
    "input settings session")
replace_once(
    "src/platform/cardputer_midi_settings_session.cpp",
    '''void initializeCardputerMidiSettingsSession() {
    settingsSession().initialize();
}
''',
    '''void initializeCardputerMidiSettingsSession() {
    settingsSession().initialize();
}

void initializeCardputerMidiInputSettings() {
    inputSettingsSession().initialize();
}

GroovePuterMidi::MidiInputRoutingConfig cardputerMidiInputRoutingConfig() {
    return inputSettingsSession().config();
}

bool setCardputerMidiInputRoutingConfig(
        const GroovePuterMidi::MidiInputRoutingConfig& config) {
    return inputSettingsSession().set(config);
}
''',
    "public persisted input settings API")

# Restore only after the runtime dispatcher has bound its router/IO owners.
replace_once(
    "GroovePuter.ino",
    '''  } else {
    markBootStage(53, "after USB MIDI sink");
  }

  screenLog("5. Creating Encoder8");
''',
    '''  } else {
    markBootStage(53, "after USB MIDI sink");
    GroovePuterPlatform::initializeCardputerMidiInputSettings();
  }

  screenLog("5. Creating Encoder8");
''',
    "boot input settings restore")

# ---------------------------------------------------------------------------
# 6. Project/MIDI user controls.
# ---------------------------------------------------------------------------
replace_once(
    "src/ui/pages/project_page.h",
    '''  enum class MainFocus { Load = 0, SaveAs, New, ImportMidi, ClearProject, VisualStyle, GrooveMode, GrooveFlavor, Volume, LedMode, LedSource, LedColor, LedBri, LedFlash, MidiDevice };
''',
    '''  enum class MainFocus { Load = 0, SaveAs, New, ImportMidi, ClearProject, VisualStyle, GrooveMode, GrooveFlavor, Volume, LedMode, LedSource, LedColor, LedBri, LedFlash, MidiDevice, MidiInputEnabled, MidiInputChannel, MidiInputTarget };
''',
    "Project MIDI input focus enum")
replace_once(
    "src/ui/pages/project_page.h",
    '''  void autoRouteMidi();
''',
    '''  void autoRouteMidi();
  bool adjustMidiInput(int delta);
''',
    "Project MIDI input adjustment declaration")

replace_once(
    "src/ui/pages/project_page.cpp",
    '''#include "../midi_device_profile_ui.h"
''',
    '''#include "../midi_device_profile_ui.h"
#include "../midi_input_ui.h"
''',
    "Project input UI include")
replace_once(
    "src/ui/pages/project_page.cpp",
    '''namespace ProfileUi = GroovePuterUi::MidiDeviceProfileUi;
''',
    '''namespace ProfileUi = GroovePuterUi::MidiDeviceProfileUi;
namespace InputUi = GroovePuterUi::MidiInputUi;
''',
    "Project input UI namespace")
replace_once(
    "src/ui/pages/project_page.cpp",
    '''    case 3: // midi
      first = (int)ProjectPage::MainFocus::MidiDevice;
      last = (int)ProjectPage::MainFocus::MidiDevice;
      return;
''',
    '''    case 3: // midi
      first = (int)ProjectPage::MainFocus::MidiDevice;
      last = (int)ProjectPage::MainFocus::MidiInputTarget;
      return;
''',
    "Project MIDI section range")
replace_once(
    "src/ui/pages/project_page.cpp",
    '''  const int maxFocus = static_cast<int>(MainFocus::LedFlash);
''',
    '''  const int maxFocus = static_cast<int>(MainFocus::MidiInputTarget);
''',
    "Project main focus max")
replace_once(
    "src/ui/pages/project_page.cpp",
    '''bool ProjectPage::handleEvent(UIEvent& ui_event) {
''',
    '''bool ProjectPage::adjustMidiInput(int delta) {
    const auto current = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
    auto next = current;
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

bool ProjectPage::handleEvent(UIEvent& ui_event) {
''',
    "Project MIDI input adjustment")
replace_once(
    "src/ui/pages/project_page.cpp",
    '''            if (main_focus_ == MainFocus::MidiDevice) {
''',
    '''            if (main_focus_ == MainFocus::MidiInputEnabled ||
                main_focus_ == MainFocus::MidiInputChannel ||
                main_focus_ == MainFocus::MidiInputTarget) {
                return adjustMidiInput(right ? 1 : -1);
            }
            if (main_focus_ == MainFocus::MidiDevice) {
''',
    "Project arrow input controls")
replace_once(
    "src/ui/pages/project_page.cpp",
    '''    if (key == '\\n' || key == '\\r') {
        if (main_focus_ == MainFocus::MidiDevice) {
''',
    '''    if (key == '\\n' || key == '\\r') {
        if (main_focus_ == MainFocus::MidiInputEnabled ||
            main_focus_ == MainFocus::MidiInputChannel ||
            main_focus_ == MainFocus::MidiInputTarget) {
            return adjustMidiInput(1);
        }
        if (main_focus_ == MainFocus::MidiDevice) {
''',
    "Project enter input controls")
replace_once(
    "src/ui/pages/project_page.cpp",
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
        const auto pending =
            GroovePuterPlatform::pendingCardputerMidiDeviceProfile();
        const auto selected = ProfileUi::profileFromPreview(
            midi_profile_preview_, pending);
        std::snprintf(line, sizeof(line), "Device     <%s>%s",
                      ProfileUi::shortName(selected),
                      selected != pending ? "*" : "");
        break;
      }
      case MainFocus::MidiInputEnabled: {
        const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
        std::snprintf(line, sizeof(line), "MIDI Input <%s>",
                      InputUi::enabledName(input.enabled));
        break;
      }
      case MainFocus::MidiInputChannel: {
        const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
        char channel[12];
        InputUi::formatChannel(input, channel, sizeof(channel));
        std::snprintf(line, sizeof(line), "Input Ch   <%s>", channel);
        break;
      }
      case MainFocus::MidiInputTarget: {
        const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
        std::snprintf(line, sizeof(line), "Input To   <%s>",
                      InputUi::targetName(input.target));
        break;
      }
''',
    "Project draw input controls")
replace_once(
    "src/ui/pages/project_page.cpp",
    '''    std::snprintf(midi2, sizeof(midi2), "Tab:Section  </>:Edit");
''',
    '''    const auto input = GroovePuterPlatform::cardputerMidiInputRoutingConfig();
    char inputChannel[10];
    InputUi::formatChannel(input, inputChannel, sizeof(inputChannel));
    std::snprintf(midi2, sizeof(midi2), "In:%s %s>%s",
                  InputUi::enabledName(input.enabled), inputChannel,
                  InputUi::targetName(input.target));
''',
    "Project MIDI input summary")
replace_once(
    "src/ui/pages/project_page.cpp",
    '''  if (sectionIdx == 3) return (int)ProjectPage::MainFocus::MidiDevice;
''',
    '''  if (sectionIdx == 3) return (int)ProjectPage::MainFocus::MidiInputTarget;
''',
    "Project MIDI section last focus")
replace_once(
    "src/ui/pages/project_page.cpp",
    '''  if (sectionIdx == 3) return f == ProjectPage::MainFocus::MidiDevice;
''',
    '''  if (sectionIdx == 3) return f >= ProjectPage::MainFocus::MidiDevice &&
                              f <= ProjectPage::MainFocus::MidiInputTarget;
''',
    "Project MIDI focus membership")

# ---------------------------------------------------------------------------
# 7. One-shot flushed breadcrumbs around the known FEEL/settings -> Synth A/B
#    panic window. These do not change navigation or rendering semantics.
# ---------------------------------------------------------------------------
replace_once(
    "src/ui/miniacid_display.cpp",
    '''constexpr int kSmfPlayerPage = WorkflowPages::kPlayer;
''',
    '''constexpr int kSmfPlayerPage = WorkflowPages::kPlayer;

#if defined(ARDUINO) && (defined(ESP32) || defined(ESP_PLATFORM))
int g_firstSynthFrameTracePage = -1;
void traceSynthUiStage(int page, const char* stage) {
    if (page != 1 && page != 2) return;
    Serial.printf("[UI-TRACE] page=%d stage=%s\\n", page, stage);
    Serial.flush();
}
#else
int g_firstSynthFrameTracePage = -1;
void traceSynthUiStage(int, const char*) {}
#endif
''',
    "Synth crash trace helper")
replace_once(
    "src/ui/miniacid_display.cpp",
    '''        pages_[index] = createPage_(index);
        if (pages_[index]) {
            pages_[index]->restoreViewContinuity(ui_view_continuity_);
            pages_[index]->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
            pages_[index]->setVisualStyle(UI::currentStyle);
        }
''',
    '''        pages_[index] = createPage_(index);
        if (pages_[index]) {
            traceSynthUiStage(index, "post-create");
            traceSynthUiStage(index, "restore-begin");
            pages_[index]->restoreViewContinuity(ui_view_continuity_);
            traceSynthUiStage(index, "restore-end");
            pages_[index]->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
            traceSynthUiStage(index, "bounds-end");
            pages_[index]->setVisualStyle(UI::currentStyle);
            traceSynthUiStage(index, "style-end");
            if (index == 1 || index == 2) g_firstSynthFrameTracePage = index;
        }
''',
    "post-create Synth crash breadcrumbs")
replace_once(
    "src/ui/miniacid_display.cpp",
    '''    const UI::UiStatusSnapshot frameStatus =
        UI::captureUiStatusSnapshot(mini_acid_, statusContext);
    
    UI::UiShellFrameModel shellFrame{};
''',
    '''    if (g_firstSynthFrameTracePage == page_index_) {
        traceSynthUiStage(page_index_, "frame-status-begin");
    }
    const UI::UiStatusSnapshot frameStatus =
        UI::captureUiStatusSnapshot(mini_acid_, statusContext);
    if (g_firstSynthFrameTracePage == page_index_) {
        traceSynthUiStage(page_index_, "frame-status-end");
    }
    
    UI::UiShellFrameModel shellFrame{};
''',
    "first frame status breadcrumbs")
replace_once(
    "src/ui/miniacid_display.cpp",
    '''    IPage* currentPage = getPage_(page_index_);
    if (currentPage) {
        currentPage->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
        currentPage->tick();
        currentPage->draw(gfx_);
''',
    '''    if (g_firstSynthFrameTracePage == page_index_) {
        traceSynthUiStage(page_index_, "frame-get-begin");
    }
    IPage* currentPage = getPage_(page_index_);
    if (g_firstSynthFrameTracePage == page_index_) {
        traceSynthUiStage(page_index_, "frame-get-end");
    }
    if (currentPage) {
        currentPage->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
        if (g_firstSynthFrameTracePage == page_index_) {
            traceSynthUiStage(page_index_, "frame-tick-begin");
        }
        currentPage->tick();
        if (g_firstSynthFrameTracePage == page_index_) {
            traceSynthUiStage(page_index_, "frame-tick-end");
            traceSynthUiStage(page_index_, "frame-draw-begin");
        }
        currentPage->draw(gfx_);
        if (g_firstSynthFrameTracePage == page_index_) {
            traceSynthUiStage(page_index_, "frame-draw-end");
            g_firstSynthFrameTracePage = -1;
        }
''',
    "first frame tick draw breadcrumbs")
replace_once(
    "src/ui/miniacid_display.cpp",
    '''    IPage* newPage = getPage_(index);
    if (newPage) {
        newPage->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
        newPage->onEnter(context);
        Serial.printf("[UI] transition: %d -> %d (%s, ctx=%d)\\n", 
                      previous_page_index_, page_index_, newPage->getTitle().c_str(), context);
    }
''',
    '''    IPage* newPage = getPage_(index);
    traceSynthUiStage(index, "transition-get-end");
    if (newPage) {
        newPage->setBoundaries(Rect{0, 0, gfx_.width(), gfx_.height()});
        traceSynthUiStage(index, "transition-bounds-end");
        newPage->onEnter(context);
        traceSynthUiStage(index, "transition-enter-end");
        traceSynthUiStage(index, "transition-title-begin");
        const std::string& transitionTitle = newPage->getTitle();
        traceSynthUiStage(index, "transition-title-end");
        Serial.printf("[UI] transition: %d -> %d (%s, ctx=%d)\\n", 
                      previous_page_index_, page_index_, transitionTitle.c_str(), context);
        traceSynthUiStage(index, "transition-log-end");
    }
''',
    "transition Synth crash breadcrumbs")

print("C9 MIDI user closure applied")
