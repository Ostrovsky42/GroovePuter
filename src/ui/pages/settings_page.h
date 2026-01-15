#pragma once

#include "../ui_core.h"
#include "../../dsp/miniacid_engine.h"

#include "../components/scroll_list.h"

// Define types for table-driven menu
enum class SettingUIType : uint8_t { Slider01, Toggle, IntRange, EnumScale, EnumNote };

struct SettingDesc {
  int id; // Cast to specific enum in implementation
  const char* label;
  SettingUIType type;
};

class SettingsPage : public IPage {
public:
    SettingsPage(IGfx& gfx, MiniAcid& mini_acid);
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const override;

private:
    IGfx& gfx_;
    MiniAcid& mini_acid_;
    
    // Use the new ScrollList component
    ScrollList list_;

    // Helper drawing methods
    void drawSettingRow(IGfx& gfx, int x, int y, int w, GeneratorParams& params, const SettingDesc& d, bool selected);
    void drawScrollbar(IGfx& gfx, const Rect& b, int scroll, int sel, int total, int vis);
    
    // Existing drawing primitives
    void drawSlider(IGfx& gfx, int x, int y, int w, int h, float value, const char* label, bool selected);
    void drawToggle(IGfx& gfx, int x, int y, int w, int h, bool value, const char* label, bool selected);
    void drawValue(IGfx& gfx, int x, int y, int w, int h, const char* valueStr, const char* label, bool selected);
    
    void adjustSetting(int delta, bool shift);
};
