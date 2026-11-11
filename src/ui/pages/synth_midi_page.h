#pragma once

#include <string>

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "../selection_popup.h"

// MIDI tab of a synth: one selected SEQTRAK controller of this voice, its value,
// and a four-bar sweep (RAMP or RETURN). The page changes no music: it requests
// values and sweeps from the MIDI dispatcher through the voice's control channel.
class SynthMidiPage : public IPage {
 public:
  SynthMidiPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard, int voiceIndex);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;
  // True while the controller list is open. The synth page sends every key to
  // this page while it is, so Tab, Backspace, GO and Cancel cannot take them.
  bool popupOpen() const { return popup_.isOpen(); }

 private:
  void adjust(int direction, bool fine);
  void toggleSweep();
  void toggleShape();
  void adjustLength(int direction);
  void openControllerList();
  void selectController(uint8_t index);

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int voice_index_;
  std::string title_;
  int focus_ = 0;          // 0 controller, 1 SWEEP, 2 TARGET
  uint8_t target_ = 110;   // sweep target, held here until the sweep is requested
  int requested_ = -1;     // last value this page requested, -1 when caught up
  UI::SelectionPopup popup_;
};
