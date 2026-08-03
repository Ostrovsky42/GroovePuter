#pragma once
#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
#include "src/state/scene_revision.h"

class ModePage : public IPage {
public:
    ModePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
    
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const;

private:
    enum class FocusRow : uint8_t {
        Mode = 0,
        Flavor = 1,
        PhraseLength = 2,
        GeneratePhrase = 3,
        Macros = 4
    };

    void toggleMode();
    void shiftMode(int delta);
    void shiftFlavor(int delta);
    void shiftPhraseLength(int delta);
    void generatePhrase();
    void applyTo303(int voiceIdx);
    void applyToDrums();
    void previewMode();
    void toggleMacros();
    void moveFocus(int delta);
    void drawRow(IGfx& gfx, int y, const char* label, const char* value, bool focused, IGfxColor accent);
    const char* modeName(GrooveboxMode mode) const;
    const char* flavorName(GrooveboxMode mode, int flavor) const;
    template <typename F>
    void withAudioGuard(F&& fn) {
        if (audio_guard_) audio_guard_(std::forward<F>(fn));
        else fn();
        GroovePuterState::markSceneMutated();
    }

    MiniAcid& mini_acid_;
    AudioGuard audio_guard_;
    FocusRow focus_ = FocusRow::Mode;
    uint8_t phrase_bars_ = 4;
    std::string title_ = "GROOVE LAB";
};
