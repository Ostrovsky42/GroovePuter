#pragma once

#include <string>

#include "../ui_common.h"
#include "../workflow_mode.h"
#include "src/input/performance_keyboard.h"

class PerformPage final : public IPage {
public:
    PerformPage(IGfx& gfx,
                MiniAcid& miniAcid,
                PerformanceKeyboard& keyboard);

    const std::string& getTitle() const override { return title_; }
    bool handleEvent(UIEvent& event) override;
    void drawHeader(IGfx& gfx) override;
    void drawContent(IGfx& gfx) override;
    void drawFooter(IGfx& gfx) override;

private:
    static const char* noteName(int midiNote);
    bool handleToolKey(const UIEvent& event);
    void drawToolsLayer(IGfx& gfx);

    MiniAcid& miniAcid_;
    PerformanceKeyboard& keyboard_;
    bool toolsLayerVisible_{false};
    std::string title_{"PERFORM"};
};
