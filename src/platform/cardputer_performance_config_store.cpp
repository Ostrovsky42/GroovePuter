#include "cardputer_performance_config_store.h"

#ifdef ARDUINO

#include <Preferences.h>

#include "src/input/performance_config_save_timer.h"

namespace GroovePuterPlatform {
namespace {

constexpr const char* kNamespace = "gp_perf";
constexpr const char* kConfigKey = "keyboard_v2";
constexpr const char* kLegacyConfigKey = "keyboard_v1";
PerformanceConfigSaveTimer g_saveTimer;
PerformanceKeyboardConfig g_observedConfig{};
bool g_observedConfigValid = false;

}  // namespace

bool saveCardputerPerformanceConfig(const PerformanceKeyboardConfig& config) {
    // Write keyboard_v2 only. keyboard_v1 is left untouched on purpose: a
    // rollback to 0.9.22 then loads the last v1 snapshot it understands.
    uint8_t bytes[PerformanceKeyboardConfigCodec::kV2RecordSize]{};
    if (!PerformanceKeyboardConfigCodec::encodeV2(config, bytes, sizeof(bytes)))
        return false;
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const bool saved = preferences.putBytes(kConfigKey, bytes, sizeof(bytes)) ==
        sizeof(bytes);
    preferences.end();
    return saved;
}

bool loadCardputerPerformanceConfig(PerformanceKeyboardConfig& config) {
    uint8_t v2[PerformanceKeyboardConfigCodec::kV2RecordSize]{};
    uint8_t v1[PerformanceKeyboardConfigCodec::kCurrentRecordSize]{};
    std::size_t v2Size = 0;
    std::size_t v1Size = 0;
    {
        Preferences preferences;
        if (!preferences.begin(kNamespace, true)) return false;
        const std::size_t n2 = preferences.getBytesLength(kConfigKey);
        if (n2 == sizeof(v2) && preferences.getBytes(kConfigKey, v2, n2) == n2)
            v2Size = n2;
        if (v2Size == 0) {
            const std::size_t n1 = preferences.getBytesLength(kLegacyConfigKey);
            const bool sizeSupported =
                n1 == PerformanceKeyboardConfigCodec::kLegacyRecordSize ||
                n1 == PerformanceKeyboardConfigCodec::kLegacyV1RecordSize ||
                n1 == PerformanceKeyboardConfigCodec::kCurrentRecordSize;
            if (sizeSupported &&
                preferences.getBytes(kLegacyConfigKey, v1, n1) == n1)
                v1Size = n1;
        }
        preferences.end();
    }
    const PerformanceKeyboardConfigSource source = resolvePersistedKeyboardConfig(
        v2, v2Size, v1, v1Size, config);
    if (source == PerformanceKeyboardConfigSource::MigratedV1) {
        // One-time best-effort copy into keyboard_v2; v1 stays as rollback.
        (void)saveCardputerPerformanceConfig(config);
    }
    return source != PerformanceKeyboardConfigSource::None;
}

void initializeCardputerPerformanceConfig(PerformanceKeyboard& keyboard) {
    PerformanceKeyboardConfig loaded{};
    if (loadCardputerPerformanceConfig(loaded)) {
        (void)keyboard.applyConfiguration(loaded);
    }
    keyboard.markConfigPersisted();
    g_observedConfig = keyboard.exportConfiguration();
    g_observedConfigValid = true;
    g_saveTimer.saved();
}

void serviceCardputerPerformanceConfig(PerformanceKeyboard& keyboard,
                                      uint32_t nowMs) {
    const PerformanceKeyboardConfig current = keyboard.exportConfiguration();
    if (!g_observedConfigValid ||
        !performanceKeyboardConfigEqual(current, g_observedConfig)) {
        g_observedConfig = current;
        g_observedConfigValid = true;
        g_saveTimer.changed(nowMs);
    }
    if (!keyboard.configChanged()) {
        g_saveTimer.saved();
        return;
    }
    if (!g_saveTimer.due(nowMs)) return;
    if (saveCardputerPerformanceConfig(current)) {
        keyboard.markConfigPersisted();
        g_observedConfig = keyboard.exportConfiguration();
        g_saveTimer.saved();
    } else {
        g_saveTimer.retryAfter(nowMs);
    }
}

}  // namespace GroovePuterPlatform

#endif  // ARDUINO
