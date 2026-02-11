#pragma once

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
#include "../ui_common.h"

#include <string>

class FeelTexturePage : public IPage {
public:
    FeelTexturePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);

    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;

    const std::string& getTitle() const override { return title_; }
    void setVisualStyle(VisualStyle style) override { style_ = style; }

private:
    MiniAcid& mini_acid_;
    AudioGuard audio_guard_;
    VisualStyle style_ = VisualStyle::MINIMAL;
    std::string title_ = "FEEL";

    enum class FocusArea { FEEL, PRESETS };

    enum class GridResolution : uint8_t {
        Eighth = 8,
        Sixteenth = 16,
        ThirtySecond = 32
    };

    enum class PatternLength : uint8_t {
        OneBar = 1,
        TwoBars = 2,
        FourBars = 4,
        EightBars = 8
    };

    enum class Timebase : uint8_t {
        Half = 0,
        Normal = 1,
        Double = 2
    };

    GridResolution grid_resolution_ = GridResolution::Sixteenth;
    Timebase timebase_ = Timebase::Normal;
    PatternLength pattern_length_ = PatternLength::OneBar;

    FocusArea focus_ = FocusArea::FEEL;
    int feel_row_ = 0;     // 0=GRID, 1=TIMEBASE, 2=LENGTH
    int texture_row_ = 0;  // 0=LOFI, 1=DRIVE, 2=TAPE
    int preset_index_ = 0; // 0..3

    void syncFromScene();
    void applyGridResolution();
    void applyTimebase();
    void applyPatternLength();

    void drawGridSelector(IGfx& gfx, int x, int y);
    void drawTimebaseSelector(IGfx& gfx, int x, int y);
    void drawLengthSelector(IGfx& gfx, int x, int y);
    void drawPresets(IGfx& gfx, int x, int y, int width);
    void drawCursor(IGfx& gfx, int x, int y, int w, int h);
    void applyPreset(int index);
    const char* currentHint() const;

    static const char* gridToString(GridResolution g);
    static const char* lengthToString(PatternLength l);
    static const char* timebaseToString(Timebase t);

    template <typename F>
    void withAudioGuard(F&& fn) {
        if (audio_guard_) audio_guard_(std::forward<F>(fn));
        else fn();
    }

    int maxRowForFocus(FocusArea focus) const;
};
