#pragma once
#include "dsp_engine.h"
#include "display.h"

class MiniAcidDisplay {
public:
  MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid);
  ~MiniAcidDisplay();
  void update();
  void nextPage();
  void previousPage();
  int active303Voice() const;
  bool is303ControlPage() const;
  bool isPatternEditPage() const;
  int activePatternVoice() const;
  int activePatternCursor() const;
  int activePatternStep() const;
  bool isDrumSequencerPage() const { return page_index_ == kDrumSequencer; }
  void setPatternCursor(int voiceIndex, int cursorIndex);
  void movePatternCursor(int delta);
  void movePatternCursorVertical(int delta);
  int activeDrumPatternCursor() const;
  int activeDrumStep() const;
  int activeDrumVoice() const;
  void setDrumPatternCursor(int cursorIndex);
  void moveDrumCursor(int delta);
  void moveDrumCursorVertical(int delta);
  void focusPatternRow(int voiceIndex);
  void focusPatternSteps(int voiceIndex);
  bool patternRowFocused(int voiceIndex) const;
  void focusDrumPatternRow();
  void focusDrumGrid();
  bool drumPatternRowFocused() const;
  void dismissSplash();
  bool showingSplash() const { return splash_active_; }

private:
  enum Page {
    kKnobs = 0,
    kPatternEditA,
    kKnobs2,
    kPatternEditB,
    kDrumSequencer,
    kWaveform,
    kHelp1,
    kHelp2,
    kHelpPattern,
    kHelpDrum,
    kPageCount
  };

  enum class PatternEditFocus {
    Steps = 0,
    PatternRow
  };

  int drawPageTitle(int x, int y, int w, const char* text);
  void drawWaveform(int x, int y, int w, int h);
  void drawMutesSection(int x, int y, int w, int h);
  void drawHelpPage(int x, int y, int w, int h, int helpPage);
  void drawKnobPage(int x, int y, int w, int h, int voiceIndex);
  void drawPatternEditPage(int x, int y, int w, int h, int voiceIndex);
  void drawDrumSequencerPage(int x, int y, int w, int h);
  void drawDrumLane(int x, int y, int w, int h, int cursorStep, int cursorVoice, bool gridFocus);
  void drawPageHint(int x, int y);
  void drawSplashScreen();

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  int page_index_ = 0;
  unsigned long splash_start_ms_ = 0;
  bool splash_active_ = true;
  int pattern_edit_cursor_[NUM_303_VOICES] = {0, 0};
  int pattern_row_cursor_[NUM_303_VOICES] = {0, 0};
  PatternEditFocus pattern_focus_[NUM_303_VOICES] = {PatternEditFocus::Steps,
                                                     PatternEditFocus::Steps};
  int drum_step_cursor_ = 0;
  int drum_voice_cursor_ = 0;
  int drum_pattern_cursor_ = 0;
  bool drum_pattern_focus_ = true;
};
