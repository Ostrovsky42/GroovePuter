#include <cassert>
#include <cstdint>

#include "src/input/performance_config_save_timer.h"
#include "src/input/performance_keyboard.h"

namespace {

PerformanceIntent toggleIntent(PerformanceIntentKind kind, uint8_t value) {
    PerformanceIntent intent{};
    intent.kind = kind;
    intent.value = value;
    return intent;
}

}  // namespace

int main() {
    MusicalEventRouter router;
    PerformanceKeyboard keyboard(router);

    // A fresh keyboard matches the record it would save: nothing to persist.
    assert(!keyboard.configChanged());

    // A MIDI-style switch changes the record and must be seen without any page involved.
    assert(keyboard.dispatchIntent(toggleIntent(PerformanceIntentKind::ToggleArp, 1)));
    assert(keyboard.arpeggiatorEnabled());
    assert(keyboard.configChanged());
    keyboard.markConfigPersisted();
    assert(!keyboard.configChanged());

    // Latch is part of the same record: on is a change, and switching back to the saved state is not.
    assert(keyboard.dispatchIntent(toggleIntent(PerformanceIntentKind::ToggleLatch, 1)));
    assert(keyboard.latchEnabled());
    assert(keyboard.configChanged());
    assert(keyboard.dispatchIntent(toggleIntent(PerformanceIntentKind::ToggleLatch, 0)));
    assert(!keyboard.configChanged());

    // A rejected intent leaves the record alone.
    assert(!keyboard.dispatchIntent(toggleIntent(PerformanceIntentKind::ToggleArp, 2)));
    assert(!keyboard.configChanged());

    // Applying a loaded record is the new persisted baseline, not a change to save.
    PerformanceKeyboard restored(router);
    assert(!restored.configChanged());
    PerformanceKeyboardConfig loaded = keyboard.exportConfiguration();
    loaded.velocity = 90;
    assert(restored.applyConfiguration(loaded));
    assert(!restored.configChanged());

    // Debounce: the first change starts the quiet window, a save is due after kQuietMs unchanged.
    PerformanceConfigSaveTimer timer;
    assert(!timer.due(false, 0));
    assert(!timer.due(true, 100));
    assert(!timer.due(true, 100 + PerformanceConfigSaveTimer::kQuietMs - 1));
    assert(timer.due(true, 100 + PerformanceConfigSaveTimer::kQuietMs));
    timer.saved();
    assert(!timer.due(false, 2000));

    // A failed save waits another quiet window before the next attempt.
    PerformanceConfigSaveTimer retry;
    assert(!retry.due(true, 0));
    assert(retry.due(true, PerformanceConfigSaveTimer::kQuietMs));
    retry.retryAfter(PerformanceConfigSaveTimer::kQuietMs);
    assert(!retry.due(true, PerformanceConfigSaveTimer::kQuietMs + 1));
    assert(retry.due(true, 2 * PerformanceConfigSaveTimer::kQuietMs));
    return 0;
}
