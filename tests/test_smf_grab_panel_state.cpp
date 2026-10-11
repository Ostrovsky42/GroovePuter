#include <cassert>
#include <cstdio>

#include "src/ui/pages/smf_grab_panel_state.h"

int main() {
    using namespace GroovePuterMidi;
    auto panel = SmfGrabPanelState::open(5, 20, true, 3, 6);
    assert(panel.fromBar == 3);
    assert(panel.lengthBars() == 4);
    assert(panel.toVoice == 0);
    assert(panel.endBar() == 6);
    panel.adjust(1);  // FROM is independent of the A-B marks.
    assert(panel.fromBar == 4 && panel.endBar() == 7);
    panel.moveFocus(1);
    panel.adjust(1);  // LOOP -> 1 bar.
    assert(panel.lengthBars() == 1 && panel.fromBar == 4);
    panel.moveFocus(1);
    panel.adjust(1);
    assert(panel.toVoice == 1);

    auto noLoop = SmfGrabPanelState::open(19, 20, false, 0, 0);
    assert(noLoop.fromBar == 19);
    assert(noLoop.lengthBars() == 2);
    noLoop.moveFocus(1);
    noLoop.adjust(1);  // 4 bars moves FROM to fit within the file.
    assert(noLoop.lengthBars() == 4 && noLoop.fromBar == 17);
    noLoop.adjust(1);
    assert(noLoop.lengthBars() == 8 && noLoop.fromBar == 13);
    noLoop.adjust(1);  // No LOOP choice without an A-B section.
    assert(noLoop.lengthBars() == 1);

    auto tooLong = SmfGrabPanelState::open(1, 20, true, 2, 10);
    assert(!tooLong.valid());
    tooLong.moveFocus(1);
    tooLong.adjust(1);
    assert(tooLong.valid() && tooLong.lengthBars() == 1);
    std::puts("SMF GRAB panel state: PASS");
}
