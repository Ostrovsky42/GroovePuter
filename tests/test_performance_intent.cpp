#include <cassert>
#include <cstdint>
#include <type_traits>

#include "src/input/performance_intent.h"

int main() {
    static_assert(std::is_trivially_copyable<PerformanceIntent>::value,
                  "performance intents cross bounded realtime queues");
    static_assert(sizeof(PerformanceIntent) <= 24,
                  "all performance domains need one bounded intent envelope");
    static_assert(sizeof(PerformanceInputIdentity) <= 8,
                  "input identity must stay compact");

    const PerformanceInputIdentity usbSession1{7, 1, 0, 60};
    const PerformanceInputIdentity uartSession1{7, 2, 0, 60};
    const PerformanceInputIdentity usbSession2{8, 1, 0, 60};
    const PerformanceInputIdentity usbOtherChannel{7, 1, 1, 60};
    const PerformanceInputIdentity usbOtherNote{7, 1, 0, 61};

    assert(samePerformanceInput(usbSession1, usbSession1));
    assert(!samePerformanceInput(usbSession1, uartSession1));
    assert(!samePerformanceInput(usbSession1, usbSession2));
    assert(!samePerformanceInput(usbSession1, usbOtherChannel));
    assert(!samePerformanceInput(usbSession1, usbOtherNote));

    const PerformanceIntent noteOn{
        PerformanceIntentKind::PlayNote, usbSession1, 60, 100};
    const PerformanceIntent noteOff{
        PerformanceIntentKind::ReleaseNote, usbSession1, 60, 0};
    assert(noteOn.kind == PerformanceIntentKind::PlayNote);
    assert(samePerformanceInput(noteOn.input, noteOff.input));
    assert(noteOn.note == noteOff.note && noteOn.velocity == 100);

    const PerformanceIntent cardputerFilter = makeControlPerformanceIntent(
        usbSession1, 0, 0, 91, 0);
    const PerformanceIntent midiFilter = makeControlPerformanceIntent(
        uartSession1, 0, 0, 91, 0);
    assert(cardputerFilter.kind == PerformanceIntentKind::ChangeControl);
    assert(samePerformanceIntent(cardputerFilter, midiFilter));

    const PerformanceIntent recallFromShortcut = makeScenePerformanceIntent(
        PerformanceIntentKind::RecallScene, usbSession1, 2, 1);
    const PerformanceIntent recallFromPad = makeScenePerformanceIntent(
        PerformanceIntentKind::RecallScene, uartSession1, 2, 1);
    assert(samePerformanceIntent(recallFromShortcut, recallFromPad));

    const PerformanceIntent bassRole = makeSelectionPerformanceIntent(
        PerformanceIntentKind::SelectRole, usbSession1, 1);
    assert(bassRole.kind == PerformanceIntentKind::SelectRole);
    assert(bassRole.value == 1);
}
