#include "cardputer_performance_config_owner.h"

#include "cardputer_performance_scene_store.h"
#include "src/input/performance_config_save_timer.h"
#include "src/input/performance_keyboard.h"

namespace GroovePuterPlatform {

namespace {
PerformanceConfigSaveTimer g_configSaveTimer;
}  // namespace

void restoreCardputerPerformanceConfig(PerformanceKeyboard& keyboard) {
    PerformanceKeyboardConfig config{};
    if (loadCardputerPerformanceConfig(config)) (void)keyboard.applyConfiguration(config);
}

void servicePerformanceConfigSave(PerformanceKeyboard& keyboard, uint32_t nowMs) {
    if (!g_configSaveTimer.due(keyboard.configChanged(), nowMs)) return;
    if (saveCardputerPerformanceConfig(keyboard.exportConfiguration())) {
        keyboard.markConfigPersisted();
        g_configSaveTimer.saved();
    } else {
        g_configSaveTimer.retryAfter(nowMs);
    }
}

}  // namespace GroovePuterPlatform
