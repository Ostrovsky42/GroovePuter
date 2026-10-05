#pragma once

#include "display.h"
#include "ui_core.h"

class MiniAcid;

class GlobalMidiSyncOverlay {
public:
    bool isVisible() const { return visible_; }

    void open() { selectedRow_ = 0; visible_ = true; }
    void close() { visible_ = false; }
    bool handleEvent(UIEvent& event);
    void draw(IGfx& gfx, const MiniAcid& miniAcid) const;

private:
    bool visible_{false};
    uint8_t selectedRow_{0};
};
