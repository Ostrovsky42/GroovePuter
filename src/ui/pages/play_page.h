#pragma once

#include "../ui_core.h"
#include "../../dsp/miniacid_engine.h"
#include "../layout_manager.h"
#include "../ui_widgets.h"

class PlayPage : public IPage {
public:
    enum class Mode { OVERVIEW, DETAIL };
    
    PlayPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
    
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const override { return title_; }
    
private:
    MiniAcid& mini_acid_;
    AudioGuard audio_guard_;
    std::string title_ = "PLAY";
    Mode mode_ = Mode::OVERVIEW;
    int selectedTrack_ = 0; // 0-4: BASS, LEAD, KICK, SNARE, HAT
    
    void drawOverview(IGfx& gfx);
    void drawDetail(IGfx& gfx);
    template <typename F>
    void withAudioGuard(F&& fn) {
      if (audio_guard_) audio_guard_(std::forward<F>(fn));
      else fn();
    }
    
    // Получение данных из движка
    uint16_t getStepMask(int track) const;
    const char* getTrackName(int track) const;
    int getCurrentStep() const;
    int getSwingPercent() const;
    char getBankChar() const;
    
    // Навигация
    void togglePlayback();
    void nextTrack();
    void prevTrack();
};
