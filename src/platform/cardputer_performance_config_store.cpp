#include "cardputer_performance_config_store.h"

#ifdef ARDUINO

#include <Preferences.h>

#include "src/input/performance_config_save_timer.h"

namespace GroovePuterPlatform {
namespace {

constexpr const char* kNamespace = "gp_perf";
constexpr const char* kConfigKey = "keyboard_v1";
PerformanceConfigSaveTimer g_saveTimer;
PerformanceKeyboardConfig g_observedConfig{};
bool g_observedConfigValid = false;

}  // namespace

bool loadCardputerPerformanceConfig(PerformanceKeyboardConfig& config) {
    Preferences preferences;
    if (!preferences.begin(kNamespace, true)) return false;
    const std::size_t storedSize = preferences.getBytesLength(kConfigKey);
    uint8_t bytes[PerformanceKeyboardConfigCodec::kCurrentRecordSize]{};
    const bool sizeSupported =
        storedSize == PerformanceKeyboardConfigCodec::kLegacyRecordSize ||
        storedSize == PerformanceKeyboardConfigCodec::kLegacyV1RecordSize ||
        storedSize == PerformanceKeyboardConfigCodec::kCurrentRecordSize;
    const bool read = sizeSupported &&
        preferences.getBytes(kConfigKey, bytes, storedSize) == storedSize;
    preferences.end();
    return read && PerformanceKeyboardConfigCodec::decode(
        bytes, storedSize, config);
}

bool saveCardputerPerformanceConfig(const PerformanceKeyboardConfig& config) {
    uint8_t bytes[PerformanceKeyboardConfigCodec::kCurrentRecordSize]{};
    if (!PerformanceKeyboardConfigCodec::encode(config, bytes, sizeof(bytes)))
        return false;
    Preferences preferences;
    if (!preferences.begin(kNamespace, false)) return false;
    const bool saved = preferences.putBytes(kConfigKey, bytes, sizeof(bytes)) ==
        sizeof(bytes);
    preferences.end();
    return saved;
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
