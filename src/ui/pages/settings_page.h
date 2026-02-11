#pragma once

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"

class SettingsPage : public IPage {
public:
    SettingsPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard);
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const override;

private:
    MiniAcid& mini_acid_;
    AudioGuard& audio_guard_;

    enum class Group { Timing, Notes, Scale };
    Group group_ = Group::Timing;
    int row_ = 0;
    int preset_index_ = 0;

    void formatSetting(char* buf, size_t bufSize, int id, const GeneratorParams& params) const;
    int settingForRow() const;
    const char* hintText() const;
    const char* const* commentLines(int& count, const GeneratorParams& params) const;
    void adjustSetting(int delta, bool shift);
    void applyPreset(int index);
    void measureSDPerformance();
    
    template <typename F>
    void withAudioGuard(F&& fn) {
        if (audio_guard_) audio_guard_(std::forward<F>(fn));
        else fn();
    }
};
