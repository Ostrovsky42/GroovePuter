#pragma once

#include "display.h"
#include "ui_core.h"

class MiniAcid;

class GlobalMidiSyncOverlay {
public:
    bool isVisible() const { return visible_; }

    void open() { selectedRow_ = 0; tempoDelta_ = 0; visible_ = true; }
    void close() { tempoDelta_ = 0; visible_ = false; }
    bool handleEvent(UIEvent& event);
    void draw(IGfx& gfx, const MiniAcid& miniAcid) const;
    int takeTempoDelta() { const int delta = tempoDelta_; tempoDelta_ = 0; return delta; }

private:
    bool visible_{false};
    uint8_t selectedRow_{0};
    int tempoDelta_{0};
};
