#pragma once

#include <cstdint>

namespace GroovePuterMidi {

// Selection is local to the panel. It never reads GEN LENGTH or GEN TARGET.
struct SmfGrabPanelState {
    uint32_t fromBar{1};
    uint32_t totalBars{1};
    uint32_t loopBars{0};
    uint8_t lengthChoice{2};  // 1, 2, 4, 8, LOOP
    uint8_t toVoice{0};       // Synth A or B
    uint8_t focus{0};         // FROM, LEN, TO

    static SmfGrabPanelState open(uint32_t currentBar, uint32_t total,
                                  bool sectionLoop, uint32_t loopStart,
                                  uint32_t loopEnd) {
        SmfGrabPanelState state{};
        state.totalBars = total > 0 ? total : 1;
        const bool usableLoop = sectionLoop && loopStart > 0 &&
                                loopEnd >= loopStart && loopEnd <= state.totalBars;
        if (usableLoop) {
            state.fromBar = loopStart;
            state.loopBars = loopEnd - loopStart + 1;
            state.lengthChoice = 4;
        } else {
            state.fromBar = currentBar < 1 ? 1
                            : currentBar > state.totalBars ? state.totalBars
                                                            : currentBar;
            const uint32_t remaining = state.totalBars - state.fromBar + 1;
            state.lengthChoice = remaining >= 4 ? 2 : remaining >= 2 ? 1 : 0;
        }
        return state;
    }

    uint32_t lengthBars() const {
        constexpr uint8_t lengths[4] = {1, 2, 4, 8};
        return lengthChoice == 4 ? loopBars : lengths[lengthChoice];
    }

    uint32_t endBar() const { return fromBar + lengthBars() - 1; }

    bool valid() const {
        const uint32_t length = lengthBars();
        return length >= 1 && length <= 8 && fromBar >= 1 &&
               fromBar <= totalBars && length <= totalBars - fromBar + 1;
    }

    void moveFocus(int direction) {
        focus = static_cast<uint8_t>((focus + (direction > 0 ? 1 : 2)) % 3);
    }

    void adjust(int direction) {
        if (focus == 2) {
            toVoice = static_cast<uint8_t>(1 - toVoice);
            return;
        }
        if (focus == 1) {
            const uint8_t choices = loopBars > 0 ? 5 : 4;
            lengthChoice = static_cast<uint8_t>(
                (lengthChoice + choices + (direction > 0 ? 1 : -1)) % choices);
            const uint32_t length = lengthBars();
            if (length <= totalBars && fromBar > totalBars - length + 1) {
                fromBar = totalBars - length + 1;
            }
            return;
        }
        const uint32_t length = lengthBars();
        const uint32_t maxFrom = length <= totalBars
            ? totalBars - length + 1 : totalBars;
        if (direction > 0 && fromBar < maxFrom) ++fromBar;
        if (direction < 0 && fromBar > 1) --fromBar;
    }
};

}  // namespace GroovePuterMidi
