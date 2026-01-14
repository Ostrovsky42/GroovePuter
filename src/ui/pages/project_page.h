#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class ProjectPage : public IPage{
 public:
  ProjectPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard& audio_guard);
  void draw(IGfx& gfx) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override;

 private:
  enum class MainFocus { Load = 0, SaveAs, New, Mode };
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
