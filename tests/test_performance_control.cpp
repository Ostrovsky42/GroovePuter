#include <cassert>
#include <cstdint>

#include "src/input/performance_control.h"
#include "src/input/performance_intent.h"

int main() {
    using namespace GroovePuterPerformance;
    PerformanceControlState state{};
    assert(state.get(PerformanceControlId::Filter) == 96);
    assert(applyControlIntent(state, PerformanceControlIntent{
        PerformanceControlId::Filter, ControlChangeMode::Absolute, 24}));
    assert(state.get(PerformanceControlId::Filter) == 24);
    assert(applyControlIntent(state, PerformanceControlIntent{
        PerformanceControlId::Filter, ControlChangeMode::Relative, 0, -40}));
    assert(state.get(PerformanceControlId::Filter) == 0);
    assert(applyControlIntent(state, PerformanceControlIntent{
        PerformanceControlId::Level, ControlChangeMode::Relative, 0, 200}));
    assert(state.get(PerformanceControlId::Level) == 127);

    PerformanceGestureOverlay gestures;
    assert(gestures.begin(PerformanceGestureKind::DelayThrow, state, 1000, 500));
    assert(gestures.value(PerformanceControlId::DelaySend, state, 1200) == 127);
    const PerformanceControlState duringThrow = gestures.effectiveState(state, 1200);
    assert(duringThrow.get(PerformanceControlId::DelaySend) == 127);
    assert(duringThrow.get(PerformanceControlId::Filter) == state.get(PerformanceControlId::Filter));
    assert(state.get(PerformanceControlId::DelaySend) == 32); // transient overlay never edits base.
    assert(gestures.value(PerformanceControlId::DelaySend, state, 1600) == 32); // bounded cleanup.
    assert(!gestures.active(PerformanceGestureKind::DelayThrow));
    const PerformanceControlState afterThrow = gestures.effectiveState(state, 1600);
    assert(afterThrow.get(PerformanceControlId::DelaySend) == 32);
    assert(gestures.begin(PerformanceGestureKind::ReverbBloom, state, 2000, 0));
    assert(gestures.value(PerformanceControlId::ReverbSend, state, 2001) == 127);
    assert(gestures.release(PerformanceGestureKind::ReverbBloom));
    assert(gestures.value(PerformanceControlId::ReverbSend, state, 2002) == 0);
    assert(!gestures.begin(PerformanceGestureKind::Break, state, 2100));
    assert(!gestures.active(PerformanceGestureKind::Break));
    assert(gestures.begin(PerformanceGestureKind::FilterPush, state, 2200, 0));
    assert(gestures.begin(PerformanceGestureKind::FilterClose, state, 2200, 0));
    const uint8_t gestureMask = gestures.activeMask(2201);
    const PerformanceControlState sceneDuringGesture =
        PerformanceGestureOverlay::effectiveStateForMask(state, gestureMask);
    assert(sceneDuringGesture.get(PerformanceControlId::Filter) == 8u);
    assert((gestureMask & (1u << static_cast<uint8_t>(
        PerformanceGestureKind::DelayThrow))) == 0u);
    assert(gestures.release(PerformanceGestureKind::FilterPush));
    assert(gestures.release(PerformanceGestureKind::FilterClose));
    PerformanceGestureEvent unsupportedGesture{};
    const PerformanceIntent breakIntent = makeGesturePerformanceIntent(
        PerformanceInputIdentity{},
        static_cast<uint8_t>(PerformanceGestureKind::Break), true, 0);
    assert(!fromPerformanceIntent(breakIntent, unsupportedGesture));
    PerformanceCcBinding binding{};
    PerformanceControlIntent ccIntent{};
    assert(genericMidiCcBinding(74, binding));
    assert(mapCcToControl(binding, 74, 81, ccIntent));
    assert(ccIntent.control == PerformanceControlId::Filter && ccIntent.value == 81);
    assert(!genericMidiCcBinding(23, binding));
    PerformanceIntentKind toggle{};
    assert(genericMidiToggleCc(80, toggle) && toggle == PerformanceIntentKind::ToggleArp);
    assert(genericMidiToggleCc(81, toggle) && toggle == PerformanceIntentKind::ToggleLatch);
    assert(!genericMidiToggleCc(74, toggle));
    // The switch CCs must not also be continuous controls, or one message would do two things.
    assert(!genericMidiCcBinding(80, binding) && !genericMidiCcBinding(81, binding));
    const PerformanceIntent armArp = makeSelectionPerformanceIntent(
        PerformanceIntentKind::ToggleArp, PerformanceInputIdentity{}, 1u);
    assert(armArp.kind == PerformanceIntentKind::ToggleArp && armArp.value == 1u);

    const PerformanceInputIdentity cardputer{1, 0, 0, 0};
    const PerformanceInputIdentity midi{4, 1, 0, 0};
    const PerformanceControlIntent cardputerControl{
        PerformanceControlId::Filter, ControlChangeMode::Absolute, 91, 0};
    const PerformanceIntent cardputerIntent = toPerformanceIntent(
        cardputerControl, cardputer);
    const PerformanceControlIntent midiControl{
        PerformanceControlId::Filter, ControlChangeMode::Absolute, 91, 0};
    const PerformanceIntent midiIntent = toPerformanceIntent(midiControl, midi);
    assert(samePerformanceIntent(cardputerIntent, midiIntent));
    PerformanceControlIntent restored{};
    assert(fromPerformanceIntent(cardputerIntent, restored));
    assert(restored.control == cardputerControl.control &&
           restored.mode == cardputerControl.mode &&
           restored.value == cardputerControl.value);

    const PerformanceIntent gestureIntent = makeGesturePerformanceIntent(
        cardputer, static_cast<uint8_t>(PerformanceGestureKind::DelayThrow), true, 750);
    assert(gestureIntent.kind == PerformanceIntentKind::TriggerPerformanceGesture);
    PerformanceGestureEvent restoredGesture{};
    assert(fromPerformanceIntent(gestureIntent, restoredGesture));
    assert(restoredGesture.gesture == PerformanceGestureKind::DelayThrow);
    return 0;
}
