#include "project_page.h"
#include "../ui_common.h"
#include "../../audio/midi_importer.h"
#include <algorithm>
#include <vector>
#ifdef ARDUINO
#include <SD.h>
#endif
#if defined(ESP32) || defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <new>

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
      last = (int)ProjectPage::MainFocus::ClearProject;
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
  if (scenes_.empty()) refreshScenes();
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

void ProjectPage::openImportMidiDialog() {
  GroovePuterUi::midiFileManager().open();
  midi_selected_path_.clear();
  dialog_type_ = DialogType::ImportMidi;
  dialog_focus_ = DialogFocus::List;
  selection_index_ = 0;
  scroll_offset_ = 0;
  midi_import_start_pattern_ = 0;
  midi_import_from_bar_ = 0;
  midi_import_length_bars_ = 16;
  midi_mask_a_ = 0;
  midi_mask_b_ = 0;
  midi_mask_d_ = 0;
  midi_import_append_ = false;
}

void ProjectPage::openMidiAdvanceDialog() {
  char selectedPath[GroovePuterUi::MidiFileManager::kPathBytes]{};
  if (!GroovePuterUi::midiFileManager().selectedFilePath(
          selectedPath, sizeof(selectedPath))) {
    UI::showToast("Select a MIDI file", 900);
    return;
  }

  midi_selected_path_ = selectedPath;
  dialog_type_ = DialogType::MidiAdvance;
  midi_advance_focus_ = MidiAdvanceFocus::Mode;
  midi_adv_scroll_ = 0;

  UI::showToast("Scanning MIDI...");
  MidiImporter importer(mini_acid_);
  midi_scan_ = importer.scanFile(midi_selected_path_);
  if (midi_scan_.valid) {
    autoRouteMidi();
    if (midi_scan_.estimatedBars > 0) {
      midi_import_length_bars_ = midi_scan_.estimatedBars;
    }
  }
}

void ProjectPage::autoRouteMidi() {
  midi_mask_a_ = 0;
  midi_mask_b_ = 0;
  midi_mask_d_ = 0;

  // Channel 10 is the only unconditional GM drum route. Otherwise
  // require an explicit percussion-like track name; never steal a
  // melodic channel just because it has the most notes.
  int drumCh = midi_scan_.channels[9].used() ? 10 : -1;
  if (drumCh < 0) {
    for (int i = 0; i < 16; ++i) {
      if (!midi_scan_.channels[i].used()) continue;
      std::string name = midi_scan_.channels[i].trackName;
      std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
      });
      if (name.find("drum") != std::string::npos ||
          name.find("perc") != std::string::npos ||
          name.find("beat") != std::string::npos ||
          name.find("kick") != std::string::npos ||
          name.find("snare") != std::string::npos) {
        drumCh = i + 1;
        break;
      }
    }
  }
  if (drumCh > 0) midi_mask_d_ |= (1u << (drumCh - 1));

  std::vector<int> candidates;
  for (int i = 0; i < 16; ++i) {
    if (drumCh > 0 && i == drumCh - 1) continue;
    if (midi_scan_.channels[i].used()) candidates.push_back(i + 1);
  }

  int foundA = -1;
  int foundB = -1;
  for (int chNum : candidates) {
    std::string name = midi_scan_.channels[chNum - 1].trackName;
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
      return static_cast<char>(std::tolower(ch));
    });
    if (foundA < 0 &&
        (name.find("bass") != std::string::npos ||
         name.find("303") != std::string::npos)) {
      foundA = chNum;
    } else if (foundB < 0 &&
               (name.find("acid") != std::string::npos ||
                name.find("lead") != std::string::npos ||
                name.find("arp") != std::string::npos ||
                name.find("melody") != std::string::npos)) {
      foundB = chNum;
    }
  }

  std::sort(candidates.begin(), candidates.end(), [this](int a, int b) {
    return midi_scan_.channels[a - 1].minNote <
           midi_scan_.channels[b - 1].minNote;
  });

  if (foundA > 0) {
    midi_mask_a_ |= (1u << (foundA - 1));
  } else if (!candidates.empty()) {
    midi_mask_a_ |= (1u << (candidates.front() - 1));
  }

  if (foundB > 0 && !((midi_mask_a_ >> (foundB - 1)) & 1u)) {
    midi_mask_b_ |= (1u << (foundB - 1));
  } else {
    for (int chNum : candidates) {
      if (!((midi_mask_a_ >> (chNum - 1)) & 1u)) {
        midi_mask_b_ |= (1u << (chNum - 1));
        break;
      }
    }
  }

  std::string path = midi_selected_path_;
  std::transform(path.begin(), path.end(), path.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  if (path.find("hotline") != std::string::npos ||
      path.find("perturbator") != std::string::npos ||
      path.find("disco") != std::string::npos) {
    midi_import_profile_ = MidiImportProfile::Loud;
  }
}

void ProjectPage::openConfirmClearDialog() {
  dialog_type_ = DialogType::ConfirmClear;
  dialog_focus_ = DialogFocus::Cancel;
}

void ProjectPage::drawConfirmClearDialog(IGfx& gfx) {
    int w = 180;
    int h = 70;
    int x = Layout::CONTENT.x + (Layout::CONTENT.w - w) / 2;
    int y = Layout::CONTENT.y + (Layout::CONTENT.h - h) / 2;

    gfx.fillRect(x, y, w, h, COLOR_DARKER);
    gfx.drawRect(x, y, w, h, COLOR_ACCENT);

    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x + 10, y + 10, "REALLY CLEAR PROJECT?");

    int btnW = 60;
    int btnH = 20;
    int btnY = y + 35;

    // YES button (reusing List focus)
    bool yesFocus = (dialog_focus_ == DialogFocus::List);
    gfx.fillRect(x + 20, btnY, btnW, btnH, yesFocus ? COLOR_PANEL : COLOR_GRAY);
    gfx.setTextColor(yesFocus ? COLOR_ACCENT : COLOR_LABEL);
    gfx.drawText(x + 40, btnY + 4, "YES");

    // NO button (reusing Cancel focus)
    bool noFocus = (dialog_focus_ == DialogFocus::Cancel);
    gfx.fillRect(x + w - btnW - 20, btnY, btnW, btnH, noFocus ? COLOR_PANEL : COLOR_GRAY);
    gfx.setTextColor(noFocus ? COLOR_ACCENT : COLOR_LABEL);
    gfx.drawText(x + w - btnW, btnY + 4, "NO");
}

bool ProjectPage::clearProject() {
  withAudioGuard([&]() {
    mini_acid_.sceneManager().wipeToZero();
  });
  UI::showToast("Project cleared to zero");
  closeDialog();
  return true;
}

void ProjectPage::onEnter(int context) {
  dialog_type_ = DialogType::None;
  main_focus_ = MainFocus::Load;
  section_ = ProjectSection::Scenes;
  if (scenes_.empty()) refreshScenes();
}

bool ProjectPage::importMidiAtSelection() {
  if (midi_selected_path_.empty()) {
    UI::showToast("Select a MIDI file", 900);
    dialog_type_ = DialogType::ImportMidi;
    return true;
  }

  if ((midi_mask_a_ | midi_mask_b_ | midi_mask_d_) == 0) {
    UI::showToast("Select at least one MIDI route");
    return true;
  }

  const std::string path = midi_selected_path_;
  Serial.printf("[ProjectPage] Import MIDI: %s\n", path.c_str());

  MidiImporter::ImportSettings settings;
  if (midi_import_start_pattern_ < 0) midi_import_start_pattern_ = 0;
  if (midi_import_start_pattern_ >= kMaxPatterns) {
    midi_import_start_pattern_ = kMaxPatterns - 1;
  }
  if (midi_import_from_bar_ < 0) midi_import_from_bar_ = 0;
  if (midi_import_from_bar_ > 511) midi_import_from_bar_ = 511;
  if (midi_import_length_bars_ < 0) midi_import_length_bars_ = 0;
  if (midi_import_length_bars_ > 256) midi_import_length_bars_ = 256;

  settings.targetPatternIndex = midi_import_start_pattern_;
  settings.startStepOffset = 0;
  settings.sourceStartBar = midi_import_from_bar_;
  settings.sourceLengthBars = midi_import_length_bars_;
  settings.overwrite = true;
  settings.loudMode = (midi_import_profile_ == MidiImportProfile::Loud);
  settings.synthAMask = midi_mask_a_;
  settings.synthBMask = midi_mask_b_;
  settings.drumMask = midi_mask_d_;

  MidiImporter importer(mini_acid_);
  MidiImporter::Error err = MidiImporter::Error::ReadError;
  bool persisted = false;

  UI::showToast("Importing MIDI...");
  withAudioGuard([&]() {
    const bool wasPlaying = mini_acid_.isPlaying();
    if (wasPlaying) mini_acid_.stop();

    std::string lowerPath = path;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(),
                   [](unsigned char ch) {
                     return static_cast<char>(std::tolower(ch));
                   });
    if (lowerPath.find("disco") != std::string::npos ||
        lowerPath.find("perturbator") != std::string::npos ||
        lowerPath.find("909") != std::string::npos) {
      mini_acid_.setDrumEngine("909");
    } else if (lowerPath.find("808") != std::string::npos ||
               lowerPath.find("trap") != std::string::npos ||
               lowerPath.find("hiphop") != std::string::npos) {
      mini_acid_.setDrumEngine("808");
    }

    err = importer.importFile(path, settings);
    if (err == MidiImporter::Error::None) {
      auto clampf = [](float value, float lo, float hi) {
        if (value < lo) return lo;
        if (value > hi) return hi;
        return value;
      };

      auto& sm = mini_acid_.sceneManager();
      for (int voice = 0; voice < 2; ++voice) {
        SynthParameters params = sm.getSynthParameters(voice);
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
        mini_acid_.set303Parameter(TB303ParamId::Resonance,
                                  params.resonance, voice);
        mini_acid_.set303Parameter(TB303ParamId::EnvAmount,
                                  params.envAmount, voice);
        mini_acid_.set303Parameter(TB303ParamId::EnvDecay,
                                  params.envDecay, voice);
      }

      const int startPattern = settings.targetPatternIndex;
      const int lastPattern = importer.getLastImportedPatternIdx();
      const int importedBars = lastPattern >= startPattern
          ? lastPattern - startPattern + 1
          : 0;

      int songPosition = sm.getSongPosition();
      if (midi_import_append_) {
        songPosition = sm.songLength();
        const bool firstRowEmpty =
            sm.songLength() == 1 &&
            sm.songPattern(0, SongTrack::SynthA) < 0 &&
            sm.songPattern(0, SongTrack::SynthB) < 0 &&
            sm.songPattern(0, SongTrack::Drums) < 0 &&
            sm.songPattern(0, SongTrack::Voice) < 0;
        if (firstRowEmpty) songPosition = 0;
      }

      for (int i = 0; i < importedBars; ++i) {
        const int targetPosition = songPosition + i;
        const int patternIndex = startPattern + i;
        if (targetPosition >= Song::kMaxPositions ||
            patternIndex >= kMaxPatterns) {
          break;
        }
        if (midi_mask_a_) {
          sm.setSongPattern(targetPosition, SongTrack::SynthA, patternIndex);
        }
        if (midi_mask_b_) {
          sm.setSongPattern(targetPosition, SongTrack::SynthB, patternIndex);
        }
        if (midi_mask_d_) {
          sm.setSongPattern(targetPosition, SongTrack::Drums, patternIndex);
        }
      }

      persisted = mini_acid_.saveSceneAs(mini_acid_.currentSceneName());
    }

    if (wasPlaying) mini_acid_.start();
  });

  Serial.printf("[ProjectPage] MIDI import result=%d saved=%d\n",
                static_cast<int>(err), persisted ? 1 : 0);
  if (err == MidiImporter::Error::None) {
    if (persisted) GroovePuterState::markSceneSaveSucceeded();
    UI::showToast(persisted
        ? "MIDI imported and saved"
        : "MIDI imported; save failed");
    closeDialog();
  } else {
    UI::showToast(importer.getErrorString(err).c_str());
  }
  return true;
}

bool ProjectPage::deleteSelectionInDialog() {
  if (dialog_focus_ != DialogFocus::List || dialog_type_ != DialogType::Load) {
    return true;
  }
  if (scenes_.empty()) return true;
  if (selection_index_ < 0 || selection_index_ >= static_cast<int>(scenes_.size())) {
    return true;
  }

  const std::string name = scenes_[selection_index_];
  const std::string path = "/scenes/" + name + ".json";
  const std::string autoPath = "/scenes/" + name + ".auto.json";
  const bool removed = SD.remove(path.c_str());
  SD.remove(autoPath.c_str());
  if (removed) {
    UI::showToast("Scene deleted");
    refreshScenes();
    if (selection_index_ >= static_cast<int>(scenes_.size())) {
      selection_index_ = static_cast<int>(scenes_.size()) - 1;
    }
    if (selection_index_ < 0) selection_index_ = 0;
    ensureSelectionVisible(10);
  } else {
    UI::showToast("Delete failed");
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
  const int listCount = static_cast<int>(scenes_.size());
  if (listCount <= 0) {
    scroll_offset_ = 0;
    selection_index_ = 0;
    return;
  }
  const int maxIdx = listCount - 1;
  selection_index_ = std::max(0, std::min(selection_index_, maxIdx));
  if (scroll_offset_ > selection_index_) scroll_offset_ = selection_index_;
  if (selection_index_ >= scroll_offset_ + visibleRows) {
    scroll_offset_ = selection_index_ - visibleRows + 1;
  }
  const int maxScroll = std::max(0, maxIdx - visibleRows + 1);
  scroll_offset_ = std::max(0, std::min(scroll_offset_, maxScroll));
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
    GroovePuterState::markSceneLoadSucceeded();
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
  const std::string name = save_name_;
  const GroovePuterState::SceneRevisionState revisionBefore =
      GroovePuterState::sceneRevisionSnapshot();
  withAudioGuard([&]() {
    saved = mini_acid_.saveSceneAs(name);
  });
  if (saved) {
    GroovePuterState::markSceneSaveSucceeded();
    closeDialog();
    refreshScenes();
    UI::showToast("Project and songs saved");
  } else {
    GroovePuterState::restoreSceneRevision(revisionBefore);
    UI::showToast("Project save failed");
  }
  return true;
}

bool ProjectPage::createNewScene() {
  randomizeSaveName();
  bool created = false;
  const std::string name = save_name_;
  withAudioGuard([&]() {
    created = mini_acid_.createNewSceneWithName(name);
  });
  if (created) {
    GroovePuterState::markSceneSaveSucceeded();
    refreshScenes();
    UI::showToast("Blank project created");
  } else {
    UI::showToast("New project save failed");
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

namespace {
const char* getGMName(uint8_t program) {
    if (program >= 0 && program <= 7) return "PIANO";
    if (program >= 8 && program <= 15) return "CHRMTIC";
    if (program >= 16 && program <= 23) return "ORGAN";
    if (program >= 24 && program <= 31) return "GUITAR";
    if (program >= 32 && program <= 39) return "BASS";
    if (program >= 40 && program <= 47) return "STRINGS";
    if (program >= 48 && program <= 55) return "ENSEMBL";
    if (program >= 56 && program <= 63) return "BRASS";
    if (program >= 64 && program <= 71) return "REED";
    if (program >= 72 && program <= 79) return "PIPE";
    if (program >= 80 && program <= 87) return "SYN LD";
    if (program >= 88 && program <= 95) return "SYN PD";
    if (program >= 96 && program <= 103) return "SYN FX";
    if (program >= 104 && program <= 111) return "ETHNIC";
    if (program >= 112 && program <= 119) return "PERCUS";
    if (program >= 120 && program <= 127) return "SFX";
    return "";
}
}

void ProjectPage::drawMidiAdvanceDialog(IGfx& gfx) {
    int w = Layout::CONTENT.w - 4;
    int h = Layout::CONTENT.h - 4;
    int x = Layout::CONTENT.x + (Layout::CONTENT.w - w) / 2;
    int y = Layout::CONTENT.y + (Layout::CONTENT.h - h) / 2;

    gfx.fillRect(x, y, w, h, COLOR_DARKER);
    gfx.drawRect(x, y, w, h, COLOR_ACCENT);

    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(x + 6, y + 4, "MIDI MATRIX");

    int lineH = gfx.fontHeight() + 2;
    int startY = y + 20;

    // --- Left Side: Settings ---
    auto drawRow = [&](int rowIdx, const char* label, const char* value, bool focus) {
        int ry = startY + rowIdx * lineH;
        if (focus) {
            gfx.fillRect(x + 2, ry - 1, 100, lineH, COLOR_PANEL);
        }
        gfx.setTextColor(focus ? COLOR_ACCENT : COLOR_LABEL);
        gfx.drawText(x + 4, ry, label);
        gfx.setTextColor(COLOR_WHITE);
        gfx.drawText(x + 55, ry, value); 
    };

    char buf[64];
    
    // Explicit order of items
    MidiAdvanceFocus items[] = {
        MidiAdvanceFocus::Mode,
        MidiAdvanceFocus::StartPattern,
        MidiAdvanceFocus::AutoFind,
        MidiAdvanceFocus::FromBar,
        MidiAdvanceFocus::LengthBars,
        MidiAdvanceFocus::Import,
        MidiAdvanceFocus::Cancel
    };
    
    for (int i = 0; i < 7; ++i) {
        MidiAdvanceFocus f = items[i];
        bool focused = (midi_advance_focus_ == f);
        const char* label = "";
        
        switch (f) {
            case MidiAdvanceFocus::Mode:
                label = "Profile:"; std::snprintf(buf, sizeof(buf), "%s", (midi_import_profile_ == MidiImportProfile::Loud) ? "LOUD" : "CLEAN");
                break;
            case MidiAdvanceFocus::StartPattern:
                std::snprintf(buf, sizeof(buf), "0x%02X", midi_import_start_pattern_);
                label = "Target:";
                break;
            case MidiAdvanceFocus::AutoFind:
                label = "Method:"; std::snprintf(buf, sizeof(buf), "%s", midi_import_append_ ? "APPEND" : "OVWRT");
                break;
            case MidiAdvanceFocus::FromBar:
                std::snprintf(buf, sizeof(buf), (midi_import_from_bar_ == 0) ? "START" : "Bar %d", midi_import_from_bar_ + 1);
                label = "From:";
                break;
            case MidiAdvanceFocus::LengthBars:
                if (midi_import_length_bars_ <= 0) std::strcpy(buf, "AUTO");
                else std::snprintf(buf, sizeof(buf), "%d Bars", midi_import_length_bars_);
                label = "Length:";
                break;
            case MidiAdvanceFocus::Import:
                label = ">> EXECUTE <<"; buf[0] = 0;
                break;
            case MidiAdvanceFocus::Cancel:
                label = "<< CANCEL >>"; buf[0] = 0;
                break;
            default: continue;
        }
        
        drawRow(i, label, buf, focused);
    }

    // General Info (Bottom Left)
    float bpm = mini_acid_.bpm();
    int barsVal = (midi_import_length_bars_ > 0) ? midi_import_length_bars_ : 16;
    std::snprintf(buf, sizeof(buf), "%d Bars @ %.0f BPM", barsVal, bpm);
    gfx.setTextColor(COLOR_LABEL);
    gfx.drawText(x + 4, y + h - 12, buf);

    // --- Right Side: Track Map ---
    int mapX = x + 108;
    int mapY = startY;
    int mapW = w - 112;
    
    int cellW = mapW / 4;
    int cellH = lineH + 2; 
    
    // Labels for Map
    if (midi_advance_focus_ == MidiAdvanceFocus::TrackMap) {
        gfx.setTextColor(COLOR_ACCENT);
        gfx.drawText(mapX, y + 4, "[ CH SELECT ]");
    } else {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(mapX, y + 4, "TRACK CHANNELS");
    }

    for (int i=0; i<16; ++i) {
        int col = i % 4;
        int row = i / 4;
        int cx = mapX + col * cellW;
        int cy = mapY + row * cellH;
        
        bool isCursor = (midi_advance_focus_ == MidiAdvanceFocus::TrackMap && midi_map_cursor_ == i);
        bool used = midi_scan_.channels[i].used();
        
        // bg
        if (used) {
            gfx.fillRect(cx+1, cy+1, cellW-2, cellH-2, COLOR_DARK_GRAY);
        }
        
        // border
        if (isCursor) {
            gfx.drawRect(cx, cy, cellW, cellH, (millis() % 500 < 250) ? COLOR_WHITE : COLOR_ACCENT);
        } else {
            gfx.drawRect(cx, cy, cellW, cellH, COLOR_PANEL);
        }
        
        // Num
        std::snprintf(buf, sizeof(buf), "%d", i+1);
        gfx.setTextColor(used ? COLOR_WHITE : COLOR_GRAY);
        gfx.drawText(cx + 3, cy + 2, buf);
        
        // Tag
        char tag = ' ';
        IGfxColor tagColor = COLOR_GRAY;
        
        if ((midi_mask_a_ >> i) & 1) { tag = 'A'; tagColor = COLOR_ACCENT; }
        else if ((midi_mask_b_ >> i) & 1) { tag = 'B'; tagColor = 0xFD20; }
        else if ((midi_mask_d_ >> i) & 1) { tag = 'D'; tagColor = 0x07E0; }
        
        if (tag != ' ') {
            buf[0] = tag; buf[1] = 0;
            gfx.setTextColor(tagColor);
            gfx.drawText(cx + cellW - 9, cy + 2, buf);
        }
    }
    
    // Instrument / Channel Details (Under the map)
    int detailX = mapX;
    int detailY = mapY + 4 * cellH + 6;
    
    int focusCh = (midi_advance_focus_ == MidiAdvanceFocus::TrackMap) ? midi_map_cursor_ : -1;
    // Fallback: show info of first routed track if not in map focus? 
    // Actually user wants info of hovered channel.
    
    if (focusCh >= 0) {
        auto& ci = midi_scan_.channels[focusCh];
        gfx.setTextColor(COLOR_WHITE);
        if (ci.trackName[0]) {
             Widgets::drawClippedText(gfx, detailX, detailY, mapW, ci.trackName);
        } else if (ci.used()) {
             std::snprintf(buf, sizeof(buf), "Ch %d: %d notes", focusCh+1, ci.noteCount);
             gfx.drawText(detailX, detailY, buf);
        } else {
             gfx.setTextColor(COLOR_GRAY);
             gfx.drawText(detailX, detailY, "Empty Channel");
        }
        
        // Add more detail like min/max note or program if available
        if (ci.used()) {
            if (ci.program != 255) {
                std::snprintf(buf, sizeof(buf), "GM: %s", getGMName(ci.program));
                gfx.setTextColor(COLOR_LABEL);
                gfx.drawText(detailX, detailY + lineH, buf);
            } else {
                char n1[8], n2[8];
                formatNoteName(ci.minNote, n1, sizeof(n1));
                formatNoteName(ci.maxNote, n2, sizeof(n2));
                std::snprintf(buf, sizeof(buf), "Range: %s - %s", n1, n2);
                gfx.setTextColor(COLOR_LABEL);
                gfx.drawText(detailX, detailY + lineH, buf);
            }
        }
    } else {
        // Show routing summary?
        int countA=0, countB=0, countD=0;
        for(int i=0; i<16; i++) {
            if((midi_mask_a_ >> i) & 1) countA++;
            if((midi_mask_b_ >> i) & 1) countB++;
            if((midi_mask_d_ >> i) & 1) countD++;
        }
        gfx.setTextColor(COLOR_LABEL);
        std::snprintf(buf, sizeof(buf), "Routes: A[%d] B[%d] D[%d]", countA, countB, countD);
        gfx.drawText(detailX, detailY, buf);
    }

    // Scrollbar indicator if needed (shifted left to avoid map)
    int visibleRows = 7;
    if ((int)MidiAdvanceFocus::Count > visibleRows) {
        int sbH = h - 45; 
        int sbY = startY;
        int knobH = std::max(4, sbH * visibleRows / (int)MidiAdvanceFocus::Count);
        int knobY = sbY + (sbH - knobH) * midi_adv_scroll_ / ((int)MidiAdvanceFocus::Count - visibleRows);
        gfx.fillRect(mapX - 6, sbY, 2, sbH, COLOR_PANEL);
        gfx.fillRect(mapX - 6, knobY, 2, knobH, COLOR_ACCENT);
    }
}


bool ProjectPage::handleEvent(UIEvent& ui_event) {
    if (ui_event.event_type != GROOVEPUTER_KEY_DOWN) return false;

    if (dialog_type_ == DialogType::ImportMidi) {
        if (ui_event.key == '	') {
            openMidiAdvanceDialog();
            return true;
        }
        char activatedPath[GroovePuterUi::MidiFileManager::kPathBytes]{};
        const auto result = GroovePuterUi::midiFileManager().handleEvent(
            ui_event, activatedPath, sizeof(activatedPath));
        if (result == GroovePuterUi::MidiFileManager::EventResult::FileActivated) {
            midi_selected_path_ = activatedPath;
            openMidiAdvanceDialog();
            return true;
        }
        if (result == GroovePuterUi::MidiFileManager::EventResult::CloseRequested) {
            closeDialog();
            return true;
        }
        return true;
    }

    if (dialog_type_ == DialogType::Load) {
        if (ui_event.scancode == GROOVEPUTER_ESCAPE || ui_event.key == '') {
            closeDialog();
            return true;
        }
        if (ui_event.scancode == GROOVEPUTER_LEFT) {
            dialog_focus_ = DialogFocus::List;
            return true;
        }
        if (ui_event.scancode == GROOVEPUTER_RIGHT) {
            dialog_focus_ = DialogFocus::Cancel;
            return true;
        }
        if (ui_event.scancode == GROOVEPUTER_UP && dialog_focus_ == DialogFocus::List) {
            moveSelection(-1);
            return true;
        }
        if (ui_event.scancode == GROOVEPUTER_DOWN && dialog_focus_ == DialogFocus::List) {
            moveSelection(1);
            return true;
        }
        if (ui_event.key == 'x' || ui_event.key == 'X') {
            return deleteSelectionInDialog();
        }
        if (ui_event.key == '
' || ui_event.key == '') {
            if (dialog_focus_ == DialogFocus::Cancel) {
                closeDialog();
                return true;
            }
            return loadSceneAtSelection();
        }
        return true;
    }

    if (dialog_type_ == DialogType::MidiAdvance) {
        if (ui_event.scancode == GROOVEPUTER_ESCAPE) {
            dialog_type_ = DialogType::ImportMidi;
            return true;
        }
        int focus = (int)midi_advance_focus_;

        // UP / DOWN
        if (ui_event.scancode == GROOVEPUTER_UP) {
            if (midi_advance_focus_ == MidiAdvanceFocus::TrackMap) {
                midi_map_cursor_ -= 4;
                if (midi_map_cursor_ < 0) midi_map_cursor_ += 16;
                return true;
            }
            focus--;
            if (focus < 0) focus = (int)MidiAdvanceFocus::Count - 1;
            midi_advance_focus_ = static_cast<MidiAdvanceFocus>(focus);
            
            if (midi_advance_focus_ == MidiAdvanceFocus::TrackMap) {
                midi_advance_focus_ = MidiAdvanceFocus::LengthBars; 
            }
            
            if ((int)midi_advance_focus_ < midi_adv_scroll_) {
                midi_adv_scroll_ = (int)midi_advance_focus_;
            }
            if ((int)midi_advance_focus_ > (int)MidiAdvanceFocus::Count - 5) {
                midi_adv_scroll_ = (int)MidiAdvanceFocus::Count - 5;
            }
            return true;
        }
        if (ui_event.scancode == GROOVEPUTER_DOWN) {
            if (midi_advance_focus_ == MidiAdvanceFocus::TrackMap) {
                midi_map_cursor_ = (midi_map_cursor_ + 4) % 16;
                return true;
            }
            focus++;
            if (focus >= (int)MidiAdvanceFocus::Count) focus = 0;
            midi_advance_focus_ = static_cast<MidiAdvanceFocus>(focus);
            
            if (midi_advance_focus_ == MidiAdvanceFocus::TrackMap) {
                midi_advance_focus_ = MidiAdvanceFocus::Import;
            }

            if ((int)midi_advance_focus_ >= midi_adv_scroll_ + 5) {
                midi_adv_scroll_ = (int)midi_advance_focus_ - 4;
            }
            if (midi_advance_focus_ == MidiAdvanceFocus::Mode) {
                midi_adv_scroll_ = 0;
            }
            return true;
        }
        
        // LEFT / RIGHT
        if (ui_event.scancode == GROOVEPUTER_RIGHT) {
            if (midi_advance_focus_ == MidiAdvanceFocus::TrackMap) {
                if ((midi_map_cursor_ % 4) < 3) midi_map_cursor_++;
                return true;
            }
            // Enter map from settings
            if (midi_advance_focus_ != MidiAdvanceFocus::Import && midi_advance_focus_ != MidiAdvanceFocus::Cancel) {
                midi_advance_focus_ = MidiAdvanceFocus::TrackMap;
                return true;
            }
        }
        
        if (ui_event.scancode == GROOVEPUTER_LEFT) {
            if (midi_advance_focus_ == MidiAdvanceFocus::TrackMap) {
                if ((midi_map_cursor_ % 4) == 0) {
                    midi_advance_focus_ = MidiAdvanceFocus::StartPattern; 
                } else {
                    midi_map_cursor_--;
                }
                return true;
            }
        }

        char key = ui_event.key;
        if (key == '\r' || key == '\n') {
            if (midi_advance_focus_ == MidiAdvanceFocus::Import) return importMidiAtSelection();
            if (midi_advance_focus_ == MidiAdvanceFocus::Cancel) { dialog_type_ = DialogType::ImportMidi; return true; }
            
            if (midi_advance_focus_ == MidiAdvanceFocus::TrackMap) {
                 // Toggle Routing: . -> A -> B -> D -> .
                 int ch = midi_map_cursor_ + 1; // 1-based
                 bool isA = (midi_mask_a_ >> (ch-1)) & 1;
                 bool isB = (midi_mask_b_ >> (ch-1)) & 1;
                 bool isD = (midi_mask_d_ >> (ch-1)) & 1;
                 
                 // Clear all first
                 midi_mask_a_ &= ~(1 << (ch-1)); 
                 midi_mask_b_ &= ~(1 << (ch-1)); 
                 midi_mask_d_ &= ~(1 << (ch-1));
                 
                 if (isA) {
                     midi_mask_b_ |= (1 << (ch-1)); 
                 } else if (isB) {
                     midi_mask_d_ |= (1 << (ch-1));
                 } else if (isD) {
                     // Nothing (already cleared)
                 } else {
                     midi_mask_a_ |= (1 << (ch-1));
                 }
                 return true;
            }
        }
        if (key == '\b') { dialog_type_ = DialogType::ImportMidi; return true; }

        int delta = 0;
        if (ui_event.scancode == GROOVEPUTER_LEFT) delta = -1;
        if (ui_event.scancode == GROOVEPUTER_RIGHT) delta = 1;
        if (key == '-' || key == '_') delta = -1;
        if (key == '=' || key == '+') delta = 1;

        switch (midi_advance_focus_) {
            case MidiAdvanceFocus::Mode:
                if (delta != 0) {
                    midi_import_profile_ = (midi_import_profile_ == MidiImportProfile::Loud) ? MidiImportProfile::Clean : MidiImportProfile::Loud;
                    return true;
                }
                break;
            case MidiAdvanceFocus::StartPattern:
                if (delta != 0) {
                    midi_import_start_pattern_ += delta;
                    if (midi_import_start_pattern_ < 0) midi_import_start_pattern_ = 0;
                    if (midi_import_start_pattern_ >= kMaxPatterns) midi_import_start_pattern_ = kMaxPatterns - 1;
                    return true;
                }
                break;
            case MidiAdvanceFocus::AutoFind:
                if (delta != 0 || key == ' ') { midi_import_append_ = !midi_import_append_; return true; }
                break;
            case MidiAdvanceFocus::FromBar:
                if (delta != 0) {
                    midi_import_from_bar_ += delta;
                    if (midi_import_from_bar_ < 0) midi_import_from_bar_ = 0;
                    if (midi_import_from_bar_ > 511) midi_import_from_bar_ = 511;
                    return true;
                }
                break;
            case MidiAdvanceFocus::LengthBars:
                if (delta != 0) {
                    midi_import_length_bars_ += delta;
                    if (midi_import_length_bars_ < 0) midi_import_length_bars_ = 0;
                    if (midi_import_length_bars_ > 256) midi_import_length_bars_ = 256;
                    return true;
                }
                break;
            default: break;
        }

        if (midi_advance_focus_ == MidiAdvanceFocus::AutoFind && (key == 'f' || key == 'F' || (key == '\r' || key == '\n'))) {
                int patternsNeeded = midi_import_length_bars_;
                if (patternsNeeded <= 0) patternsNeeded = 16;
                SongTrack track = SongTrack::SynthA;
                if (midi_mask_a_ == 0) {
                    if (midi_mask_b_ != 0) track = SongTrack::SynthB;
                    else if (midi_mask_d_ != 0) track = SongTrack::Drums;
                }
                int freeIdx = mini_acid_.sceneManager().findFirstFreePattern(midi_import_start_pattern_, track, patternsNeeded);
                if (freeIdx >= 0) {
                    midi_import_start_pattern_ = freeIdx;
                    UI::showToast("Found free block");
                } else {
                    UI::showToast("No free space");
                }
                return true;
        }

        return true; // Consume all keys in advanced dialog
    }
    if (dialog_type_ == DialogType::ConfirmClear) {
        if (ui_event.scancode == GROOVEPUTER_LEFT) { dialog_focus_ = DialogFocus::List; return true; }
        if (ui_event.scancode == GROOVEPUTER_RIGHT) { dialog_focus_ = DialogFocus::Cancel; return true; }
        char key = ui_event.key;
        if (key == '\r' || key == '\n') {
            if (dialog_focus_ == DialogFocus::List) return clearProject();
            else { closeDialog(); return true; }
        }
        if (key == '\b') { closeDialog(); return true; }
        return true;
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
                GroovePuterState::markSceneMutated();
                return true;
            }
            if (main_focus_ == MainFocus::VisualStyle) {
                UI::currentStyle = right ? nextStyle(UI::currentStyle) : prevStyle(UI::currentStyle);
                return true;
            }
            if (main_focus_ == MainFocus::GrooveMode) {
                withAudioGuard([&]() { mini_acid_.toggleGrooveboxMode(); });
                char toast[64];
                std::snprintf(toast, sizeof(toast), "Groove Mode: %s (override)",
                              grooveModeName(mini_acid_.grooveboxMode()));
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
                GroovePuterState::markSceneMutated();
                return true;
            }
            if (main_focus_ == MainFocus::LedMode) {
                int m = static_cast<int>(led.mode);
                m += right ? 1 : -1;
                if (m < 0) m = 3;
                if (m > 3) m = 0;
                led.mode = static_cast<LedMode>(m);
                GroovePuterState::markSceneMutated();
                return true;
            }
            if (main_focus_ == MainFocus::LedSource) {
                int s = static_cast<int>(led.source);
                s += right ? 1 : -1;
                int max = static_cast<int>(VoiceId::Count) - 1;
                if (s < 0) s = max;
                if (s > max) s = 0;
                led.source = static_cast<LedSource>(s);
                GroovePuterState::markSceneMutated();
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
                GroovePuterState::markSceneMutated();
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
                GroovePuterState::markSceneMutated();
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
                GroovePuterState::markSceneMutated();
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
        if (main_focus_ == MainFocus::ClearProject) { openConfirmClearDialog(); return true; }

        if (main_focus_ == MainFocus::VisualStyle) { UI::currentStyle = nextStyle(UI::currentStyle); return true; }
        if (main_focus_ == MainFocus::GrooveMode) {
            withAudioGuard([&]() { mini_acid_.toggleGrooveboxMode(); });
            char toast[64];
            std::snprintf(toast, sizeof(toast), "Groove Mode: %s (override)",
                          grooveModeName(mini_acid_.grooveboxMode()));
            UI::showToast(toast);
            return true;
        }
        if (main_focus_ == MainFocus::GrooveFlavor) {
            withAudioGuard([&]() { mini_acid_.shiftGrooveFlavor(1); });
            return true;
        }
        
        auto& led = mini_acid_.sceneManager().currentScene().led;
        if (main_focus_ == MainFocus::LedMode) { led.mode = static_cast<LedMode>((static_cast<int>(led.mode) + 1) % 4); GroovePuterState::markSceneMutated(); return true; }
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
            GroovePuterState::markSceneMutated();
            return true;
        }
        if (main_focus_ == MainFocus::LedColor) {
            int currentIdx = 0;
            for (int i=0; i<6; ++i) if (TAPE_PALETTE[i].rgb.r == led.color.r && TAPE_PALETTE[i].rgb.g == led.color.g) currentIdx = i;
            led.color = TAPE_PALETTE[(currentIdx + 1) % 6].rgb;
            GroovePuterState::markSceneMutated();
            return true;
        }
        if (main_focus_ == MainFocus::LedBri) {
            int currentIdx = 0;
            for (int i=0; i<5; ++i) if (BRI_STEPS[i] == led.brightness) currentIdx = i;
            led.brightness = BRI_STEPS[(currentIdx + 1) % 5];
            GroovePuterState::markSceneMutated();
            return true;
        }
        if (main_focus_ == MainFocus::LedFlash) {
            int currentIdx = 0;
            for (int i=0; i<4; ++i) if (FLASH_STEPS[i] == led.flashMs) currentIdx = i;
            led.flashMs = FLASH_STEPS[(currentIdx + 1) % 4];
            GroovePuterState::markSceneMutated();
            return true;
        }
    }
    return false;
}

const std::string & ProjectPage::getTitle() const {
  static std::string title = "PROJECT";
  return title;
}

std::unique_ptr<MultiPageHelpDialog> ProjectPage::getHelpDialog() {
  return std::make_unique<MultiPageHelpDialog>(*this);
}

int ProjectPage::getHelpFrameCount() const {
  return 3;
}

void ProjectPage::drawHelpFrame(IGfx& gfx, int frameIndex, Rect bounds) const {
  if (bounds.w <= 0 || bounds.h <= 0) return;
  switch (frameIndex) {
    case 0:
      drawHelpPageProject(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    case 1:
      drawHelpPageMIDI(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    case 2:
      drawHelpPageSettings(gfx, bounds.x, bounds.y, bounds.w, bounds.h);
      break;
    default:
      break;
  }
}

void ProjectPage::draw(IGfx& gfx) {
  UI::drawStandardHeader(gfx, mini_acid_, "PROJECT");
  if (dialog_type_ == DialogType::MidiAdvance) {
    drawMidiAdvanceDialog(gfx);
    return;
  }
  if (dialog_type_ == DialogType::ConfirmClear) {
    drawConfirmClearDialog(gfx);
    return;
  }

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

  if (dialog_type_ == DialogType::ImportMidi) {
    GroovePuterUi::midiFileManager().draw(gfx, Layout::CONTENT, "IMPORT");
    return;
  }

  if (dialog_type_ == DialogType::Load || dialog_type_ == DialogType::SaveAs) {
    const int w = Layout::CONTENT.w - 2 * Layout::CONTENT_PAD_X;
    const int h = Layout::CONTENT.h - 2 * Layout::CONTENT_PAD_Y;
    const int yStart = Layout::CONTENT.y + Layout::CONTENT_PAD_Y;
    int dialogW = w - 16;
    if (dialogW < 80) dialogW = w - 4;
    int dialogH = h - 16;
    if (dialogH < 70) dialogH = h - 4;
    const int dialogX = x + (w - dialogW) / 2;
    const int dialogY = yStart + (h - dialogH) / 2;

    gfx.fillRect(dialogX, dialogY, dialogW, dialogH, COLOR_DARKER);
    gfx.drawRect(dialogX, dialogY, dialogW, dialogH, COLOR_ACCENT);

    if (dialog_type_ == DialogType::Load) {
      const int headerH = line_h + 4;
      const int cancelH = line_h + 8;
      const int listY = dialogY + headerH + 2;
      const int listH = dialogH - headerH - cancelH - 10;
      const int rowH = line_h + 3;
      const int visibleRows = std::max(1, listH / rowH);
      ensureSelectionVisible(visibleRows);
      gfx.setTextColor(COLOR_WHITE);
      gfx.drawText(dialogX + 4, dialogY + 2, "Load Scene");

      if (scenes_.empty()) {
        gfx.setTextColor(COLOR_LABEL);
        gfx.drawText(dialogX + 6, listY + 2, "No files");
      } else {
        const int rows = std::min(visibleRows,
                                  static_cast<int>(scenes_.size()) - scroll_offset_);
        for (int row = 0; row < rows; ++row) {
          const int index = scroll_offset_ + row;
          const int rowY = listY + row * rowH;
          const bool selected = index == selection_index_;
          if (selected) {
            gfx.fillRect(dialogX + 2, rowY, dialogW - 4, rowH, COLOR_PANEL);
            gfx.drawRect(dialogX + 2, rowY, dialogW - 4, rowH, COLOR_ACCENT);
          }
          gfx.setTextColor(selected ? COLOR_WHITE : COLOR_LABEL);
          Widgets::drawClippedText(gfx, dialogX + 6, rowY + 1,
                                   dialogW - 12, scenes_[index].c_str());
        }
      }

      const int buttonWidth = 60;
      const int buttonX = dialogX + (dialogW - buttonWidth) / 2;
      const int buttonY = dialogY + dialogH - cancelH - 4;
      const bool focused = dialog_focus_ == DialogFocus::Cancel;
      gfx.fillRect(buttonX, buttonY, buttonWidth, cancelH, COLOR_PANEL);
      gfx.drawRect(buttonX, buttonY, buttonWidth, cancelH,
                   focused ? COLOR_ACCENT : COLOR_LABEL);
      gfx.setTextColor(focused ? COLOR_WHITE : COLOR_LABEL);
      gfx.drawText(buttonX + (buttonWidth - textWidth(gfx, "Cancel")) / 2,
                   buttonY + (cancelH - line_h) / 2, "Cancel");
      return;
    }

    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(dialogX + 4, dialogY + 2, "Save Scene");
    const int inputH = line_h + 8;
    const int inputY = dialogY + line_h + 6;
    gfx.fillRect(dialogX + 4, inputY, dialogW - 8, inputH, COLOR_PANEL);
    const bool inputFocused = save_dialog_focus_ == SaveDialogFocus::Input;
    gfx.drawRect(dialogX + 4, inputY, dialogW - 8, inputH,
                 inputFocused ? COLOR_ACCENT : COLOR_LABEL);
    gfx.setTextColor(COLOR_WHITE);
    gfx.drawText(dialogX + 8, inputY + (inputH - line_h) / 2,
                 save_name_.c_str());

    const char* buttons[] = {"RND", "SAVE", "ESC"};
    const SaveDialogFocus focuses[] = {
        SaveDialogFocus::Randomize,
        SaveDialogFocus::Save,
        SaveDialogFocus::Cancel,
    };
    const int buttonWidth = (dialogW - 16) / 3;
    for (int index = 0; index < 3; ++index) {
      const int buttonX = dialogX + 4 + index * (buttonWidth + 4);
      const int buttonY = inputY + inputH + 6;
      const bool focused = save_dialog_focus_ == focuses[index];
      gfx.fillRect(buttonX, buttonY, buttonWidth, line_h + 8, COLOR_PANEL);
      gfx.drawRect(buttonX, buttonY, buttonWidth, line_h + 8,
                   focused ? COLOR_ACCENT : COLOR_LABEL);
      gfx.setTextColor(focused ? COLOR_WHITE : COLOR_LABEL);
      gfx.drawText(buttonX + (buttonWidth - textWidth(gfx, buttons[index])) / 2,
                   buttonY + 4, buttons[index]);
    }
    return;
  }

  // Main Page Drawing
  const int rowBase = 2;
  const int visibleRows = 8;
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
      case MainFocus::ClearProject: std::snprintf(line, sizeof(line), "Clear Project"); break;
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

}

int ProjectPage::firstFocusInSection(int sectionIdx) {
  if (sectionIdx == 0) return (int)ProjectPage::MainFocus::Load;
  if (sectionIdx == 1) return (int)ProjectPage::MainFocus::VisualStyle;
  if (sectionIdx == 2) return (int)ProjectPage::MainFocus::LedMode;
  return 0;
}

int ProjectPage::lastFocusInSection(int sectionIdx) {
  if (sectionIdx == 0) return (int)ProjectPage::MainFocus::ClearProject;
  if (sectionIdx == 1) return (int)ProjectPage::MainFocus::Volume;
  if (sectionIdx == 2) return (int)ProjectPage::MainFocus::LedFlash;
  return 0;
}

bool ProjectPage::focusInSection(int sectionIdx, int focusIdx) {
  ProjectPage::MainFocus f = static_cast<ProjectPage::MainFocus>(focusIdx);
  if (sectionIdx == 0) return f >= ProjectPage::MainFocus::Load && f <= ProjectPage::MainFocus::ClearProject;
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
