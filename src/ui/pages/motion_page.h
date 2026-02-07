#pragma once

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"

class MotionPage : public IPage {
public:
  MotionPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string& getTitle() const override;

private:
  enum Row {
    ROW_ENABLED = 0,
    ROW_MASTER,
    ROW_MODE,
    ROW_AXIS,
    ROW_TARGET,
    ROW_VOICE,
    ROW_DEPTH,
    ROW_DEADZONE,
    ROW_SMOOTHING,
    ROW_RATE,
    ROW_THRESHOLD,
    ROW_HOLD,
    ROW_QUANTIZE,
    ROW_INVERT,
    ROW_PRESET,
    ROW_COUNT
  };

  MiniAcid& mini_acid_;
  AudioGuard& audio_guard_;
  int row_ = 0;
  int preset_index_ = 0;
  int first_visible_row_ = 0;
  static constexpr int kVisibleRows = 8;

  void applyPreset(int idx);
  void adjustRow(int delta);
  void ensureRowVisible();
  void withAudioGuard(const std::function<void()>& fn);
};
