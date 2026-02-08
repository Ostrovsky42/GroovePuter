#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class ProjectPage : public IPage{
 public:
  ProjectPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override;
  enum class ProjectSection { Scenes = 0, Groove, Led };
  enum class MainFocus { Load = 0, SaveAs, New, VisualStyle, GrooveMode, GrooveFlavor, ApplyMacros, Volume, LedMode, LedSource, LedColor, LedBri, LedFlash };

 private:
  enum class DialogType { None = 0, Load, SaveAs };
  enum class DialogFocus { List = 0, Cancel };
  enum class SaveDialogFocus { Input = 0, Randomize, Save, Cancel };

  int firstFocusInSection(int sectionIdx);
  int lastFocusInSection(int sectionIdx);
  bool focusInSection(int sectionIdx, int focusIdx);
  int sectionNextFocus(int section, int current, int delta);

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
  void ensureMainFocusVisible(int visibleRows);
  template <typename F>
  void withAudioGuard(F&& fn) {
      if (audio_guard_) audio_guard_(std::forward<F>(fn));
      else fn();
  }
  
  void updateFromEngine();

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  MainFocus main_focus_;
  ProjectSection section_ = ProjectSection::Scenes;
  DialogType dialog_type_;
  DialogFocus dialog_focus_;
  SaveDialogFocus save_dialog_focus_;
  int selection_index_;
  int scroll_offset_;
  int main_scroll_ = 0;
  bool loadError_;
  std::vector<std::string> scenes_;
  std::string save_name_;
};
