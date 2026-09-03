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
    uint8_t currentRow() const;
    const char* selectedRowHint() const;
    void drawToolTabs(IGfx& gfx, int y);
    void drawToolsLayer(IGfx& gfx);

    MiniAcid& miniAcid_;
    PerformanceKeyboard& keyboard_;
    bool toolsLayerVisible_{false};
    // Local navigation state only: the selected context and, per context, the
    // selected row. Musical values are always read from PerformanceKeyboard.
    PerformanceToolContext selectedContext_{PerformanceToolContext::Key};
    uint8_t selectedRow_[static_cast<int>(PerformanceToolContext::Count)]{};
    std::string title_{"PERFORM"};
};
