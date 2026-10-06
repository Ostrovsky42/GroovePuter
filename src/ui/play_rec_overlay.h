#pragma once

#include "display.h"
#include "ui_core.h"

class MiniAcid;

// Commands are applied by the display owner through the existing player
// transport handlers. This view never owns audio or external recording.
class PlayRecOverlay {
public:
    enum class Action { None, GrooveTransport, MidiTransport, Sync, Player };
    bool isVisible() const { return visible_; }
    void open(bool playerContext) { midiSelected_ = playerContext; visible_ = true; }
    void close() { visible_ = false; action_ = Action::None; }
    bool handleEvent(const UIEvent& event);
    Action takeAction() { const auto action = action_; action_ = Action::None; return action; }
    void draw(IGfx& gfx, const MiniAcid& engine) const;
private:
    bool visible_{false};
    bool midiSelected_{false};
    Action action_{Action::None};
};
