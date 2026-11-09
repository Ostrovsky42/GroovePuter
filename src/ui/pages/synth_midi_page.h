#pragma once

#include <string>

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

// MIDI tab of a synth: the external CC74 cutoff of this voice and its sweep.
// Only this page owns the external controls; the TB303 knobs stay on their own
// tab. The page changes no music: it requests values and sweeps from the MIDI
// dispatcher through the voice's shared control channel.
class SynthMidiPage : public IPage {
 public:
  SynthMidiPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard, int voiceIndex);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;

 private:
  void adjust(int direction, bool fine);
  void toggleSweep();

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  int voice_index_;
  std::string title_;
  int focus_ = 0;          // 0 CUTOFF, 1 SWEEP, 2 TARGET
  uint8_t target_ = 110;   // sweep target, held here until the sweep is requested
};
