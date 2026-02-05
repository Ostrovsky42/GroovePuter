#include "voice_page.h"
#include "../ui_input.h"
#include <cstdio>
#include <algorithm>
#include <cctype>

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
inline constexpr IGfxColor kVoiceColor = IGfxColor(0x00CED1);  // Dark cyan
inline constexpr IGfxColor kActiveColor = IGfxColor(0x00FF7F); // Spring green
inline constexpr IGfxColor kCacheColor = IGfxColor(0xFFD700);  // Gold for cached items

// Built-in phrase names for display and playback
// NOTE: SAM TTS is phoneme-based. Non-English words are transliterated.
const char* const BUILTIN_PHRASE_NAMES[] = {
    // === MUSICAL COMMANDS ===
    "Tek no tek no tek h no ",        // 1: Techno
    "Mi ni mal",                   // 2: Minimal
    "Kur wa bo ber, ia per do le",              // 4: Kurwa bober
    "Er ror",                     // 10: Error
    "Bee P auM",                   // 11: BPM
    //"O ver ride",                 // 13: Override
    "ku ra chu",                   // 14: Welcome
    "Wat sap   be acth",                   // 15: What's up
    "pich ku mate ri nu",          // 28: Human detected
   // "Self de struct",             // 26: Self destruct
    "oh no oh no      oh no no non non onono",                   // 17: Loading
    "Press an y key",             // 31: Press any key

    
    // === USEFUL / STANDARD ===
    "Yes",                        // 32
    "No",                         // 33
    "Ok ay",                      // 34: OK
    "Thank you",                  // 35
    "Sor ree",                    // 36: Sorry
    "Warn ing",                   // 37
    "Com plete",                  // 38
    "Fail ure",                   // 39
    "Bat ter ry low",             // 40
    "Con nect ed",                // 41
    "Dis con nect ed",            // 42
    "Secu ri ty breach",          // 43
    "Le vel up",                  // 44
    "Game o ver",                 // 45
    "Vic tor ry",                 // 46
};
const int NUM_BUILTIN_PHRASES = sizeof(BUILTIN_PHRASE_NAMES) / sizeof(BUILTIN_PHRASE_NAMES[0]);

} // namespace

VoicePage::VoicePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard) {
  // Set OPTIMAL defaults for intelligibility on first open (Sweet Spot)
  // Slower speed and slightly lower pitch help SAM sound clearer on small speakers.
  withAudioGuard([&]() {
    auto& synth = mini_acid_.vocalSynth();
    // Only set if parameters are at their factory-default high values
    if (synth.pitch() > 140.0f && synth.speed() > 1.1f) { 
        synth.setPitch(120.0f);   // "Sweet spot": slightly deeper
        synth.setSpeed(0.95f);    // "Sweet spot": slightly slower
        synth.setRobotness(0.0f); // Pure monotone is actually clearer
        synth.setVolume(1.0f);    // Maximum presence
    }
  });
}

void VoicePage::setBoundaries(const Rect& rect) {
  Frame::setBoundaries(rect);
}

void VoicePage::drawPhraseSection(IGfx& gfx, int y) {
  int x = dx() + 4;
  char buf[64];
  
  // Phrase Type row
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x, y, "TYPE:");
  gfx.setTextColor(kVoiceColor);
  const char* typeStr = useCustomPhrase_ ? "CUSTOM" : "BUILTIN";
  gfx.drawText(x + 35, y, typeStr);
  
  if (focus_ == FocusItem::PhraseType) {
    gfx.drawRect(x + 33, y - 1, 50, 10, kFocusColor);
  }
  
  y += 12;
  
  // Phrase Index row
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x, y, "PHR:");
  gfx.setTextColor(kActiveColor);
  
  if (useCustomPhrase_) {
    const char* customPhrase = mini_acid_.vocalSynth().getCustomPhrase(phraseIndex_);
    if (customPhrase && customPhrase[0] != '\0') {
      snprintf(buf, sizeof(buf), "%d: %s", phraseIndex_ + 1, customPhrase);
    } else {
      snprintf(buf, sizeof(buf), "%d: (empty)", phraseIndex_ + 1);
    }
  } else {
    if (phraseIndex_ >= 0 && phraseIndex_ < NUM_BUILTIN_PHRASES) {
      // Ensure we don't overflow the display buffer
      const char* phraseText = BUILTIN_PHRASE_NAMES[phraseIndex_];
      bool isCached = mini_acid_.voiceCache().isCached(phraseText);
      
      if (isCached) {
        gfx.setTextColor(kCacheColor); // Gold for cached
        snprintf(buf, sizeof(buf), "%d: %.45s [C]", phraseIndex_ + 1, phraseText);
      } else {
        snprintf(buf, sizeof(buf), "%d: %.50s", phraseIndex_ + 1, phraseText);
      }
    } else {
      snprintf(buf, sizeof(buf), "%d: ???", phraseIndex_ + 1);
    }
  }
  gfx.drawText(x + 30, y, buf);
  
  if (focus_ == FocusItem::PhraseIndex) {
    gfx.drawRect(x + 28, y - 1, width() - 40, 10, kFocusColor);
  }
}

void VoicePage::drawParameterSection(IGfx& gfx, int y) {
  int x = dx() + 4;
  int midX = x + (width() - 8) / 2;
  char buf[32];
  
  auto& synth = mini_acid_.vocalSynth();
  
  // Pitch row (with intelligibility indicator)
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x, y, "PCH:");
  
  float pitch = synth.pitch();
  IGfxColor pitchColor;
  if (pitch < 100.0f) {
    pitchColor = COLOR_RED;          // Too low, muddy
  } else if (pitch < 200.0f) {
    pitchColor = COLOR_KNOB_1;       // Good range
  } else if (pitch < 300.0f) {
    pitchColor = IGfxColor(0x00FF7F); // High but OK
  } else {
    pitchColor = COLOR_RED;          // Too high, shrill
  }
  
  gfx.setTextColor(pitchColor);
  snprintf(buf, sizeof(buf), "%.0f Hz", pitch);
  gfx.drawText(x + 30, y, buf);
  if (focus_ == FocusItem::Pitch) {
    gfx.drawRect(x + 28, y - 1, 55, 10, kFocusColor);
  }
  
  // Speed
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(midX, y, "SPD:");
  gfx.setTextColor(COLOR_KNOB_2);
  snprintf(buf, sizeof(buf), "%.1fx", synth.speed());
  gfx.drawText(midX + 30, y, buf);
  if (focus_ == FocusItem::Speed) {
    gfx.drawRect(midX + 28, y - 1, 40, 10, kFocusColor);
  }
  
  y += 12;
  
  // Robotness row
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x, y, "ROB:");
  gfx.setTextColor(COLOR_KNOB_3);
  snprintf(buf, sizeof(buf), "%d%%", (int)(synth.robotness() * 100));
  gfx.drawText(x + 30, y, buf);
  if (focus_ == FocusItem::Robotness) {
    gfx.drawRect(x + 28, y - 1, 40, 10, kFocusColor);
  }
  
  // Volume
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(midX, y, "VOL:");
  gfx.setTextColor(COLOR_KNOB_4);
  snprintf(buf, sizeof(buf), "%d%%", (int)(synth.volume() * 100));
  gfx.drawText(midX + 30, y, buf);
  if (focus_ == FocusItem::Volume) {
    gfx.drawRect(midX + 28, y - 1, 40, 10, kFocusColor);
  }
}

void VoicePage::drawCustomPhraseSection(IGfx& gfx, int y) {
  int x = dx() + 4;
  
  // Custom phrase edit section
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x, y, "EDIT:");
  
  if (editingCustom_) {
    // Show edit buffer with cursor
    gfx.setTextColor(kActiveColor);
    gfx.drawText(x + 35, y, editBuffer_);
    
    // Draw cursor
    int cursorX = x + 35 + editCursor_ * 6; // Approximate char width
    gfx.fillRect(cursorX, y, 2, 8, kActiveColor);
  } else {
    gfx.setTextColor(COLOR_GRAY);
    gfx.drawText(x + 35, y, "[Press Enter to edit]");
  }
  
  if (focus_ == FocusItem::CustomEdit) {
    gfx.drawRect(x + 33, y - 1, width() - 45, 10, kFocusColor);
  }
  
  y += 14;
  
  // Status line with level meter
  if (mini_acid_.vocalSynth().isSpeaking()) {
    gfx.setTextColor(kActiveColor);
    gfx.drawText(x, y, ">> Speaking");
    
    // VU meter for voice level
    float level = mini_acid_.vocalSynth().getCurrentLevel();
    int barWidth = (int)(level * 60);
    if (barWidth > 60) barWidth = 60;
    int barX = x + 70;
    
    // Background
    gfx.drawRect(barX, y, 62, 8, COLOR_GRAY);
    
    // Level bar (color-coded)
    IGfxColor barColor = IGfxColor::Green();
    if (level > 0.8f) barColor = COLOR_RED;
    else if (level > 0.5f) barColor = IGfxColor(0xB36A00); // Orange-ish
    
    if (barWidth > 0) {
      gfx.fillRect(barX + 1, y + 1, barWidth, 6, barColor);
    }
  } else if (mini_acid_.isVoiceTrackMuted()) {
    gfx.setTextColor(COLOR_RED);
    gfx.drawText(x, y, "MUTED");
  } else {
    gfx.setTextColor(COLOR_GRAY);
    gfx.drawText(x, y, "Space=Preview M=Mute");
  }
  
  // Ducking indicator
  float duckLevel = mini_acid_.getVoiceDuckingLevel();
  if (duckLevel > 0.05f) {
    gfx.setTextColor(IGfxColor(0xB36A00)); // Focus-like color
    char duckBuf[32];
    snprintf(duckBuf, sizeof(duckBuf), "Ducking: %d%%", (int)(duckLevel * 100));
    gfx.drawText(x + 110, y, duckBuf);
  }
  
  y += 12;
  gfx.setTextColor(COLOR_GRAY);
  gfx.drawText(x, y, "Presets: R=Robot H=Human D=Deep C=Chipmunk");
}

void VoicePage::draw(IGfx& gfx) {
  int y = dy() + 2;
  
  // Quality Indicator (top right)
  gfx.setTextColor(COLOR_GRAY);
  gfx.drawText(dx() + width() - 55, dy() + 2, "Qual: HI");

  // Section 1: Phrase selection
  drawPhraseSection(gfx, y);
  y += 28;
  
  // Divider
  gfx.drawLine(dx() + 2, y, dx() + width() - 2, y, COLOR_GRAY);
  y += 4;
  
  // Section 2: Parameters
  drawParameterSection(gfx, y);
  y += 28;
  
  // Divider
  gfx.drawLine(dx() + 2, y, dx() + width() - 2, y, COLOR_GRAY);
  y += 4;
  
  // Section 3: Custom phrase edit
  drawCustomPhraseSection(gfx, y);
  y += 26;

  // Section 4: Preview Area (New)
  drawPreviewSection(gfx, y);
}

void VoicePage::adjustCurrentValue(int delta) {
  auto& synth = mini_acid_.vocalSynth();
  
  withAudioGuard([&]() {
    switch (focus_) {
      case FocusItem::PhraseType:
        useCustomPhrase_ = !useCustomPhrase_;
        phraseIndex_ = 0; // Reset to first in new category
        break;
        
      case FocusItem::PhraseIndex:
        if (useCustomPhrase_) {
          phraseIndex_ = (phraseIndex_ + delta + MAX_CUSTOM_PHRASES) % MAX_CUSTOM_PHRASES;
        } else {
          phraseIndex_ = (phraseIndex_ + delta + NUM_BUILTIN_PHRASES) % NUM_BUILTIN_PHRASES;
        }
        break;
        
      case FocusItem::Pitch:
        mini_acid_.adjustParameter(MiniAcidParamId::VoicePitch, delta);
        break;
        
      case FocusItem::Speed:
        mini_acid_.adjustParameter(MiniAcidParamId::VoiceSpeed, delta);
        break;
        
      case FocusItem::Robotness:
        mini_acid_.adjustParameter(MiniAcidParamId::VoiceRobotness, delta);
        break;
        
      case FocusItem::Volume:
        mini_acid_.adjustParameter(MiniAcidParamId::VoiceVolume, delta);
        break;
        
      default:
        break;
    }
  });
}

std::string VoicePage::phoneticTransform(const std::string& input) {
    std::string output;
    output.reserve(input.length() * 2); 
    
    for (size_t i = 0; i < input.length(); ++i) {
        char c = tolower(input[i]);
        
        switch(c) {
            case 'c': 
                if (i + 1 < input.length() && tolower(input[i+1]) == 'h') {
                    output += "ch";
                    i++;
                } else if (i + 1 < input.length() && tolower(input[i+1]) == 'k') {
                    output += "k";
                    i++;
                } else {
                    output += "k";
                }
                break;
            case 'x': output += "ks"; break;
            case 'q': output += "kw"; break;
            case 'j': output += "j"; break;
            case 'y': output += "y"; break;
            case 'w': output += "w"; break;
            case 'z': output += "z"; break;
            default:
                if ((c >= 'a' && c <= 'z') || c == ' ' || (c >= '0' && c <= '9')) {
                    output += c;
                }
                break;
        }
    }
    
    std::string result;
    for (size_t i = 0; i < output.length(); i++) {
        result += output[i];
        if (i < output.length() - 1) {
            bool isConsonant = !strchr("aeiou ", output[i]);
            bool isVowel = strchr("aeiou", output[i+1]) != nullptr;
            if (isConsonant && isVowel && (rand() % 100 < 20)) { 
                result += ' ';
            }
        }
    }
    
    return result;
}

void VoicePage::triggerPreview() {
  withAudioGuard([&]() {
    if (useCustomPhrase_) {
      const char* customPhrase = mini_acid_.vocalSynth().getCustomPhrase(phraseIndex_);
      if (customPhrase && customPhrase[0] != '\0') {
          std::string phonetized = phoneticTransform(customPhrase);
          mini_acid_.vocalSynth().speak(phonetized.c_str());
          previewText_ = phonetized;
      }
    } else {
      if (phraseIndex_ >= 0 && phraseIndex_ < NUM_BUILTIN_PHRASES) {
         mini_acid_.vocalSynth().speak(BUILTIN_PHRASE_NAMES[phraseIndex_]);
         previewText_ = BUILTIN_PHRASE_NAMES[phraseIndex_];
      }
    }
  });
}

void VoicePage::drawPreviewSection(IGfx& gfx, int y) {
  gfx.drawLine(dx() + 2, y, dx() + width() - 2, y, COLOR_GRAY);
  
  int x = dx() + 4;
  gfx.setTextColor(COLOR_KNOB_1);
  gfx.drawText(x, y + 4, "PREVIEW:");
  
  gfx.setTextColor(COLOR_WHITE);
  if (!previewText_.empty()) {
    std::string disp = previewText_;
    if (disp.length() > 25) disp = disp.substr(0, 22) + "...";
    gfx.drawText(x + 55, y + 4, disp.c_str());
  } else {
    gfx.drawText(x + 55, y + 4, "Press 'a' to test");
  }
  
  if (mini_acid_.vocalSynth().isSpeaking()) {
    if (millis() - lastSpeakAnim_ > 200) {
        lastSpeakAnim_ = millis();
        speakAnimState_ = !speakAnimState_;
    }
    gfx.fillCircle(dx() + width() - 10, y + 7, 3, 
                   speakAnimState_ ? COLOR_KNOB_3 : IGfxColor(0x004400));
  }
}


void VoicePage::nextFocus() {
  int f = static_cast<int>(focus_);
  f = (f + 1) % static_cast<int>(FocusItem::Count);
  focus_ = static_cast<FocusItem>(f);
}

void VoicePage::prevFocus() {
  int f = static_cast<int>(focus_);
  f = (f - 1 + static_cast<int>(FocusItem::Count)) % static_cast<int>(FocusItem::Count);
  focus_ = static_cast<FocusItem>(f);
}

bool VoicePage::handleEvent(UIEvent& ui_event) {
  if (ui_event.event_type != MINIACID_KEY_DOWN) {
    return Container::handleEvent(ui_event);
  }

  // Handle text input when editing custom phrase
  if (editingCustom_) {
    if (UIInput::isConfirm(ui_event)) {
      // Save and exit edit mode
      withAudioGuard([&]() {
        mini_acid_.vocalSynth().setCustomPhrase(phraseIndex_, editBuffer_);
      });
      editingCustom_ = false;
      return true;
    } else if (ui_event.key == 0x1B) {  // ESC
      // Cancel edit
      editingCustom_ = false;
      return true;
    } else if (ui_event.key == 0x08) {  // Backspace
      if (editCursor_ > 0) {
        editCursor_--;
        // Shift remaining chars left
        int len = strlen(editBuffer_);
        for (int i = editCursor_; i < len; i++) {
          editBuffer_[i] = editBuffer_[i + 1];
        }
      }
      return true;
    } else if (ui_event.key >= 32 && ui_event.key < 127) {
      // Printable character
      int len = strlen(editBuffer_);
      if (len < 30) {
        // Shift chars right to make room
        for (int i = len + 1; i > editCursor_; i--) {
          editBuffer_[i] = editBuffer_[i - 1];
        }
        editBuffer_[editCursor_] = ui_event.key;
        editCursor_++;
      }
      return true;
    }
    return true;
  }

  // Normal navigation
  switch (ui_event.scancode) {
    case MINIACID_UP:
      prevFocus();
      return true;
    case MINIACID_DOWN:
      nextFocus();
      return true;
    case MINIACID_LEFT:
      adjustCurrentValue(-1);
      return true;
    case MINIACID_RIGHT:
      adjustCurrentValue(1);
      return true;
    default:
      break;
  }

  // Enter key for custom phrase editing
  if (UIInput::isConfirm(ui_event)) {
    if (focus_ == FocusItem::CustomEdit && useCustomPhrase_) {
      // Start editing custom phrase
      const char* current = mini_acid_.vocalSynth().getCustomPhrase(phraseIndex_);
      strncpy(editBuffer_, current ? current : "", sizeof(editBuffer_) - 1);
      editBuffer_[sizeof(editBuffer_) - 1] = '\0';
      editCursor_ = strlen(editBuffer_);
      editingCustom_ = true;
      return true;
    }
  }

  char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(ui_event.key)));
  
  // Space = preview
  if (ui_event.key == ' ') {
    triggerPreview();
    return true;
  }
  
  // M = toggle mute
  if (lowerKey == 'm') {
    withAudioGuard([&]() {
      mini_acid_.toggleVoiceTrackMute();
    });
    return true;
  }
  
  // S = stop speaking
  if (lowerKey == 's') {
    withAudioGuard([&]() {
      mini_acid_.stopSpeaking();
    });
    return true;
  }
  
  // C = cache current phrase to SD card
  if (lowerKey == 'c') {
    if (!useCustomPhrase_ && phraseIndex_ >= 0 && phraseIndex_ < NUM_BUILTIN_PHRASES) {
      const char* phraseText = BUILTIN_PHRASE_NAMES[phraseIndex_];
      if (!mini_acid_.voiceCache().isCached(phraseText)) {
        // TODO: Synthesize phrase to buffer and cache
        // For now, just show message via Serial
        Serial.printf("[VoicePage] Cache request for: %s\n", phraseText);
        // This would require synthesizing to a buffer first
        // which needs FormantSynth modification
      }
    }
    return true;
  }
  
  // X = clear voice cache
  if (lowerKey == 'x') {
    mini_acid_.voiceCache().clearAll();
    Serial.println("[VoicePage] Voice cache cleared");
    return true;
  }

  
  // Number keys 1-9 = quick phrase selection
  if (ui_event.key >= '1' && ui_event.key <= '9') {
    int idx = ui_event.key - '1';
    if (!useCustomPhrase_ && idx < NUM_BUILTIN_PHRASES) {
      phraseIndex_ = idx;
      triggerPreview();
    } else if (useCustomPhrase_ && idx < MAX_CUSTOM_PHRASES) {
      phraseIndex_ = idx;
      triggerPreview();
    }
    return true;
  }
  
  // 0 = phrase 10
  if (ui_event.key == '0') {
    phraseIndex_ = 9;
    if (!useCustomPhrase_ && phraseIndex_ < NUM_BUILTIN_PHRASES) {
      triggerPreview();
    }
    return true;
  }

  // Presets
  if (lowerKey == 'r') {
    withAudioGuard([&]() {
      mini_acid_.setParameter(MiniAcidParamId::VoicePitch, 120.0f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceRobotness, 0.9f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceVolume, 0.8f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceSpeed, 1.0f);
    });
    return true;
  }
  if (lowerKey == 'h') {
    withAudioGuard([&]() {
      mini_acid_.setParameter(MiniAcidParamId::VoicePitch, 180.0f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceRobotness, 0.2f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceVolume, 0.7f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceSpeed, 1.2f);
    });
    return true;
  }
  if (lowerKey == 'd') {
    withAudioGuard([&]() {
      mini_acid_.setParameter(MiniAcidParamId::VoicePitch, 80.0f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceRobotness, 0.5f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceVolume, 0.9f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceSpeed, 0.8f);
    });
    return true;
  }
  if (lowerKey == 'c') {
    withAudioGuard([&]() {
      mini_acid_.setParameter(MiniAcidParamId::VoicePitch, 280.0f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceRobotness, 0.4f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceVolume, 0.6f);
      mini_acid_.setParameter(MiniAcidParamId::VoiceSpeed, 1.5f);
    });
    return true;
  }

  return Container::handleEvent(ui_event);
}

const std::string& VoicePage::getTitle() const { return title_; }