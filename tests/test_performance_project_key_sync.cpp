// Project key sync must not mark the keyboard NVS record dirty.
#include <cassert>

#include "src/input/performance_keyboard.h"

int main() {
    MusicalEventRouter router;
    PerformanceKeyboard kb(router);
    kb.markConfigPersisted();
    assert(!kb.configChanged());

    // Scene key differs from the stored one: sync, nothing to persist.
    kb.syncProjectKey(7, PerformanceScale::Major);
    assert(kb.rootPitchClass() == 7);
    assert(kb.scale() == PerformanceScale::Major);
    assert(!kb.configChanged());

    // Repeating the same values changes nothing.
    kb.syncProjectKey(7, PerformanceScale::Major);
    assert(kb.rootPitchClass() == 7);
    assert(kb.scale() == PerformanceScale::Major);
    assert(!kb.configChanged());

    // Out-of-range scale is rejected, state untouched.
    kb.syncProjectKey(3, PerformanceScale::Count);
    assert(kb.rootPitchClass() == 7);
    assert(!kb.configChanged());

    // A real edit made before a sync stays dirty and is exported with the new key.
    const uint8_t ratchetBefore = kb.ratchetCount();
    kb.cycleRatchet();
    assert(kb.ratchetCount() != ratchetBefore);
    assert(kb.configChanged());
    kb.syncProjectKey(2, PerformanceScale::Dorian);
    assert(kb.configChanged());
    const PerformanceKeyboardConfig cfg = kb.exportConfiguration();
    assert(cfg.rootPitchClass == 2 && cfg.scale == PerformanceScale::Dorian);
    assert(cfg.ratchetCount != ratchetBefore);

    // After persisting, the baseline is clean again and a key sync stays clean.
    kb.markConfigPersisted();
    assert(!kb.configChanged());
    kb.syncProjectKey(9, PerformanceScale::NaturalMinor);
    assert(!kb.configChanged());

    // A user edit of the key on the keyboard itself is still a persisted change.
    kb.cycleRoot(1);
    assert(kb.configChanged());
    return 0;
}
