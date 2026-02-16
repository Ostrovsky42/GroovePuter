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
  midi_dirs_.clear();
  if (!SD.exists(midi_current_path_.c_str())) {
      SD.mkdir(midi_current_path_.c_str());
  }
  File root = SD.open(midi_current_path_.c_str());
  if (!root) return;
  while (true) {
    File entry = root.openNextFile();
    if (!entry) break;
    std::string name = entry.name();
    if (entry.isDirectory()) {
      // Only show immediate child name, not full path
      size_t lastSlash = name.rfind('/');
      if (lastSlash != std::string::npos) name = name.substr(lastSlash + 1);
      if (!name.empty() && name[0] != '.') {
        midi_dirs_.push_back(name);
      }
    } else {
      size_t lastSlash = name.rfind('/');
      if (lastSlash != std::string::npos) name = name.substr(lastSlash + 1);
      if (name.size() > 4 && name.substr(name.size() - 4) == ".mid") {
        midi_files_.push_back(name);
      }
    }
    entry.close();
  }
  root.close();
  std::sort(midi_dirs_.begin(), midi_dirs_.end());
  std::sort(midi_files_.begin(), midi_files_.end());
}

bool ProjectPage::navigateIntoMidiDir(const std::string& dirName) {
  std::string newPath = midi_current_path_ + "/" + dirName;
  if (!SD.exists(newPath.c_str())) return false;
  midi_current_path_ = newPath;
  refreshMidiFiles();
  selection_index_ = 0;
  scroll_offset_ = 0;
  return true;
}

bool ProjectPage::navigateUpMidiDir() {
  if (midi_current_path_ == "/midi" || midi_current_path_.empty()) return false;
  size_t lastSlash = midi_current_path_.rfind('/');
  if (lastSlash == std::string::npos || lastSlash == 0) {
    midi_current_path_ = "/midi";
  } else {
    midi_current_path_ = midi_current_path_.substr(0, lastSlash);
  }
  if (midi_current_path_.size() < 5) midi_current_path_ = "/midi"; // safety
  refreshMidiFiles();
  selection_index_ = 0;
  scroll_offset_ = 0;
  return true;
}

bool ProjectPage::isMidiDirEntry(int index) const {
  // The combined list is: [.. (if not root)] + dirs + files
  bool hasParent = (midi_current_path_ != "/midi");
  if (hasParent) {
    if (index == 0) return true; // ".." entry
    index--;
  }
  return index < (int)midi_dirs_.size();
}

std::string ProjectPage::midiDisplayName(int index) const {
  bool hasParent = (midi_current_path_ != "/midi");
  if (hasParent) {
    if (index == 0) return "..";
    index--;
  }
  if (index < (int)midi_dirs_.size()) {
    return midi_dirs_[index];
  }
  int fileIdx = index - (int)midi_dirs_.size();
  if (fileIdx >= 0 && fileIdx < (int)midi_files_.size()) {
    return midi_files_[fileIdx];
  }
  return "?";
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
  midi_mask_a_ = 0;
  midi_mask_b_ = 0;
  midi_mask_d_ = 0;
  midi_import_append_ = false;
  midi_current_path_ = "/midi";
}

void ProjectPage::openMidiAdvanceDialog() {
  dialog_type_ = DialogType::MidiAdvance;
  midi_advance_focus_ = MidiAdvanceFocus::Mode; // Start at top of list
  midi_adv_scroll_ = 0;
  
  // Resolve selected file path
  bool hasParent = (midi_current_path_ != "/midi");
  int offset = hasParent ? 1 : 0;
  int fileIdx = selection_index_ - offset - (int)midi_dirs_.size();
  
  if (fileIdx >= 0 && fileIdx < (int)midi_files_.size()) {
      std::string path = midi_current_path_ + "/" + midi_files_[fileIdx];
      UI::showToast("Scanning MIDI...");
      MidiImporter importer(mini_acid_);
      midi_scan_ = importer.scanFile(path);
      
      if (midi_scan_.valid) {
          autoRouteMidi();
          if (midi_scan_.estimatedBars > 0) {
              midi_import_length_bars_ = midi_scan_.estimatedBars;
          }
      }
  } else {
      midi_scan_ = MidiImporter::ScanResult(); // Invalid scan
  }
  }


void ProjectPage::autoRouteMidi() {
    // Reset masks
    midi_mask_a_ = 0;
    midi_mask_b_ = 0;
    midi_mask_d_ = 0;

    // 1. Drums: Preference for Ch 10 OR track names containing "drum", "perc", "hit"
    int drumCh = 10;
    int maxNotes = 0;
    
    // Scan for drum keywords in names first
    for(int i=0; i<16; i++) {
        std::string name = midi_scan_.channels[i].trackName;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (name.find("drum") != std::string::npos || name.find("perc") != std::string::npos) {
            drumCh = i + 1;
            break; 
        }
        if (midi_scan_.channels[i].noteCount > maxNotes) {
            maxNotes = midi_scan_.channels[i].noteCount;
            if (!midi_scan_.channels[9].used()) drumCh = i + 1; // Only switch from 10 if 10 is empty
        }
    }
    if (midi_scan_.channels[9].used()) drumCh = 10; // Ch 10 is king for GM

    midi_mask_d_ |= (1 << (drumCh - 1));

    // 2. Synths: Find candidate channels
    std::vector<int> candidates;
    for (int i = 0; i < 16; i++) {
        if (i == (drumCh - 1)) continue;
        if (midi_scan_.channels[i].used()) candidates.push_back(i + 1);
    }

    // Keyword scan for A (Bass) and B (Acid/Lead)
    int foundA = -1, foundB = -1;
    for (int chNum : candidates) {
        std::string name = midi_scan_.channels[chNum-1].trackName;
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);
        if (foundA < 0 && (name.find("bass") != std::string::npos || name.find("303") != std::string::npos)) {
            foundA = chNum;
        } else if (foundB < 0 && (name.find("acid") != std::string::npos || name.find("lead") != std::string::npos || name.find("arp") != std::string::npos)) {
            foundB = chNum;
        }
    }

    // Sort remainders by pitch
    std::sort(candidates.begin(), candidates.end(), [this](int a, int b) {
        return midi_scan_.channels[a-1].minNote < midi_scan_.channels[b-1].minNote;
    });

    if (foundA > 0) midi_mask_a_ |= (1 << (foundA - 1));
    else if (candidates.size() >= 1) midi_mask_a_ |= (1 << (candidates[0] - 1));
    
    // Default fallback
    if (midi_mask_a_ == 0 && midi_scan_.channels[0].used()) midi_mask_a_ |= 1;

    if (foundB > 0) midi_mask_b_ |= (1 << (foundB - 1));
    else if (candidates.size() >= 2) {
        // Pick the next one that isn't routed to A
        for(int c : candidates) { 
            if(!((midi_mask_a_ >> (c-1)) & 1)) { 
                midi_mask_b_ |= (1 << (c - 1)); 
                break; 
            } 
        }
    } else {
        // If B empty, map to A2 or just 2
         if (midi_mask_b_ == 0) {
             int ch2 = 2;
             if (!((midi_mask_a_ >> (ch2-1)) & 1) && !((midi_mask_d_ >> (ch2-1)) & 1)) {
                 midi_mask_b_ |= (1 << (ch2 - 1));
             }
         }
    }
    
    // Auto-profile: Loud for Perturbator/Hotline
    std::string path = midi_current_path_;
    std::transform(path.begin(), path.end(), path.begin(), ::tolower);
    if (path.find("hotline") != std::string::npos || path.find("perturbator") != std::string::npos || path.find("disco") != std::string::npos) {
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
  refreshScenes();
}

bool ProjectPage::importMidiAtSelection() {
  // Map from combined list index to actual file
  bool hasParent = (midi_current_path_ != "/midi");
  int offset = hasParent ? 1 : 0;
  int fileIdx = selection_index_ - offset - (int)midi_dirs_.size();
  if (fileIdx < 0 || fileIdx >= (int)midi_files_.size()) return true;
  
  std::string filename = midi_files_[fileIdx];
  std::string path = midi_current_path_ + "/" + filename;
  Serial.printf("[ProjectPage] Import MIDI: %s\n", path.c_str());
  
  // Auto-switch engine for specific genres/files
  std::string lowerPath = path;
  std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
  if (lowerPath.find("disco") != std::string::npos || lowerPath.find("perturbator") != std::string::npos || lowerPath.find("909") != std::string::npos) {
      mini_acid_.setDrumEngine("909");
  } else if (lowerPath.find("808") != std::string::npos || lowerPath.find("trap") != std::string::npos || lowerPath.find("hiphop") != std::string::npos) {
      mini_acid_.setDrumEngine("808");
  }

  MidiImporter importer(mini_acid_);

  MidiImporter::ImportSettings settings;
  if (midi_import_start_pattern_ < 0) midi_import_start_pattern_ = 0;
  if (midi_import_start_pattern_ > 127) midi_import_start_pattern_ = 127;
  
  int actualStartPattern = midi_import_start_pattern_;
  if (midi_import_append_) {
    int lastRow = mini_acid_.sceneManager().currentScene().songs[mini_acid_.sceneManager().activeSongSlot()].length;
    // For simplicity, we just find a free block after pattern 100 or something?
    // Actually, "Append" in song context usually means after the last used pattern in song.
    // But here we'll just use the pattern index the user picked, 
    // OR if they used 'F' they already have a good pattern index.
  }

  if (midi_import_from_bar_ < 0) midi_import_from_bar_ = 0;
  if (midi_import_from_bar_ > 511) midi_import_from_bar_ = 511;
  if (midi_import_length_bars_ < 0) midi_import_length_bars_ = 0;
  if (midi_import_length_bars_ > 256) midi_import_length_bars_ = 256;
  settings.targetPatternIndex = actualStartPattern;
  settings.startStepOffset = 0;
  settings.sourceStartBar = midi_import_from_bar_;
  settings.sourceLengthBars = midi_import_length_bars_;
  settings.overwrite = true; // Implicit
  settings.loudMode = (midi_import_profile_ == MidiImportProfile::Loud);
  settings.synthAMask = midi_mask_a_;
  settings.synthBMask = midi_mask_b_;
  settings.drumMask = midi_mask_d_;
  
  UI::showToast("Importing MIDI...");
  
  MidiImporter::Error err;
  bool omniFallbackUsed = false;
  withAudioGuard([&]() {
    bool wasPlaying = mini_acid_.isPlaying();
    if (wasPlaying) {
      mini_acid_.stop();
    }
    err = importer.importFile(path, settings);
    // Omni fallback removed as matrix routing replaces it
    
    // If successful and append mode is on, update the song structure
    if (err == MidiImporter::Error::None && midi_import_append_) {
        auto& sm = mini_acid_.sceneManager();
        int songLen = sm.currentScene().songs[sm.activeSongSlot()].length;
        int importLenPatterns = (midi_import_length_bars_ > 0) ? midi_import_length_bars_ : 
                                (importer.getLastImportedPatternIdx() - settings.targetPatternIndex + 1);
        if (importLenPatterns < 1) importLenPatterns = 1;
        
        for (int i = 0; i < importLenPatterns; ++i) {
            int songPos = songLen + i;
            if (songPos < Song::kMaxPositions) {
                if (midi_mask_a_) sm.setSongPattern(songPos, SongTrack::SynthA, settings.targetPatternIndex + i);
                if (midi_mask_b_) sm.setSongPattern(songPos, SongTrack::SynthB, settings.targetPatternIndex + i);
                if (midi_mask_d_) sm.setSongPattern(songPos, SongTrack::Drums, settings.targetPatternIndex + i);
            }
        }
    }

    if (wasPlaying) {
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

        // --- NEW: Automate song population ---
        int startPat = settings.targetPatternIndex;
        int bars = settings.sourceLengthBars;
        
        // If AUTO (0), determine actual bars from the importer's last written pattern
        if (bars == 0) {
            int lastPat = importer.getLastImportedPatternIdx();
            if (lastPat >= startPat) {
                bars = lastPat - startPat + 1;
            }
        }
        
        int songPos = mini_acid_.currentSongPosition();
        
        // Populate patterns into the song timeline
        for (int i = 0; i < bars; ++i) {
            int targetPos = songPos + i;
            if (targetPos >= Song::kMaxPositions) break;
            int patToAssign = startPat + i;
            if (patToAssign >= kMaxPatterns) break;
            
            // Assign to all 3 main tracks
            mini_acid_.setSongPattern(targetPos, SongTrack::SynthA, patToAssign);
            mini_acid_.setSongPattern(targetPos, SongTrack::SynthB, patToAssign);
            mini_acid_.setSongPattern(targetPos, SongTrack::Drums, patToAssign);
        }
        
        // Automatically extend song length if necessary
        if (mini_acid_.songLength() < songPos + bars) {
            mini_acid_.setSongLength(songPos + bars);
        }
      });
      if (omniFallbackUsed) {
        UI::showToast("Imported via OMNI (check channels)");
      } else {
        UI::showToast("Import Successful");
      }
      closeDialog();
  } else {
      UI::showToast(importer.getErrorString(err).c_str());
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
    // Only delete actual files, not directories
    if (isMidiDirEntry(selection_index_)) return true; // can't delete dirs from here
    bool hasParent = (midi_current_path_ != "/midi");
    int offset = hasParent ? 1 : 0;
    int fileIdx = selection_index_ - offset - (int)midi_dirs_.size();
    if (fileIdx < 0 || fileIdx >= (int)midi_files_.size()) return true;
    std::string path = midi_current_path_ + "/" + midi_files_[fileIdx];
    bool removed = SD.remove(path.c_str());
    if (removed) {
      UI::showToast("MIDI deleted");
      refreshMidiFiles();
      int totalItems = (hasParent ? 1 : 0) + (int)midi_dirs_.size() + (int)midi_files_.size();
      if (selection_index_ >= totalItems) selection_index_ = totalItems - 1;
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

  // Compute list size based on dialog type
  int listCount;
  if (dialog_type_ == DialogType::ImportMidi) {
    bool hasParent = (midi_current_path_ != "/midi");
    listCount = (hasParent ? 1 : 0) + (int)midi_dirs_.size() + (int)midi_files_.size();
  } else if (dialog_type_ == DialogType::Load) {
    listCount = (int)scenes_.size();
  } else {
    listCount = (int)scenes_.size();
  }

  if (listCount <= 0) {
    scroll_offset_ = 0;
    selection_index_ = 0;
    return;
  }
  int maxIdx = listCount - 1;
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

    if (dialog_type_ == DialogType::Load || dialog_type_ == DialogType::ImportMidi) {
        // For MIDI dialog, combined list size = [..] + dirs + files
        int listSize;
        if (dialog_type_ == DialogType::ImportMidi) {
            bool hasParent = (midi_current_path_ != "/midi");
            listSize = (hasParent ? 1 : 0) + (int)midi_dirs_.size() + (int)midi_files_.size();
        } else {
            listSize = (int)scenes_.size();
        }
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
                    if (selection_index_ >= listSize) selection_index_ = listSize - 1;
                    if (selection_index_ < 0) selection_index_ = 0;
                    ensureSelectionVisible(10);
                    return true; 
                }
                break;
            default: break;
        }
        char key = ui_event.key;
        if (dialog_type_ == DialogType::ImportMidi) {
            if (key == '\t') { openMidiAdvanceDialog(); return true; }
        }
        if (key == 'x' || key == 'X') {
            return deleteSelectionInDialog();
        }
        if (key == '\n' || key == '\r') {
            if (dialog_focus_ == DialogFocus::Cancel) { closeDialog(); return true; }
            if (dialog_type_ == DialogType::Load) return loadSceneAtSelection();
            // MIDI import: folder navigation
            if (isMidiDirEntry(selection_index_)) {
                std::string name = midiDisplayName(selection_index_);
                if (name == "..") {
                    navigateUpMidiDir();
                } else {
                    navigateIntoMidiDir(name);
                }
                return true;
            }
            openMidiAdvanceDialog(); return true;
        }
        if (key == '\b') {
            if (dialog_type_ == DialogType::ImportMidi && midi_current_path_ != "/midi") {
                navigateUpMidiDir();
                return true;
            }
            closeDialog(); return true;
        }
        return false;
    }

    if (dialog_type_ == DialogType::MidiAdvance) {
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
                    if (midi_import_start_pattern_ > 127) midi_import_start_pattern_ = 127;
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

  if (dialog_type_ != DialogType::None) {
    // Shared list-based dialog drawing (Load, Import, SaveAs)
    int w = Layout::CONTENT.w - 2 * Layout::CONTENT_PAD_X;
    int h = Layout::CONTENT.h - 2 * Layout::CONTENT_PAD_Y;
    int y_start = Layout::CONTENT.y + Layout::CONTENT_PAD_Y;
    int dialog_w = w - 16;
    if (dialog_w < 80) dialog_w = w - 4;
    int dialog_h = h - 16;
    if (dialog_h < 70) dialog_h = h - 4;
    int dialog_x = x + (w - dialog_w) / 2;
    int dialog_y = y_start + (h - dialog_h) / 2;

    gfx.fillRect(dialog_x, dialog_y, dialog_w, dialog_h, COLOR_DARKER);
    gfx.drawRect(dialog_x, dialog_y, dialog_w, dialog_h, COLOR_ACCENT);

    if (dialog_type_ == DialogType::Load || dialog_type_ == DialogType::ImportMidi) {
        int header_h = line_h + 4;
        gfx.setTextColor(COLOR_WHITE);
        if (dialog_type_ == DialogType::Load) {
            gfx.drawText(dialog_x + 4, dialog_y + 2, "Load Scene");
        } else {
            // Breadcrumb for MIDI folder navigation
            std::string breadcrumb = midi_current_path_;
            if (breadcrumb.size() > (size_t)(dialog_w / 6 - 2)) {
                // Truncate from the left
                breadcrumb = ".." + breadcrumb.substr(breadcrumb.size() - dialog_w / 6 + 4);
            }
            gfx.drawText(dialog_x + 4, dialog_y + 2, breadcrumb.c_str());
        }
        
        int row_h = line_h + 3;
        int cancel_h = line_h + 8;
        int footer_h = (dialog_type_ == DialogType::ImportMidi) ? (line_h + 2) : 0;
        int list_y = dialog_y + header_h + 2;
        int list_h = dialog_h - header_h - cancel_h - footer_h - 10;
        int visible_rows = list_h / row_h;
        if (visible_rows < 1) visible_rows = 1;
        ensureSelectionVisible(visible_rows);

        if (dialog_type_ == DialogType::Load) {
            // Original scene list drawing
            if (scenes_.empty()) {
                gfx.setTextColor(COLOR_LABEL);
                gfx.drawText(dialog_x + 6, list_y + 2, "No files");
            } else {
                int rows = std::min(visible_rows, (int)scenes_.size() - scroll_offset_);
                for (int i = 0; i < rows; ++i) {
                    int idx = scroll_offset_ + i;
                    int ry = list_y + i * row_h;
                    bool sel = (idx == selection_index_);
                    if (sel) {
                        gfx.fillRect(dialog_x + 2, ry, dialog_w - 4, row_h, COLOR_PANEL);
                        gfx.drawRect(dialog_x + 2, ry, dialog_w - 4, row_h, COLOR_ACCENT);
                    }
                    Widgets::drawClippedText(gfx, dialog_x + 6, ry + 1, dialog_w - 12, scenes_[idx].c_str());
                }
            }
        } else {
            // MIDI folder + file list drawing
            bool hasParent = (midi_current_path_ != "/midi");
            int totalItems = (hasParent ? 1 : 0) + (int)midi_dirs_.size() + (int)midi_files_.size();
            if (totalItems == 0) {
                gfx.setTextColor(COLOR_LABEL);
                gfx.drawText(dialog_x + 6, list_y + 2, "No files");
            } else {
                int rows = std::min(visible_rows, totalItems - scroll_offset_);
                for (int i = 0; i < rows; ++i) {
                    int idx = scroll_offset_ + i;
                    int ry = list_y + i * row_h;
                    bool sel = (idx == selection_index_);
                    bool isDir = isMidiDirEntry(idx);
                    if (sel) {
                        gfx.fillRect(dialog_x + 2, ry, dialog_w - 4, row_h, COLOR_PANEL);
                        gfx.drawRect(dialog_x + 2, ry, dialog_w - 4, row_h, COLOR_ACCENT);
                    }
                    std::string displayName = midiDisplayName(idx);
                    if (isDir) {
                        // Folder marker
                        std::string label = "[DIR] " + displayName;
                        if (sel) gfx.setTextColor(COLOR_ACCENT);
                        else gfx.setTextColor(IGfxColor(0xB4DCFF));
                        Widgets::drawClippedText(gfx, dialog_x + 6, ry + 1, dialog_w - 12, label.c_str());
                    } else {
                        gfx.setTextColor(sel ? COLOR_WHITE : COLOR_LABEL);
                        Widgets::drawClippedText(gfx, dialog_x + 6, ry + 1, dialog_w - 12, displayName.c_str());
                    }
                }
            }
            // File count footer
            char countBuf[48];
            std::snprintf(countBuf, sizeof(countBuf), "%d dirs  %d files", (int)midi_dirs_.size(), (int)midi_files_.size());
            gfx.setTextColor(COLOR_LABEL);
            gfx.drawText(dialog_x + 6, dialog_y + dialog_h - cancel_h - footer_h - 2, countBuf);
        }
        // Cancel button
        int btnW = 60;
        int btnX = dialog_x + (dialog_w - btnW) / 2;
        int btnY = dialog_y + dialog_h - cancel_h - 4;
        bool focused = (dialog_focus_ == DialogFocus::Cancel);
        gfx.fillRect(btnX, btnY, btnW, cancel_h, COLOR_PANEL);
        gfx.drawRect(btnX, btnY, btnW, cancel_h, focused ? COLOR_ACCENT : COLOR_LABEL);
        gfx.setTextColor(focused ? COLOR_WHITE : COLOR_LABEL);
        gfx.drawText(btnX + (btnW - textWidth(gfx, "Cancel"))/2, btnY + (cancel_h - line_h)/2, "Cancel");
        return;
    }

    if (dialog_type_ == DialogType::SaveAs) {
        gfx.setTextColor(COLOR_WHITE);
        gfx.drawText(dialog_x + 4, dialog_y + 2, "Save Scene");
        int input_h = line_h + 8;
        int input_y = dialog_y + line_h + 6;
        gfx.fillRect(dialog_x + 4, input_y, dialog_w - 8, input_h, COLOR_PANEL);
        bool focused = (save_dialog_focus_ == SaveDialogFocus::Input);
        gfx.drawRect(dialog_x + 4, input_y, dialog_w - 8, input_h, focused ? COLOR_ACCENT : COLOR_LABEL);
        gfx.drawText(dialog_x + 8, input_y + (input_h - line_h)/2, save_name_.c_str());

        const char* btns[] = {"RND", "SAVE", "ESC"};
        SaveDialogFocus focuses[] = {SaveDialogFocus::Randomize, SaveDialogFocus::Save, SaveDialogFocus::Cancel};
        int bw = (dialog_w - 16) / 3;
        for (int i=0; i<3; ++i) {
            int bx = dialog_x + 4 + i * (bw + 4);
            int by = input_y + input_h + 6;
            bool f = (save_dialog_focus_ == focuses[i]);
            gfx.fillRect(bx, by, bw, line_h + 8, COLOR_PANEL);
            gfx.drawRect(bx, by, bw, line_h + 8, f ? COLOR_ACCENT : COLOR_LABEL);
            gfx.setTextColor(f ? COLOR_WHITE : COLOR_LABEL);
            gfx.drawText(bx + (bw - textWidth(gfx, btns[i]))/2, by + 4, btns[i]);
        }
        return;
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
