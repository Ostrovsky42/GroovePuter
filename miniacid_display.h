#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "dsp_engine.h"
#include "display.h"


enum KeyScanCode {
  MINIACID_NO_SCANCODE = 0,
  MINIACID_DOWN,
  MINIACID_UP,
  MINIACID_LEFT,
  MINIACID_RIGHT,
};

enum EventType {
  MINIACID_NO_TYPE = 0,
  MINIACID_KEY_DOWN,
};

struct UIEvent {
  EventType event_type = MINIACID_NO_TYPE;
  KeyScanCode scancode = MINIACID_NO_SCANCODE;
  char key = 0;
  bool alt = false;
};

class IPage {
  public:
    virtual ~IPage() = default;
    virtual void draw(IGfx& gfx, int x, int y, int w, int h) = 0;
    virtual bool handleEvent(UIEvent& ui_event) = 0;
};

class PageHelper {
  public:
    virtual int drawPageTitle(int x, int y, int w, const char* text) = 0;
};

using AudioGuard = std::function<void(const std::function<void()>&)>;

class HelpPage : public IPage{
  public:
    HelpPage(PageHelper& page_helper);
    virtual void draw(IGfx& gfx, int x, int y, int w, int h) override;
    virtual bool handleEvent(UIEvent& ui_event) override;
  private:
    void drawHelpPage(IGfx& gfx, int x, int y, int w, int h, int helpPage);
    PageHelper& page_helper_;
    int help_page_index_;
    int total_help_pages_;
};

class Synth303ParamsPage : public IPage {
  public:
    Synth303ParamsPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard, int voice_index);
    virtual void draw(IGfx& gfx, int x, int y, int w, int h) override;
    virtual bool handleEvent(UIEvent& ui_event) override;
  private:
    void withAudioGuard(const std::function<void()>& fn);
    IGfx& gfx_;
    MiniAcid& mini_acid_;
    PageHelper& page_helper_;
    AudioGuard& audio_guard_;
    int voice_index_;
};

class WaveformPage : public IPage {
  public:
    WaveformPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard);
    virtual void draw(IGfx& gfx, int x, int y, int w, int h) override;
    virtual bool handleEvent(UIEvent& ui_event) override;
  private:
    IGfx& gfx_;
    MiniAcid& mini_acid_;
    PageHelper& page_helper_;
    AudioGuard& audio_guard_;
    int wave_color_index_;
};

class PatternEditPage : public IPage {
  public:
    PatternEditPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard, int voice_index);
    virtual void draw(IGfx& gfx, int x, int y, int w, int h) override;
    virtual bool handleEvent(UIEvent& ui_event) override;

    int activePatternCursor() const;
    int activePatternStep() const;
    void setPatternCursor(int cursorIndex);
    void focusPatternRow();
    void focusPatternSteps();
    bool patternRowFocused() const;
    void movePatternCursor(int delta);
    void movePatternCursorVertical(int delta);
    int voiceIndex() const { return voice_index_; }

  private:
    enum class Focus { Steps = 0, PatternRow };

    int clampCursor(int cursorIndex) const;
    int patternIndexFromKey(char key) const;
    void ensureStepFocus();
    void withAudioGuard(const std::function<void()>& fn);

    IGfx& gfx_;
    MiniAcid& mini_acid_;
    PageHelper& page_helper_;
    AudioGuard& audio_guard_;
    int voice_index_;
    int pattern_edit_cursor_;
    int pattern_row_cursor_;
    Focus focus_;
};

class DrumSequencerPage : public IPage {
  public:
    DrumSequencerPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard);
    virtual void draw(IGfx& gfx, int x, int y, int w, int h) override;
    virtual bool handleEvent(UIEvent& ui_event) override;

  private:
    int activeDrumPatternCursor() const;
    int activeDrumStep() const;
    int activeDrumVoice() const;
    void setDrumPatternCursor(int cursorIndex);
    void moveDrumCursor(int delta);
    void moveDrumCursorVertical(int delta);
    void focusPatternRow();
    void focusGrid();
    bool patternRowFocused() const;
    int patternIndexFromKey(char key) const;
    void withAudioGuard(const std::function<void()>& fn);

    IGfx& gfx_;
    MiniAcid& mini_acid_;
    PageHelper& page_helper_;
    AudioGuard& audio_guard_;
    int drum_step_cursor_;
    int drum_voice_cursor_;
    int drum_pattern_cursor_;
    bool drum_pattern_focus_;
};

class SongPage : public IPage{
  public:
    SongPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard);
    virtual void draw(IGfx& gfx, int x, int y, int w, int h) override;
    virtual bool handleEvent(UIEvent& ui_event) override;
    void setScrollToPlayhead(int playhead);
  private:
    int clampCursorRow(int row) const;
    int cursorRow() const;
    int cursorTrack() const;
    bool cursorOnModeButton() const;
    bool cursorOnPlayheadLabel() const;
    void moveCursorHorizontal(int delta);
    void moveCursorVertical(int delta);
    void syncSongPositionToCursor();
    void withAudioGuard(const std::function<void()>& fn);
    SongTrack trackForColumn(int col, bool& valid) const;
    int patternIndexFromKey(char key) const;
    bool adjustSongPatternAtCursor(int delta);
    bool adjustSongPlayhead(int delta);
    bool assignPattern(int patternIdx);
    bool clearPattern();
    bool toggleSongMode();

    IGfx& gfx_;
    MiniAcid& mini_acid_;
    PageHelper& page_helper_;
    AudioGuard& audio_guard_;
    int cursor_row_;
    int cursor_track_;
    int scroll_row_;
};

class ProjectPage : public IPage{
 public:
  ProjectPage(IGfx& gfx, MiniAcid& mini_acid, PageHelper& page_helper, AudioGuard& audio_guard);
  virtual void draw(IGfx& gfx, int x, int y, int w, int h) override;
  virtual bool handleEvent(UIEvent& ui_event) override;
 private:
  enum class MainFocus { Load = 0, SaveAs, New };
  enum class DialogType { None = 0, Load, SaveAs };
  enum class DialogFocus { List = 0, Cancel };
  enum class SaveDialogFocus { Input = 0, Randomize, Save, Cancel };

  void refreshScenes();
  void openLoadDialog();
  void openSaveDialog();
  void closeDialog();
  void moveSelection(int delta);
  bool loadSceneAtSelection();
  void ensureSelectionVisible(int visibleRows);
  void randomizeSaveName();
  bool saveCurrentScene();
  bool createNewScene();
  bool handleSaveDialogInput(char key);
  void withAudioGuard(const std::function<void()>& fn);

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  PageHelper& page_helper_;
  AudioGuard& audio_guard_;
  MainFocus main_focus_;
  DialogType dialog_type_;
  DialogFocus dialog_focus_;
  SaveDialogFocus save_dialog_focus_;
  int selection_index_;
  int scroll_offset_;
  std::vector<std::string> scenes_;
  std::string save_name_;
};

class MiniAcidDisplay : public PageHelper{
public:
  MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid);
  ~MiniAcidDisplay();
  void setAudioGuard(AudioGuard guard);
  void update();
  void nextPage();
  void previousPage();
  void dismissSplash();
  bool handleEvent(UIEvent event);

  // PageHelper Overrides
  int drawPageTitle(int x, int y, int w, const char* text) override;


private:
  enum PageType {
    k303AParameters = 0,
    k303APatternEdit,
    k303BParameters,
    k303BPatternEdit,
    kDrumSequencer,
    kSong,
    kProject,
    kWaveform,
    kHelp,
    kPageCount
  };

  void drawMutesSection(int x, int y, int w, int h);
  void drawPageHint(int x, int y);
  void drawSplashScreen();

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  int page_index_ = 0;
  unsigned long splash_start_ms_ = 0;
  bool splash_active_ = true;

  AudioGuard audio_guard_;
  std::unique_ptr<IPage> pages_[kPageCount];
};
