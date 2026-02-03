#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

/**
 * VoicePage - Formant Vocal Synthesizer Control
 * 
 * Provides UI for controlling the robotic voice synthesizer:
 * - Select built-in or custom phrases
 * - Adjust pitch, speed, robotness
 * - Preview and trigger voice announcements
 * - Manage custom phrases (saved with scene)
 */
class VoicePage : public IPage {
 public:
  VoicePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;
  void setBoundaries(const Rect& rect) override;
  
  template <typename F>
  void withAudioGuard(F&& fn) {
      if (audio_guard_) audio_guard_(std::forward<F>(fn));
      else fn();
  }

 private:
  enum class FocusItem {
    PhraseType,
    PhraseIndex,
    Pitch,
    Speed,
    Robotness,
    Volume,
    CustomEdit,
    Count
  };
  
  void drawPhraseSection(IGfx& gfx, int y);
  void drawParameterSection(IGfx& gfx, int y);
  void drawCustomPhraseSection(IGfx& gfx, int y);
  void adjustCurrentValue(int delta);
  void triggerPreview();
  void nextFocus();
  void prevFocus();
  
  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  
  FocusItem focus_ = FocusItem::PhraseType;
  
  // Phrase selection
  bool useCustomPhrase_ = false;  // false = built-in, true = custom
  int phraseIndex_ = 0;           // Index into built-in or custom phrases
  
  // Edit state for custom phrases
  bool editingCustom_ = false;
  int editCursor_ = 0;
  char editBuffer_[32] = "";
  
  std::string title_ = "VOICE SYNTH";
};
