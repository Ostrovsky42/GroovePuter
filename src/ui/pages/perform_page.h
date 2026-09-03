#pragma once

#include <cstdint>
#include <string>

#include "../ui_common.h"
#include "../workflow_mode.h"
#include "src/input/performance_keyboard.h"

enum class PerformanceToolContext : uint8_t {
    Key = 0,
    Chord,
    Arp,
    Rhythm,
    Count,
};

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
    void moveContext(int direction);
    void moveRow(int direction);
    void adjustSelectedValue(int direction);
    void toggleSelectedValue();
    uint8_t rowCountForContext() const;
    void drawToolsLayer(IGfx& gfx);

    MiniAcid& miniAcid_;
    PerformanceKeyboard& keyboard_;
    bool toolsLayerVisible_{false};
    PerformanceToolContext selectedContext_{PerformanceToolContext::Key};
    uint8_t selectedRow_{0};
    std::string title_{"PERFORM"};
};
