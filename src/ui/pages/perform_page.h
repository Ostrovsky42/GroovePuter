#pragma once

#include <cstdint>
#include <string>

#include "../ui_common.h"
#include "../ui_view_continuity.h"
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
    void captureViewContinuity(UI::UiViewContinuityState& state) const override {
        state.performToolsVisible = toolsLayerVisible_ ? 1 : 0;
        state.performContext = static_cast<uint8_t>(selectedContext_);
        for (int i = 0; i < static_cast<int>(PerformanceToolContext::Count); ++i) {
            state.performRows[i] = selectedRow_[i];
        }
    }

    void restoreViewContinuity(const UI::UiViewContinuityState& state) override {
        toolsLayerVisible_ = state.performToolsVisible != 0;
        uint8_t context = state.performContext;
        if (context >= static_cast<uint8_t>(PerformanceToolContext::Count)) context = 0;
        selectedContext_ = static_cast<PerformanceToolContext>(context);
        constexpr uint8_t kMaxRowByContext[4] = {5, 4, 3, 5};
        for (int i = 0; i < static_cast<int>(PerformanceToolContext::Count); ++i) {
            selectedRow_[i] = state.performRows[i] <= kMaxRowByContext[i]
                ? state.performRows[i]
                : 0;
        }
    }

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
