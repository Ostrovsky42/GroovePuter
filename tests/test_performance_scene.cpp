#include <cassert>
#include "src/input/performance_scene.h"

int main() {
    using namespace GroovePuterPerformance;
    PerformanceSceneBank bank;
    PerformanceSceneSnapshot a{};
    PerformanceSceneSnapshot b{};
    PerformanceSceneSnapshot applied{};
    a.controls.set(PerformanceControlId::Filter, 10);
    a.controls.set(PerformanceControlId::Density, 80);
    a.muteMask = 1;
    a.effectMask = 1;
    b.controls.set(PerformanceControlId::Filter, 110);
    b.controls.set(PerformanceControlId::Density, 20);
    b.muteMask = 2;
    b.effectMask = 2;
    assert(bank.capture(0, a));
    assert(bank.capture(1, b));
    uint8_t persisted[PerformanceSceneBank::kSerializedSize]{};
    assert(bank.serialize(persisted, sizeof(persisted)));
    PerformanceSceneBank restored;
    assert(restored.deserialize(persisted, sizeof(persisted)));
    assert(restored.scene(1, applied));
    assert(applied.controls.get(PerformanceControlId::Filter) == 110 && applied.muteMask == 2);
    persisted[5] ^= 0x01u;
    assert(!restored.deserialize(persisted, sizeof(persisted)));

    assert(bank.queueRecall(0));
    assert(bank.onBarBoundary(applied));
    assert(applied.controls.get(PerformanceControlId::Filter) == 10);
    assert(bank.activeSlot() == 0 && bank.queuedSlot() == -1);

    assert(bank.beginMorph(a, 1, SceneTransition::TwoBars, 1000));
    assert(bank.queuedSlot() == 1); // The destination remains visible while queued/morphing.
    assert(bank.updateMorph(999, applied));
    assert(bank.morphing()); // A late UI poll cannot complete a future transition early.
    assert(applied.controls.get(PerformanceControlId::Filter) == 10);
    assert(bank.updateMorph(1000 + PerformanceSceneBank::kTicksPerBar, applied));
    assert(applied.controls.get(PerformanceControlId::Filter) == 60);
    assert(applied.controls.get(PerformanceControlId::Density) == 80); // Discrete/role state held.
    assert(applied.muteMask == 1); // Mutes switch only at completion.
    assert(applied.effectMask == 1); // Effect enable bits are discrete too.
    assert(bank.updateMorph(1000 + 2 * PerformanceSceneBank::kTicksPerBar, applied));
    assert(applied.controls.get(PerformanceControlId::Filter) == 110);
    assert(applied.controls.get(PerformanceControlId::Density) == 20);
    assert(applied.muteMask == 2);
    assert(applied.effectMask == 2);
    assert(!bank.morphing() && bank.activeSlot() == 1 && !bank.dirty());
    assert(bank.queuedSlot() == -1);

    assert(bank.beginMorph(b, 0, SceneTransition::FourBars, 5000));
    assert(bank.queuedSlot() == 0);
    assert(bank.updateMorph(5000 + PerformanceSceneBank::kTicksPerBar, applied));
    assert(applied.controls.get(PerformanceControlId::Filter) == 85);
    assert(bank.finishMorph(applied)); // STOP policy completes the requested scene.
    assert(applied.controls.get(PerformanceControlId::Filter) == 10);
    assert(!bank.morphing() && bank.activeSlot() == 0 && bank.queuedSlot() == -1);

    assert(bank.beginMorph(a, 1, SceneTransition::EightBars, 9000));
    bank.cancelMorph(); // Engine command rejection or explicit cancellation cannot leave UI stuck.
    assert(!bank.morphing() && bank.queuedSlot() == -1 && bank.activeSlot() == 0);

    bank.markCurrentState(b);
    assert(bank.dirty());
    assert(!bank.capture(8, a));
    return 0;
}
