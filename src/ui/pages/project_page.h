#pragma once

#include <cstdint>

#include "../ui_core.h"
#include "../ui_colors.h"
#include "../ui_utils.h"
#include "../midi_file_manager.h"
#include "help_dialog.h"
#include "src/audio/pattern_paging.h"
#include "src/state/scene_revision.h"

// withAudioGuard() is a header-defined template, so the toast declaration
// must be visible before the template is parsed without pulling ui_common.h
// and its complete rendering dependency graph into every ProjectPage user.
namespace UI {
void showToast(const char* msg, int durationMs);
}

class ProjectPage : public IPage, public IMultiHelpFramesProvider {
 public:
  ProjectPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard);
  void draw(IGfx& gfx) override;
  void onEnter(int context = 0) override;
  bool handleEvent(UIEvent& ui_event) override;
  const std::string & getTitle() const override;

  std::unique_ptr<MultiPageHelpDialog> getHelpDialog() override;
  int getHelpFrameCount() const override;
  void drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const override;
  enum class ProjectSection { Scenes = 0, Groove, Led, Midi };
  enum class MainFocus { Load = 0, SaveAs, New, ImportMidi, ClearProject, VisualStyle, GrooveMode, GrooveFlavor, Volume, LedMode, LedSource, LedColor, LedBri, LedFlash, MidiDevice, MidiInputEnabled, MidiInputChannel, MidiInputTarget };

 private:
  enum class DialogType { None = 0, Load, SaveAs, ImportMidi, MidiAdvance, ConfirmClear };
  enum class DialogFocus { List = 0, Cancel };
  enum class SaveDialogFocus { Input = 0, Randomize, Save, Cancel };
  enum class MidiImportProfile { Clean = 0, Loud };
  enum class MidiAdvanceFocus {
      Mode = 0,
      StartPattern, AutoFind,
      FromBar, LengthBars,
      TrackMap,
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
  void openImportMidiDialog();
  void openMidiAdvanceDialog();
  void returnToMidiBrowser();
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
      const bool clearCurrentProject =
          main_focus_ == MainFocus::ClearProject &&
          dialog_type_ == DialogType::ConfirmClear;
      const bool creatingNewProject = main_focus_ == MainFocus::New;
      const std::string sceneNameBefore = creatingNewProject
          ? mini_acid_.currentSceneName()
          : std::string();
      const std::string requestedNewProject = creatingNewProject
          ? save_name_
          : std::string();

      if (audio_guard_) audio_guard_(std::forward<F>(fn));
      else fn();
      GroovePuterState::markSceneMutated();

      const std::string sceneNameAfter = creatingNewProject
          ? mini_acid_.currentSceneName()
          : std::string();
      const bool createdDifferentProject =
          creatingNewProject && sceneNameAfter != sceneNameBefore;
      const bool newProjectRolledBack =
          creatingNewProject &&
          sceneNameAfter == sceneNameBefore &&
          !requestedNewProject.empty() &&
          requestedNewProject != sceneNameBefore;

      bool lifecycleOk = true;
      if (clearCurrentProject || createdDifferentProject) {
        lifecycleOk = PatternPagingService::clearProjectPages();
      } else if (newProjectRolledBack) {
        // setCurrentSceneName() copies pages before the scene JSON is written.
        // A failed write returns to the original project; remove only the
        // abandoned target namespace without switching active project state.
        lifecycleOk = PatternPagingService::clearProjectPages(
            requestedNewProject);
      }

      // Clear must be durable immediately: rewrite the zeroed scene JSON and
      // remove its autosave recovery before the confirmation dialog closes.
      // Otherwise a reboot before the next autosave can restore page 1.
      if (clearCurrentProject && lifecycleOk) {
        lifecycleOk = mini_acid_.saveSceneAs(mini_acid_.currentSceneName());
      }

      if (!lifecycleOk) {
        UI::showToast(clearCurrentProject
                          ? "Project clear not saved"
                          : "Pattern cleanup failed",
                      1400);
      }
  }

  void autoRouteMidi();
  bool adjustMidiInput(int delta);

  IGfx& gfx_;
  MiniAcid& mini_acid_;
  AudioGuard audio_guard_;
  MainFocus main_focus_;
  ProjectSection section_ = ProjectSection::Scenes;
  DialogType dialog_type_;
  DialogFocus dialog_focus_;
  SaveDialogFocus save_dialog_focus_;
  MidiAdvanceFocus midi_advance_focus_ = MidiAdvanceFocus::Mode;
  int selection_index_;
  int scroll_offset_;
  int main_scroll_ = 0;
  bool loadError_;
  std::vector<std::string> scenes_;
  std::string midi_selected_path_;
  int midi_import_start_pattern_ = 0;
  int midi_import_from_bar_ = 0;
  int midi_import_length_bars_ = 16;
  MidiImportProfile midi_import_profile_ = MidiImportProfile::Loud;

  // Matrix Routing Masks
  uint16_t midi_mask_a_ = 0;
  uint16_t midi_mask_b_ = 0;
  uint16_t midi_mask_d_ = 0;
  int midi_map_cursor_ = 0; // 0-15, for TrackMap navigation

  bool midi_import_append_ = false;
  int midi_adv_scroll_ = 0;
  uint8_t midi_profile_preview_ = 0xFF;
  std::string save_name_;
};

#if defined(ARDUINO) || defined(ESP_PLATFORM)
static_assert(sizeof(ProjectPage) <= 256,
              "Project page must leave enough contiguous DRAM for SD browsing");
#endif
