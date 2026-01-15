#pragma once

#include "../ui_core.h"
#include "../../dsp/miniacid_engine.h"

class SettingsPage : public IPage {
public:
    SettingsPage(IGfx& gfx, MiniAcid& mini_acid);
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const override;

private:
    IGfx& gfx_;
    MiniAcid& mini_acid_;
    int selection_index_ = 0;
    
    // Ordered list of parameters to edit
    enum class SettingId {
        Swing = 0,
        VelocityRange,
        GhostProb,
        MicroTiming,
        MinNotes,
        MaxNotes,
        MinOctave,
        MaxOctave,
        ScaleRoot,
        ScaleType,
        PreferDownbeats,
        ScaleQuantize,
        Count
    };

    void drawSlider(IGfx& gfx, int x, int y, int w, int h, float value, const char* label, bool selected);
    void drawToggle(IGfx& gfx, int x, int y, int w, int h, bool value, const char* label, bool selected);
    void drawValue(IGfx& gfx, int x, int y, int w, int h, const char* valueStr, const char* label, bool selected);
    
    void adjustSetting(int delta);
};
