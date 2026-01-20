#pragma once

#include "../ui_core.h"

class DrumAutomationLaneEditor : public FocusableComponent {
 public:
  DrumAutomationLaneEditor(MiniAcid& mini_acid, AudioGuard& audio_guard);

  void setParamId(DrumAutomationParamId id);
  DrumAutomationParamId paramId() const { return param_id_; }

  bool handleEvent(UIEvent& ui_event) override;
  void draw(IGfx& gfx) override;

 private:
  static constexpr int kDefaultYSteps = 32;
  static constexpr int kXSteps = kAutomationMaxX + 1;
  static constexpr int kGraphPadding = 3;

  void withAudioGuard(const std::function<void()>& fn);
  void clampCursor();
  void setCursorFromPoint(int x, int y);
  uint8_t cursorValue() const;
  int valueToYIndex(uint8_t value) const;
  int ySteps() const;
  Rect graphBounds() const;
  int xToPixel(int x) const;
  int yIndexToPixel(int yIndex) const;
  bool toggleNodeAtCursor();
  bool removeNodeAtCursor();
  bool addNodeAtCursor();

  MiniAcid& mini_acid_;
  AudioGuard& audio_guard_;
  DrumAutomationParamId param_id_;
  int cursor_x_;
  int cursor_y_;
};
