#include "project_page.h"
#include "../ui_common.h"
#include "../../audio/midi_importer.h"
#include <SD.h>
#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>

#include "../layout_manager.h"
#include "../screen_geometry.h"
#include "../help_dialog_frames.h"
#include "../ui_widgets.h"

namespace {
std::string generateMemorableName() {
  static const char* adjectives[] = {
    "bright", "calm", "clear", "cosmic", "crisp", "deep", "dusty", "electric",
    "faded", "gentle", "golden", "hollow", "icy", "lunar", "neon", "noisy",
    "punchy", "quiet", "rusty", "shiny", "soft", "spicy", "sticky", "sunny",
    "sweet", "velvet", "warm", "wild", "windy", "zippy"
  };
  static const char* nouns[] = {
    "amber", "aster", "bloom", "cactus", "canyon", "cloud", "comet", "desert",
    "echo", "ember", "feather", "forest", "glow", "groove", "harbor", "horizon",
    "meadow", "meteor", "mirror", "mono", "oasis", "orchid", "polaris", "ripple",
    "river", "shadow", "signal", "sky", "spark", "voyage"
  };
  constexpr int adjCount = sizeof(adjectives) / sizeof(adjectives[0]);
  constexpr int nounCount = sizeof(nouns) / sizeof(nouns[0]);
  int adjIdx = rand() % adjCount;
  int nounIdx = rand() % nounCount;
  std::string name = adjectives[adjIdx];
  name.push_back('-');
  name += nouns[nounIdx];
  return name;
}

struct TapeColor {
    const char* name;
    Rgb8 rgb;
};

static const TapeColor TAPE_PALETTE[] = {
    {"Amber",    {255, 128, 0}},
    {"WarmTape", {255, 100, 50}},
    {"Violet",   {180, 100, 255}},
    {"Mint",     {100, 255, 180}},
    {"Ice",      {100, 200, 255}},
    {"Rose",     {255, 100, 150}}
};

static const char* LED_MODE_NAMES[] = {"Off", "StepTrig", "Beat", "MuteState"};
static const char* VOICE_ID_NAMES[] = {"303A", "303B", "Kick", "Snare", "HatC", "HatO", "TomM", "TomH", "Rim", "Clap"};
static const uint8_t BRI_STEPS[] = {10, 25, 40, 60, 90};
static const uint16_t FLASH_STEPS[] = {20, 40, 60, 90};

const char* styleShortName(VisualStyle style) {
  switch (style) {
    case VisualStyle::MINIMAL: return "MINI";
    case VisualStyle::RETRO_CLASSIC: return "RETRO";
    case VisualStyle::AMBER: return "AMBER";
    default: return "RETRO";
  }
}

VisualStyle nextStyle(VisualStyle style) {
  switch (style) {
    case VisualStyle::MINIMAL: return VisualStyle::RETRO_CLASSIC;
    case VisualStyle::RETRO_CLASSIC: return VisualStyle::AMBER;
    case VisualStyle::AMBER: return VisualStyle::MINIMAL;
    default: return VisualStyle::MINIMAL;
  }
}

VisualStyle prevStyle(VisualStyle style) {
  switch (style) {
    case VisualStyle::MINIMAL: return VisualStyle::AMBER;
    case VisualStyle::RETRO_CLASSIC: return VisualStyle::MINIMAL;
    case VisualStyle::AMBER: return VisualStyle::RETRO_CLASSIC;
    default: return VisualStyle::MINIMAL;
  }
}

const char* grooveModeName(GrooveboxMode mode) {
  switch (mode) {
    case GrooveboxMode::Acid: return "ACID";
    case GrooveboxMode::Minimal: return "MINIMAL";
    case GrooveboxMode::Breaks: return "BREAKS";
    case GrooveboxMode::Dub: return "DUB";
    case GrooveboxMode::Electro: return "ELECTRO";
    default: return "MINIMAL";
  }
}

const char* grooveFlavorName(GrooveboxMode mode, int flavor) {
  if (flavor < 0) flavor = 0;
  if (flavor > 4) flavor = 4;
  static const char* acid[5] = {"CLASSIC", "SHARP", "DEEP", "RUBBER", "RAVE"};
  static const char* minimal[5] = {"TIGHT", "WARM", "AIRY", "DRY", "HYPNO"};
  static const char* breaks[5] = {"NUSKOOL", "SKITTER", "ROLLER", "CRUNCH", "LIQUID"};
  static const char* dub[5] = {"HEAVY", "SPACE", "STEPPERS", "TAPE", "FOG"};
  static const char* electro[5] = {"ROBOT", "ZAP", "BOING", "MIAMI", "INDUS"};
  switch (mode) {
    case GrooveboxMode::Acid: return acid[flavor];
    case GrooveboxMode::Minimal: return minimal[flavor];
    case GrooveboxMode::Breaks: return breaks[flavor];
    case GrooveboxMode::Dub: return dub[flavor];
    case GrooveboxMode::Electro: return electro[flavor];
    default: return minimal[flavor];
  }
}

const char* sectionName(int section) {
  switch (section) {
    case 0: return "SCENES";
    case 1: return "GROOVE";
    case 2: return "LED";
    default: return "SCENES";
  }
}

void sectionRange(int section, int& first, int& last) {
  switch (section) {
    case 0: // scenes
      first = (int)ProjectPage::MainFocus::Load;
      last = (int)ProjectPage::MainFocus::ImportMidi;
      return;
    case 1: // groove
      first = (int)ProjectPage::MainFocus::VisualStyle;
      last = (int)ProjectPage::MainFocus::Volume;
      return;
    case 2: // led
      first = (int)ProjectPage::MainFocus::LedMode;
      last = (int)ProjectPage::MainFocus::LedFlash;
      return;
    default:
      first = 0;
      last = 2;
      return;
  }
}

} // namespace

ProjectPage::ProjectPage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
  : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard),
    main_focus_(MainFocus::Load),
    dialog_type_(DialogType::None),
    dialog_focus_(DialogFocus::List),
    save_dialog_focus_(SaveDialogFocus::Input),
    selection_index_(0),
    scroll_offset_(0),
    loadError_(false),
    save_name_(generateMemorableName()) {
  refreshScenes();
}

void ProjectPage::refreshScenes() {
  scenes_ = mini_acid_.availableSceneNames();
  if (scenes_.empty()) {
    selection_index_ = 0;
    scroll_offset_ = 0;
    return;
  }
  if (selection_index_ < 0) selection_index_ = 0;
  int maxIdx = static_cast<int>(scenes_.size()) - 1;
  if (selection_index_ > maxIdx) selection_index_ = maxIdx;
  if (scroll_offset_ < 0) scroll_offset_ = 0;
  if (scroll_offset_ > maxIdx) scroll_offset_ = maxIdx;
}

void ProjectPage::openLoadDialog() {
  dialog_type_ = DialogType::Load;
  dialog_focus_ = DialogFocus::List;
  selection_index_ = 0;
  scroll_offset_ = 0;
  loadError_ = false;
  refreshScenes();
  std::string current = mini_acid_.currentSceneName();
  for (size_t i = 0; i < scenes_.size(); ++i) {
    if (scenes_[i] == current) {
      selection_index_ = static_cast<int>(i);
      break;
    }
  }
  scroll_offset_ = selection_index_;
}

void ProjectPage::openSaveDialog() {
  dialog_type_ = DialogType::SaveAs;
  save_dialog_focus_ = SaveDialogFocus::Input;
  save_name_ = mini_acid_.currentSceneName();
  if (save_name_.empty()) save_name_ = generateMemorableName();
}

void ProjectPage::refreshMidiFiles() {
  midi_files_.clear();
  if (!SD.exists("/midi")) {
      SD.mkdir("/midi");
  }
  File root = SD.open("/midi");
  if (!root) return;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    if (!entry.isDirectory()) {
      std::string name = entry.name();
      if (name.size() > 4 && name.substr(name.size() - 4) == ".mid") {
        midi_files_.push_back(name);
      }
    }
    entry.close();
  }
  root.close();
}

void ProjectPage::openImportMidiDialog() {
  refreshMidiFiles();
  dialog_type_ = DialogType::ImportMidi;
  dialog_focus_ = DialogFocus::List;
  selection_index_ = 0;
  scroll_offset_ = 0;
  midi_import_start_pattern_ = 0;
  midi_import_from_bar_ = 0;
  midi_import_length_bars_ = 16;
}

void ProjectPage::onEnter(int context) {
  dialog_type_ = DialogType::None;
  main_focus_ = MainFocus::Load;
  section_ = ProjectSection::Scenes;
  refreshScenes();
}

bool ProjectPage::importMidiAtSelection() {
  if (midi_files_.empty()) return true;
  if (selection_index_ < 0 || selection_index_ >= (int)midi_files_.size()) return true;
  
  std::string filename = midi_files_[selection_index_];
  std::string path = "/midi/" + filename;
  Serial.printf("[ProjectPage] Import MIDI: %s\n", path.c_str());
  MidiImporter importer(mini_acid_);

  MidiImporter::ImportSettings settings;
  if (midi_import_start_pattern_ < 0) midi_import_start_pattern_ = 0;
  if (midi_import_start_pattern_ > 127) midi_import_start_pattern_ = 127;
  if (midi_import_from_bar_ < 0) midi_import_from_bar_ = 0;
  if (midi_import_from_bar_ > 511) midi_import_from_bar_ = 511;
  if (midi_import_length_bars_ < 1) midi_import_length_bars_ = 1;
  if (midi_import_length_bars_ > 256) midi_import_length_bars_ = 256;
  settings.targetPatternIndex = midi_import_start_pattern_;
  settings.startStepOffset = 0;
  settings.sourceStartBar = midi_import_from_bar_;
  settings.sourceLengthBars = midi_import_length_bars_;
  settings.omni = false;
  settings.loudMode = (midi_import_profile_ == MidiImportProfile::Loud);
  
  UI::showToast("Importing MIDI...");
  
  MidiImporter::Error err;
  bool omniFallbackUsed = false;
  withAudioGuard([&]() {
    bool wasPlaying = mini_acid_.isPlaying();
    if (wasPlaying) {
      mini_acid_.stop();
    }
    err = importer.importFile(path, settings);
    if (err == MidiImporter::Error::NoNotesFound) {
      settings.omni = true;
      err = importer.importFile(path, settings);
      if (err == MidiImporter::Error::None) {
        omniFallbackUsed = true;
      }
    }
    if (wasPlaying) {
      mini_acid_.stop();
      mini_acid_.start();
    }
  });
  Serial.printf("[ProjectPage] MIDI import result=%d\n", (int)err);
  
  if (err == MidiImporter::Error::None) {
      withAudioGuard([&]() {
        auto clampf = [](float v, float lo, float hi) -> float {
          if (v < lo) return lo;
          if (v > hi) return hi;
          return v;
        };
        auto& sm = mini_acid_.sceneManager();
        for (int voice = 0; voice < 2; ++voice) {
          SynthParameters params = sm.getSynthParameters(voice);
          // Keep imported MIDI cleaner: reduce resonant ringing and long filter tails.
          if (midi_import_profile_ == MidiImportProfile::Loud) {
            params.resonance = clampf(params.resonance, 0.05f, 0.46f);
            params.envAmount = clampf(params.envAmount, 100.0f, 280.0f);
            params.envDecay = clampf(params.envDecay, 90.0f, 250.0f);
          } else {
            params.resonance = clampf(params.resonance, 0.05f, 0.36f);
            params.envAmount = clampf(params.envAmount, 80.0f, 220.0f);
            params.envDecay = clampf(params.envDecay, 70.0f, 190.0f);
          }
          sm.setSynthParameters(voice, params);
          mini_acid_.set303Parameter(TB303ParamId::Resonance, params.resonance, voice);
          mini_acid_.set303Parameter(TB303ParamId::EnvAmount, params.envAmount, voice);
          mini_acid_.set303Parameter(TB303ParamId::EnvDecay, params.envDecay, voice);
        }
      });
      if (omniFallbackUsed) {
        UI::showToast("Imported via OMNI (check channels)");
      } else {
        UI::showToast("Import Successful");
      }
      closeDialog();
  } else {
      if (settings.omni && err != MidiImporter::Error::None) {
        std::string msg = "OMNI fallback failed: " + importer.getErrorString(err);
        UI::showToast(msg.c_str());
      } else {
        UI::showToast(importer.getErrorString(err).c_str());
      }
  }
  return true;
}

bool ProjectPage::deleteSelectionInDialog() {
  if (dialog_focus_ != DialogFocus::List) return true;
  if (dialog_type_ == DialogType::Load) {
    if (scenes_.empty()) return true;
    if (selection_index_ < 0 || selection_index_ >= (int)scenes_.size()) return true;
    std::string name = scenes_[selection_index_];
    std::string path = "/scenes/" + name + ".json";
    std::string autoPath = "/scenes/" + name + ".auto.json";
    bool removed = SD.remove(path.c_str());
    SD.remove(autoPath.c_str());
    if (removed) {
      UI::showToast("Scene deleted");
      refreshScenes();
      if (selection_index_ >= (int)scenes_.size()) selection_index_ = (int)scenes_.size() - 1;
      if (selection_index_ < 0) selection_index_ = 0;
      ensureSelectionVisible(10);
    } else {
      UI::showToast("Delete failed");
    }
    return true;
  }
  if (dialog_type_ == DialogType::ImportMidi) {
    if (midi_files_.empty()) return true;
    if (selection_index_ < 0 || selection_index_ >= (int)midi_files_.size()) return true;
    std::string path = "/midi/" + midi_files_[selection_index_];
    bool removed = SD.remove(path.c_str());
    if (removed) {
      UI::showToast("MIDI deleted");
      refreshMidiFiles();
      if (selection_index_ >= (int)midi_files_.size()) selection_index_ = (int)midi_files_.size() - 1;
      if (selection_index_ < 0) selection_index_ = 0;
      ensureSelectionVisible(10);
    } else {
      UI::showToast("Delete failed");
    }
    return true;
  }
  return true;
}

void ProjectPage::closeDialog() {
  dialog_type_ = DialogType::None;
  dialog_focus_ = DialogFocus::List;
  save_dialog_focus_ = SaveDialogFocus::Input;
}


void ProjectPage::moveSelection(int delta) {
  loadError_ = false; // Clear error on move
  selection_index_ += delta;
  if (selection_index_ < 0) selection_index_ = 0;
  if (!scenes_.empty() && selection_index_ >= static_cast<int>(scenes_.size())) {
    selection_index_ = static_cast<int>(scenes_.size()) - 1;
  }
  ensureSelectionVisible(10); // Assuming 10 rows visible
}

void ProjectPage::ensureSelectionVisible(int visibleRows) {
  if (visibleRows < 1) visibleRows = 1;

  // Use appropriate list based on dialog type
  const auto& list = (dialog_type_ == DialogType::Load) ? scenes_ :
                     (dialog_type_ == DialogType::ImportMidi) ? midi_files_ :
                     scenes_; // Default for main list or others

  if (list.empty()) {
    scroll_offset_ = 0;
    selection_index_ = 0;
    return;
  }
  int maxIdx = static_cast<int>(list.size()) - 1;
  if (selection_index_ < 0) selection_index_ = 0;
  if (selection_index_ > maxIdx) selection_index_ = maxIdx;
  if (scroll_offset_ < 0) scroll_offset_ = 0;
  if (scroll_offset_ > selection_index_) scroll_offset_ = selection_index_;
  int maxScroll = maxIdx - visibleRows + 1;
  if (maxScroll < 0) maxScroll = 0;
  if (selection_index_ >= scroll_offset_ + visibleRows) {
    scroll_offset_ = selection_index_ - visibleRows + 1;
  }
  if (scroll_offset_ > maxScroll) scroll_offset_ = maxScroll;
}

void ProjectPage::ensureMainFocusVisible(int visibleRows) {
  if (visibleRows < 1) visibleRows = 1;
  const int focus = static_cast<int>(main_focus_);
  const int maxFocus = static_cast<int>(MainFocus::LedFlash);
  if (main_scroll_ < 0) main_scroll_ = 0;
  if (main_scroll_ > maxFocus) main_scroll_ = maxFocus;
  if (focus < main_scroll_) {
    main_scroll_ = focus;
  } else if (focus >= main_scroll_ + visibleRows) {
    main_scroll_ = focus - visibleRows + 1;
  }
  int maxScroll = maxFocus - visibleRows + 1;
  if (maxScroll < 0) maxScroll = 0;
  if (main_scroll_ > maxScroll) main_scroll_ = maxScroll;
}

bool ProjectPage::loadSceneAtSelection() {
  if (scenes_.empty()) return true;
  if (selection_index_ < 0 || selection_index_ >= static_cast<int>(scenes_.size())) return true;
  bool loaded = false;
  std::string name = scenes_[selection_index_];
  withAudioGuard([&]() {
    loaded = mini_acid_.loadSceneByName(name);
  });
  if (loaded) {
    closeDialog();
  } else {
    loadError_ = true;
  }
  return true;
}

void ProjectPage::randomizeSaveName() {
  save_name_ = generateMemorableName();
}

bool ProjectPage::saveCurrentScene() {
  if (save_name_.empty()) randomizeSaveName();
  bool saved = false;
  std::string name = save_name_;
  withAudioGuard([&]() {
    saved = mini_acid_.saveSceneAs(name);
  });
  if (saved) {
    closeDialog();
    refreshScenes();
  }
  return true;
}

bool ProjectPage::createNewScene() {
  randomizeSaveName();
  bool created = false;
  std::string name = save_name_;
  withAudioGuard([&]() {
    created = mini_acid_.createNewSceneWithName(name);
  });
  if (created) {
    refreshScenes();
  }
  return true;
}



bool ProjectPage::handleSaveDialogInput(char key) {
  if (key == '\b') {
    if (!save_name_.empty()) save_name_.pop_back();
    return true;
  }
  if (key >= 32 && key < 127) {
    char c = key;
    bool allowed = std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
    if (allowed) {
      if (save_name_.size() < 32) save_name_.push_back(c);
      return true;
    }
  }
  return false;
}

bool ProjectPage::handleEvent(UIEvent& ui_event) {
    if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

    if (dialog_type_ == DialogType::Load || dialog_type_ == DialogType::ImportMidi) {
        auto& list = (dialog_type_ == DialogType::Load) ? scenes_ : midi_files_;
        switch (ui_event.scancode) {
            case GROOVEPUTER_LEFT:
                if (dialog_focus_ == DialogFocus::Cancel) { dialog_focus_ = DialogFocus::List; return true; }
                break;
            case GROOVEPUTER_RIGHT:
                if (dialog_focus_ == DialogFocus::List) { dialog_focus_ = DialogFocus::Cancel; return true; }
                break;
            case GROOVEPUTER_UP:
                if (dialog_focus_ == DialogFocus::List) { 
                    loadError_ = false;
                    selection_index_--;
                    if (selection_index_ < 0) selection_index_ = 0;
                    ensureSelectionVisible(10);
                    return true; 
                }
                break;
            case GROOVEPUTER_DOWN:
                if (dialog_focus_ == DialogFocus::List) { 
                    loadError_ = false;
                    selection_index_++;
                    if (!list.empty() && selection_index_ >= (int)list.size()) selection_index_ = (int)list.size() - 1;
                    ensureSelectionVisible(10);
                    return true; 
                }
                break;
            default: break;
        }
        char key = ui_event.key;
        if (dialog_type_ == DialogType::ImportMidi) {
            if (key == '-') {
                if (midi_import_start_pattern_ > 0) midi_import_start_pattern_--;
                return true;
            }
            if (key == '=' || key == '+') {
                if (midi_import_start_pattern_ < 127) midi_import_start_pattern_++;
                return true;
            }
            if (key == '[') {
                if (midi_import_from_bar_ > 0) midi_import_from_bar_--;
                return true;
            }
            if (key == ']') {
                if (midi_import_from_bar_ < 511) midi_import_from_bar_++;
                return true;
            }
            if (key == '9') {
                if (midi_import_length_bars_ > 1) midi_import_length_bars_--;
                return true;
            }
            if (key == '0') {
                if (midi_import_length_bars_ < 256) midi_import_length_bars_++;
                return true;
            }
            if (key == 'm' || key == 'M') {
                midi_import_profile_ = (midi_import_profile_ == MidiImportProfile::Loud)
                    ? MidiImportProfile::Clean
                    : MidiImportProfile::Loud;
                return true;
            }
        }
        if (key == 'x' || key == 'X') {
            return deleteSelectionInDialog();
        }
        if (key == '\n' || key == '\r') {
            if (dialog_focus_ == DialogFocus::Cancel) { closeDialog(); return true; }
            if (dialog_type_ == DialogType::Load) return loadSceneAtSelection();
            else return importMidiAtSelection();
        }
        if (key == '\b') { closeDialog(); return true; }
        return false;
    }

    if (dialog_type_ == DialogType::SaveAs) {
        switch (ui_event.scancode) {
            case GROOVEPUTER_LEFT:
                if (save_dialog_focus_ == SaveDialogFocus::Cancel) save_dialog_focus_ = SaveDialogFocus::Save;
                else if (save_dialog_focus_ == SaveDialogFocus::Save) save_dialog_focus_ = SaveDialogFocus::Randomize;
                else if (save_dialog_focus_ == SaveDialogFocus::Randomize) save_dialog_focus_ = SaveDialogFocus::Input;
                return true;
            case GROOVEPUTER_RIGHT:
                if (save_dialog_focus_ == SaveDialogFocus::Input) save_dialog_focus_ = SaveDialogFocus::Randomize;
                else if (save_dialog_focus_ == SaveDialogFocus::Randomize) save_dialog_focus_ = SaveDialogFocus::Save;
                else if (save_dialog_focus_ == SaveDialogFocus::Save) save_dialog_focus_ = SaveDialogFocus::Cancel;
                return true;
            case GROOVEPUTER_UP:
            case GROOVEPUTER_DOWN:
                if (save_dialog_focus_ == SaveDialogFocus::Input) save_dialog_focus_ = SaveDialogFocus::Randomize;
                else save_dialog_focus_ = SaveDialogFocus::Input;
                return true;
            default: break;
        }
        char key = ui_event.key;
        if (key == 0) {
            if (ui_event.scancode >= GROOVEPUTER_F1 && ui_event.scancode <= GROOVEPUTER_F8) {
                key = static_cast<char>('1' + (ui_event.scancode - GROOVEPUTER_F1));
            }
        }
        if (save_dialog_focus_ == SaveDialogFocus::Input && handleSaveDialogInput(key)) return true;
        if (key == '\n' || key == '\r') {
            if (save_dialog_focus_ == SaveDialogFocus::Randomize) { randomizeSaveName(); return true; }
            if (save_dialog_focus_ == SaveDialogFocus::Save || save_dialog_focus_ == SaveDialogFocus::Input) return saveCurrentScene();
            if (save_dialog_focus_ == SaveDialogFocus::Cancel) { closeDialog(); return true; }
        }
        if (key == '\b') {
            if (save_dialog_focus_ == SaveDialogFocus::Input) return handleSaveDialogInput(key);
            closeDialog(); return true;
        }
        return false;
    }

    char key = ui_event.key;
    if (key == '\t') {
        int sectionIdx = static_cast<int>(section_);
        sectionIdx = (sectionIdx + 1) % 3;
        section_ = static_cast<ProjectSection>(sectionIdx);
        int focusIdx = static_cast<int>(main_focus_);
        if (!focusInSection(sectionIdx, focusIdx)) {
            main_focus_ = static_cast<MainFocus>(firstFocusInSection(sectionIdx));
        }
        return true;
    }

    switch (ui_event.scancode) {
        case GROOVEPUTER_UP: {
            int sectionIdx = static_cast<int>(section_);
            int idx = sectionNextFocus(sectionIdx, static_cast<int>(main_focus_), -1);
            main_focus_ = static_cast<MainFocus>(idx);
            ensureMainFocusVisible(8);
            return true;
        }
        case GROOVEPUTER_DOWN: {
            int sectionIdx = static_cast<int>(section_);
            int idx = sectionNextFocus(sectionIdx, static_cast<int>(main_focus_), 1);
            main_focus_ = static_cast<MainFocus>(idx);
            ensureMainFocusVisible(8);
            return true;
        }
        case GROOVEPUTER_LEFT:
        case GROOVEPUTER_RIGHT: {
            const bool right = (ui_event.scancode == GROOVEPUTER_RIGHT);
            auto& led = mini_acid_.sceneManager().currentScene().led;
            if (main_focus_ == MainFocus::Volume) {
                mini_acid_.adjustParameter(MiniAcidParamId::MainVolume, right ? 1 : -1);
                return true;
            }
            if (main_focus_ == MainFocus::VisualStyle) {
                UI::currentStyle = right ? nextStyle(UI::currentStyle) : prevStyle(UI::currentStyle);
                return true;
            }
            if (main_focus_ == MainFocus::GrooveMode) {
                withAudioGuard([&]() { mini_acid_.toggleGrooveboxMode(); });
                char toast[40];
                std::snprintf(toast, sizeof(toast), "Groove Mode: %s", grooveModeName(mini_acid_.grooveboxMode()));
                UI::showToast(toast);
                return true;
            }
            if (main_focus_ == MainFocus::GrooveFlavor) {
                withAudioGuard([&]() { mini_acid_.shiftGrooveFlavor(right ? 1 : -1); });
                return true;
            }
            if (main_focus_ == MainFocus::ApplyMacros) {
                auto& genre = mini_acid_.sceneManager().currentScene().genre;
                genre.applySoundMacros = !genre.applySoundMacros;
                return true;
            }
            if (main_focus_ == MainFocus::LedMode) {
                int m = static_cast<int>(led.mode);
                m += right ? 1 : -1;
                if (m < 0) m = 3;
                if (m > 3) m = 0;
                led.mode = static_cast<LedMode>(m);
                return true;
            }
            if (main_focus_ == MainFocus::LedSource) {
                int s = static_cast<int>(led.source);
                s += right ? 1 : -1;
                int max = static_cast<int>(VoiceId::Count) - 1;
                if (s < 0) s = max;
                if (s > max) s = 0;
                led.source = static_cast<LedSource>(s);
                return true;
            }
            if (main_focus_ == MainFocus::LedColor) {
                int currentIdx = 0;
                for (int i = 0; i < 6; ++i) {
                    if (TAPE_PALETTE[i].rgb.r == led.color.r &&
                        TAPE_PALETTE[i].rgb.g == led.color.g &&
                        TAPE_PALETTE[i].rgb.b == led.color.b) {
                        currentIdx = i;
                        break;
                    }
                }
                currentIdx += right ? 1 : -1;
                if (currentIdx < 0) currentIdx = 5;
                if (currentIdx > 5) currentIdx = 0;
                led.color = TAPE_PALETTE[currentIdx].rgb;
                return true;
            }
            if (main_focus_ == MainFocus::LedBri) {
                int currentIdx = 0;
                for (int i = 0; i < 5; ++i) {
                    if (BRI_STEPS[i] == led.brightness) {
                        currentIdx = i;
                        break;
                    }
                }
                currentIdx += right ? 1 : -1;
                if (currentIdx < 0) currentIdx = 4;
                if (currentIdx > 4) currentIdx = 0;
                led.brightness = BRI_STEPS[currentIdx];
                return true;
            }
            if (main_focus_ == MainFocus::LedFlash) {
                int currentIdx = 0;
                for (int i = 0; i < 4; ++i) {
                    if (FLASH_STEPS[i] == led.flashMs) {
                        currentIdx = i;
                        break;
                    }
                }
                currentIdx += right ? 1 : -1;
                if (currentIdx < 0) currentIdx = 3;
                if (currentIdx > 3) currentIdx = 0;
                led.flashMs = FLASH_STEPS[currentIdx];
                return true;
            }
            return false;
        }
        default: break;
    }

    if (key == 'g' || key == 'G') {
        requestPageTransition(0); // Genre Page
        return true;
    }

    if (key == '\n' || key == '\r') {
        if (main_focus_ == MainFocus::Load) { openLoadDialog(); return true; }
        if (main_focus_ == MainFocus::SaveAs) { openSaveDialog(); return true; }
        if (main_focus_ == MainFocus::New) return createNewScene();
        if (main_focus_ == MainFocus::ImportMidi) { openImportMidiDialog(); return true; }

        if (main_focus_ == MainFocus::VisualStyle) { UI::currentStyle = nextStyle(UI::currentStyle); return true; }
        if (main_focus_ == MainFocus::GrooveMode) {
            withAudioGuard([&]() { mini_acid_.toggleGrooveboxMode(); });
            char toast[40];
            std::snprintf(toast, sizeof(toast), "Groove Mode: %s", grooveModeName(mini_acid_.grooveboxMode()));
            UI::showToast(toast);
            return true;
        }
        if (main_focus_ == MainFocus::GrooveFlavor) {
            withAudioGuard([&]() { mini_acid_.shiftGrooveFlavor(1); });
            return true;
        }
        
        auto& led = mini_acid_.sceneManager().currentScene().led;
        if (main_focus_ == MainFocus::LedMode) { led.mode = static_cast<LedMode>((static_cast<int>(led.mode) + 1) % 4); return true; }
        if (main_focus_ == MainFocus::LedSource) {
            led.source = static_cast<LedSource>((static_cast<int>(led.source) + 1) % static_cast<int>(VoiceId::Count));
            switch (led.source) {
                case LedSource::SynthA: led.color = TAPE_PALETTE[1].rgb; break;
                case LedSource::SynthB: led.color = TAPE_PALETTE[2].rgb; break;
                case LedSource::DrumKick: led.color = TAPE_PALETTE[0].rgb; break;
                case LedSource::DrumSnare: led.color = TAPE_PALETTE[3].rgb; break;
                case LedSource::DrumClap: led.color = TAPE_PALETTE[5].rgb; break;
                default: led.color = TAPE_PALETTE[4].rgb; break;
            }
            return true;
        }
        if (main_focus_ == MainFocus::LedColor) {
            int currentIdx = 0;
            for (int i=0; i<6; ++i) if (TAPE_PALETTE[i].rgb.r == led.color.r && TAPE_PALETTE[i].rgb.g == led.color.g) currentIdx = i;
            led.color = TAPE_PALETTE[(currentIdx + 1) % 6].rgb;
            return true;
        }
        if (main_focus_ == MainFocus::LedBri) {
            int currentIdx = 0;
            for (int i=0; i<5; ++i) if (BRI_STEPS[i] == led.brightness) currentIdx = i;
            led.brightness = BRI_STEPS[(currentIdx + 1) % 5];
            return true;
        }
        if (main_focus_ == MainFocus::LedFlash) {
            int currentIdx = 0;
            for (int i=0; i<4; ++i) if (FLASH_STEPS[i] == led.flashMs) currentIdx = i;
            led.flashMs = FLASH_STEPS[(currentIdx + 1) % 4];
            return true;
        }
    }
    return false;
}

const std::string & ProjectPage::getTitle() const {
  static std::string title = "PROJECT";
  return title;
}

void ProjectPage::draw(IGfx& gfx) {
  UI::drawStandardHeader(gfx, mini_acid_, "PROJECT");
  LayoutManager::clearContent(gfx);

  const int x = Layout::COL_1;
  const int y0 = LayoutManager::lineY(0);
  const int line_h = gfx.fontHeight();
  const int listW = Layout::COL_WIDTH;
  const int infoX = Layout::COL_2;
  const int infoW = Layout::CONTENT.w - infoX - 4;
  int sectionIdx = static_cast<int>(section_);
  int firstFocus = 0;
  int lastFocus = 0;
  sectionRange(sectionIdx, firstFocus, lastFocus);
  auto& led = mini_acid_.sceneManager().currentScene().led;

  // Top status line
  gfx.setTextColor(COLOR_LABEL);
  gfx.drawText(x, y0, "SCENE");
  gfx.setTextColor(COLOR_WHITE);
  Widgets::drawClippedText(gfx, x + 34, y0, listW - 34, mini_acid_.currentSceneName().c_str());
  gfx.setTextColor(COLOR_LABEL);
  char sectionBuf[24];
  std::snprintf(sectionBuf, sizeof(sectionBuf), "INFO [%s]", sectionName(sectionIdx));
  gfx.drawText(infoX, y0, sectionBuf);

  const char* tabNames[3] = {"SCN", "GRV", "LED"};
  for (int i = 0; i < 3; ++i) {
    int tx = x + i * 28;
    bool activeTab = (i == sectionIdx);
    gfx.setTextColor(activeTab ? COLOR_ACCENT : COLOR_LABEL);
    gfx.drawText(tx, LayoutManager::lineY(1), tabNames[i]);
  }

  const int visibleRows = 8;
  const int rowBase = 2;
  for (int row = 0; row < visibleRows; ++row) {
    int focusIdx = firstFocus + row;
    if (focusIdx > lastFocus) break;
    MainFocus focus = static_cast<MainFocus>(focusIdx);
    bool selected = (focus == main_focus_) && dialog_type_ == DialogType::None;

    char line[48];
    switch (focus) {
      case MainFocus::Load: std::snprintf(line, sizeof(line), "Load Scene"); break;
      case MainFocus::SaveAs: std::snprintf(line, sizeof(line), "Save As"); break;
      case MainFocus::New: std::snprintf(line, sizeof(line), "New"); break;
      case MainFocus::ImportMidi: std::snprintf(line, sizeof(line), "Import MIDI"); break;
      case MainFocus::VisualStyle:
        std::snprintf(line, sizeof(line), "Theme      %s", styleShortName(UI::currentStyle));
        break;
      case MainFocus::GrooveMode:
        std::snprintf(line, sizeof(line), "Groove     %s", grooveModeName(mini_acid_.grooveboxMode()));
        break;
      case MainFocus::GrooveFlavor: {
        int f = mini_acid_.grooveFlavor();
        std::snprintf(line, sizeof(line), "Flavor     %s", grooveFlavorName(mini_acid_.grooveboxMode(), f));
        break;
      }
      case MainFocus::ApplyMacros: {
        bool on = mini_acid_.sceneManager().currentScene().genre.applySoundMacros;
        std::snprintf(line, sizeof(line), "Apply Sound [%s]", on ? "ON" : "OFF");
        break;
      }
      case MainFocus::Volume: {
        int volPct = (int)(mini_acid_.miniParameter(MiniAcidParamId::MainVolume).normalized() * 100.0f + 0.5f);
        std::snprintf(line, sizeof(line), "Main Vol   %d%%", volPct);
        break;
      }
      case MainFocus::LedMode:
        std::snprintf(line, sizeof(line), "LED Mode   %s", LED_MODE_NAMES[static_cast<int>(led.mode)]);
        break;
      case MainFocus::LedSource:
        std::snprintf(line, sizeof(line), "LED Src    %s", VOICE_ID_NAMES[static_cast<int>(led.source)]);
        break;
      case MainFocus::LedColor: {
        int colorIdx = 0;
        for (int i = 0; i < 6; ++i) {
          if (TAPE_PALETTE[i].rgb.r == led.color.r &&
              TAPE_PALETTE[i].rgb.g == led.color.g &&
              TAPE_PALETTE[i].rgb.b == led.color.b) {
            colorIdx = i;
            break;
          }
        }
        std::snprintf(line, sizeof(line), "LED Color  %s", TAPE_PALETTE[colorIdx].name);
        break;
      }
      case MainFocus::LedBri:
        std::snprintf(line, sizeof(line), "LED Bri    %u%%", (unsigned)led.brightness);
        break;
      case MainFocus::LedFlash:
        std::snprintf(line, sizeof(line), "LED Flash  %ums", (unsigned)led.flashMs);
        break;
    }
    Widgets::drawListRow(gfx, x, LayoutManager::lineY(rowBase + row), listW, line, selected);
  }

  uint32_t freeInt = 0;
  uint32_t largestInt = 0;
#if defined(ESP32) || defined(ESP_PLATFORM)
  freeInt = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  largestInt = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
#endif
  int cpuAvg = (int)mini_acid_.perfStats.cpuAudioPctIdeal;
  int cpuPeak = (int)mini_acid_.perfStats.cpuAudioPeakPct;
  static char perf0[42];
  static char perf1[42];
  static char perf2[42];
  std::snprintf(perf0, sizeof(perf0), "CPU:%d/%d%%", cpuAvg, cpuPeak);
  std::snprintf(perf1, sizeof(perf1), "RAM:%uk/%uk",
                (unsigned)(freeInt / 1024), (unsigned)(largestInt / 1024));
  std::snprintf(perf2, sizeof(perf2), "Th:%s  M:%s",
                styleShortName(UI::currentStyle), grooveModeName(mini_acid_.grooveboxMode()));
  const char* infoLines[3] = {perf0, perf1, perf2};
  Widgets::drawInfoBox(gfx, infoX, LayoutManager::lineY(2), infoW, infoLines, 3);

  if (dialog_type_ == DialogType::None) {
      return;
  }

  int w = Layout::CONTENT.w - 2 * Layout::CONTENT_PAD_X;
  int h = Layout::CONTENT.h - 2 * Layout::CONTENT_PAD_Y;
  int y_start = Layout::CONTENT.y + Layout::CONTENT_PAD_Y;
  int dialog_w = w - 16;
  if (dialog_w < 80) dialog_w = w - 4;
  if (dialog_w < 60) dialog_w = 60;
  int dialog_h = h - 16;
  if (dialog_h < 70) dialog_h = h - 4;
  if (dialog_h < 50) dialog_h = 50;
  int dialog_x = x + (w - dialog_w) / 2;
  int dialog_y = y_start + (h - dialog_h) / 2;

  gfx.fillRect(dialog_x, dialog_y, dialog_w, dialog_h, COLOR_DARKER);
  gfx.drawRect(dialog_x, dialog_y, dialog_w, dialog_h, COLOR_ACCENT);

  if (dialog_type_ == DialogType::Load || dialog_type_ == DialogType::ImportMidi) {
    int header_h = line_h + 4;
    gfx.setTextColor(COLOR_WHITE);
    const char* title = (dialog_type_ == DialogType::Load) ? "Load Scene" : "Import MIDI File";
    gfx.drawText(dialog_x + 4, dialog_y + 2, title);
    if (loadError_) {
      gfx.setTextColor(COLOR_ACCENT);
      gfx.drawText(dialog_x + dialog_w - 70, dialog_y + 2, "LOAD FAILED");
    }

    int row_h = line_h + 3;
    int cancel_h = line_h + 8;
    int content_y = dialog_y + header_h + 2;
    int content_h = dialog_h - header_h - cancel_h - 10;
    if (content_h < row_h) content_h = row_h;
    int list_x = dialog_x + 2;
    int list_w = dialog_w - 4;
    int list_y = content_y;
    int list_h = content_h;
    if (dialog_type_ == DialogType::ImportMidi) {
      int info_w = dialog_w / 2;
      if (info_w < 100) info_w = 100;
      if (info_w > dialog_w - 86) info_w = dialog_w - 86;
      int gap = 4;
      list_w = dialog_w - info_w - gap - 4;
      if (list_w < 78) list_w = 78;
      int info_x = list_x + list_w + gap;
      int info_h = content_h;
      gfx.fillRect(info_x, content_y, info_w, info_h, COLOR_PANEL);
      gfx.drawRect(info_x, content_y, info_w, info_h, COLOR_LABEL);

      char startBuf[24];
      char fromBuf[24];
      char lenBuf[24];
      char sliceBuf[24];
      char writeBuf[28];
      char modeBuf[20];
      std::snprintf(startBuf, sizeof(startBuf), "Pat %d", midi_import_start_pattern_ + 1);
      std::snprintf(fromBuf, sizeof(fromBuf), "Bar %d", midi_import_from_bar_ + 1);
      std::snprintf(lenBuf, sizeof(lenBuf), "Len %d", midi_import_length_bars_);
      std::snprintf(modeBuf, sizeof(modeBuf), "Mode %s",
                    (midi_import_profile_ == MidiImportProfile::Loud) ? "LOUD" : "CLEAN");
      int sourceStart = midi_import_from_bar_ + 1;
      int sourceEnd = sourceStart + midi_import_length_bars_ - 1;
      std::snprintf(sliceBuf, sizeof(sliceBuf), "B%d-%d", sourceStart, sourceEnd);
      int targetStart = midi_import_start_pattern_ + 1;
      int targetEnd = targetStart + midi_import_length_bars_ - 1;
      bool truncated = false;
      if (targetEnd > 128) {
        targetEnd = 128;
        truncated = true;
      }
      if (truncated) {
        std::snprintf(writeBuf, sizeof(writeBuf), "P%d-%d *", targetStart, targetEnd);
      } else {
        std::snprintf(writeBuf, sizeof(writeBuf), "P%d-%d", targetStart, targetEnd);
      }

      gfx.setTextColor(COLOR_LABEL);
      gfx.drawText(info_x + 4, content_y + 3, modeBuf);
      gfx.drawText(info_x + 4, content_y + 3 + line_h + 2, startBuf);
      gfx.drawText(info_x + 4, content_y + 3 + line_h * 2 + 4, fromBuf);
      gfx.drawText(info_x + 4, content_y + 3 + line_h * 3 + 6, lenBuf);
      gfx.drawText(info_x + 4, content_y + 3 + line_h * 4 + 8, sliceBuf);
      gfx.drawText(info_x + 4, content_y + 3 + line_h * 5 + 10, writeBuf);
      gfx.drawText(info_x + 4, content_y + info_h - line_h - 1, "-/+ [/] 9/0 M");
      gfx.setTextColor(COLOR_WHITE);
    }
    int visible_rows = list_h / row_h;
    if (visible_rows < 1) visible_rows = 1;

    ensureSelectionVisible(visible_rows);

    const auto& list = (dialog_type_ == DialogType::Load) ? scenes_ : midi_files_;

    if (list.empty()) {
      gfx.setTextColor(COLOR_LABEL);
      gfx.drawText(list_x + 4, list_y + 2, "No files found");
      gfx.setTextColor(COLOR_WHITE);
    } else {
      int rowsToDraw = visible_rows;
      if (rowsToDraw > static_cast<int>(list.size()) - scroll_offset_) {
        rowsToDraw = static_cast<int>(list.size()) - scroll_offset_;
      }
      for (int i = 0; i < rowsToDraw; ++i) {
        int listIdx = scroll_offset_ + i;
        if (listIdx < 0 || listIdx >= (int)list.size()) {
            continue;
        }
        int row_y = list_y + i * row_h;
        bool selected = listIdx == selection_index_;
        if (selected) {
          gfx.fillRect(list_x, row_y, list_w, row_h, COLOR_PANEL);
          gfx.drawRect(list_x, row_y, list_w, row_h, COLOR_ACCENT);
        }
        Widgets::drawClippedText(gfx, list_x + 4, row_y + 1, list_w - 8, list[listIdx].c_str());
      }
    }

    int cancel_w = 60;
    if (cancel_w > dialog_w - 8) cancel_w = dialog_w - 8;
    int cancel_x = dialog_x + dialog_w - cancel_w - 4;
    int cancel_y = dialog_y + dialog_h - cancel_h - 4;
    bool cancelFocused = dialog_focus_ == DialogFocus::Cancel;
    gfx.fillRect(cancel_x, cancel_y, cancel_w, cancel_h, COLOR_PANEL);
    gfx.drawRect(cancel_x, cancel_y, cancel_w, cancel_h, cancelFocused ? COLOR_ACCENT : COLOR_LABEL);
    const char* cancelLabel = "Cancel";
    int cancel_tw = textWidth(gfx, cancelLabel);
    gfx.drawText(cancel_x + (cancel_w - cancel_tw) / 2, cancel_y + (cancel_h - line_h) / 2, cancelLabel);
    return;
  }

  if (dialog_type_ == DialogType::SaveAs) {
    int header_h = line_h + 4;
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(dialog_x + 4, dialog_y + 2, "Save Scene As");

    int input_h = line_h + 8;
    int input_y = dialog_y + header_h + 4;
    gfx.fillRect(dialog_x + 4, input_y, dialog_w - 8, input_h, COLOR_PANEL);
    bool inputFocused = save_dialog_focus_ == SaveDialogFocus::Input;
    gfx.drawRect(dialog_x + 4, input_y, dialog_w - 8, input_h, inputFocused ? COLOR_ACCENT : COLOR_LABEL);
    gfx.drawText(dialog_x + 8, input_y + (input_h - line_h) / 2, save_name_.c_str());

    const char* btnLabels[] = {"Randomize", "Save", "Cancel"};
    SaveDialogFocus btnFocusOrder[] = {SaveDialogFocus::Randomize, SaveDialogFocus::Save, SaveDialogFocus::Cancel};
    int btnCount = 3;
    int btnAreaY = input_y + input_h + 8;
    int btnAreaH = line_h + 8;
    int btnSpacing = 6;
    int btnAreaW = dialog_w - 12;
    int btnStartX = dialog_x + 6;
    int btnWidth = (btnAreaW - btnSpacing * (btnCount - 1)) / btnCount;
    if (btnWidth < 50) btnWidth = 50;
    for (int i = 0; i < btnCount; ++i) {
      int bx = btnStartX + i * (btnWidth + btnSpacing);
      bool focused = save_dialog_focus_ == btnFocusOrder[i];
      gfx.fillRect(bx, btnAreaY, btnWidth, btnAreaH, COLOR_PANEL);
      gfx.drawRect(bx, btnAreaY, btnWidth, btnAreaH, focused ? COLOR_ACCENT : COLOR_LABEL);
      int tw = textWidth(gfx, btnLabels[i]);
      gfx.drawText(bx + (btnWidth - tw) / 2, btnAreaY + (btnAreaH - line_h) / 2, btnLabels[i]);
    }
  }
}

int ProjectPage::firstFocusInSection(int sectionIdx) {
  if (sectionIdx == 0) return (int)ProjectPage::MainFocus::Load;
  if (sectionIdx == 1) return (int)ProjectPage::MainFocus::VisualStyle;
  if (sectionIdx == 2) return (int)ProjectPage::MainFocus::LedMode;
  return 0;
}

int ProjectPage::lastFocusInSection(int sectionIdx) {
  if (sectionIdx == 0) return (int)ProjectPage::MainFocus::ImportMidi;
  if (sectionIdx == 1) return (int)ProjectPage::MainFocus::Volume;
  if (sectionIdx == 2) return (int)ProjectPage::MainFocus::LedFlash;
  return 0;
}

bool ProjectPage::focusInSection(int sectionIdx, int focusIdx) {
  ProjectPage::MainFocus f = static_cast<ProjectPage::MainFocus>(focusIdx);
  if (sectionIdx == 0) return f >= ProjectPage::MainFocus::Load && f <= ProjectPage::MainFocus::ImportMidi;
  if (sectionIdx == 1) return f >= ProjectPage::MainFocus::VisualStyle && f <= ProjectPage::MainFocus::Volume;
  if (sectionIdx == 2) return f >= ProjectPage::MainFocus::LedMode && f <= ProjectPage::MainFocus::LedFlash;
  return false;
}

int ProjectPage::sectionNextFocus(int section, int current, int delta) {
  int first = firstFocusInSection(section);
  int last = lastFocusInSection(section);
  int span = last - first + 1;
  if (span <= 0) return first;
  int idx = current;
  if (idx < first || idx > last) idx = first;
  idx -= first;
  idx = (idx + delta + span) % span;
  return first + idx;
}
