#pragma once

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"

class ProjectPage : public IPage{
 public:
  ProjectPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
  void draw(IGfx& gfx) override;
  void onEnter(int context = 0) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override;
  enum class ProjectSection { Scenes = 0, Groove, Led };
  enum class MainFocus { Load = 0, SaveAs, New, ImportMidi, ClearProject, VisualStyle, GrooveMode, GrooveFlavor, ApplyMacros, Volume, LedMode, LedSource, LedColor, LedBri, LedFlash };

 private:
  enum class DialogType { None = 0, Load, SaveAs, ImportMidi, MidiAdvance, ConfirmClear };
  enum class DialogFocus { List = 0, Cancel };
  enum class SaveDialogFocus { Input = 0, Randomize, Save, Cancel };
  enum class MidiImportProfile { Clean = 0, Loud };
  enum class MidiAdvanceFocus { 
      SynthA = 0, SynthB, Drums, 
      Mode, 
      StartPattern, AutoFind, 
      FromBar, LengthBars, 
      Import, Cancel,
      Count
  };

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
  void refreshMidiFiles();
  bool navigateIntoMidiDir(const std::string& dirName);
  bool navigateUpMidiDir();
  bool isMidiDirEntry(int index) const;
  int midiDirCount() const { return (int)midi_dirs_.size(); }
  std::string midiDisplayName(int index) const;
  void openImportMidiDialog();
  void openMidiAdvanceDialog();
  bool importMidiAtSelection();
  void drawMidiAdvanceDialog(IGfx& gfx);
  void openConfirmClearDialog();
  void drawConfirmClearDialog(IGfx& gfx);
  bool clearProject();
  bool deleteSelectionInDialog();
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
  MidiAdvanceFocus midi_advance_focus_ = MidiAdvanceFocus::SynthA;
  int selection_index_;
  int scroll_offset_;
  int main_scroll_ = 0;
  bool loadError_;
  std::vector<std::string> scenes_;
  std::vector<std::string> midi_dirs_;
  std::vector<std::string> midi_files_;
  std::string midi_current_path_ = "/midi";
  int midi_import_start_pattern_ = 0;
  int midi_import_from_bar_ = 0;
  int midi_import_length_bars_ = 16;
  MidiImportProfile midi_import_profile_ = MidiImportProfile::Loud;
  int midi_dest_a_ = 0;
  int midi_dest_b_ = 1;
  int midi_dest_d_ = 2;
  bool midi_import_append_ = false;
  int midi_adv_scroll_ = 0;
  std::string save_name_;
};
