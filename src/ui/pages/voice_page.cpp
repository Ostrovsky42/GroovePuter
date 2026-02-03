#include "voice_page.h"
#include "../ui_input.h"
#include <cstdio>
#include <algorithm>
#include <cctype>

namespace {
inline constexpr IGfxColor kFocusColor = IGfxColor(0xB36A00);
inline constexpr IGfxColor kVoiceColor = IGfxColor(0x00CED1);  // Dark cyan
inline constexpr IGfxColor kActiveColor = IGfxColor(0x00FF7F); // Spring green

// Built-in phrase names for display
const char* const BUILTIN_PHRASE_NAMES[] = {
    "Acid",           // 0
    "Techno",         // 1
    "Minimal",        // 2
    "Pattern One",    // 3
    "Pattern Two",    // 4
    "Ready",          // 5
    "Go",             // 6
    "Stop",           // 7
    "Recording",      // 8
    "Saved",          // 9
    "Error",          // 10
    "BPM",            // 11
    "Play",           // 12
    "Mute",           // 13
    "Welcome",        // 14
    "Goodbye",        // 15
};
const int NUM_BUILTIN_PHRASES = sizeof(BUILTIN_PHRASE_NAMES) / sizeof(BUILTIN_PHRASE_NAMES[0]);

} // namespace

VoicePage::VoicePage(IGfx& gfx, MiniAcid& mini_acid, AudioGuard audio_guard)
    : gfx_(gfx), mini_acid_(mini_acid), audio_guard_(audio_guard) {
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
      snprintf(buf, sizeof(buf), "%d: %s", phraseIndex_ + 1, BUILTIN_PHRASE_NAMES[phraseIndex_]);
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
  
  // Pitch row
  gfx.setTextColor(COLOR_WHITE);
  gfx.drawText(x, y, "PCH:");
  gfx.setTextColor(COLOR_KNOB_1);
  snprintf(buf, sizeof(buf), "%.0f Hz", synth.pitch());
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
  
  // Status line
  gfx.setTextColor(COLOR_GRAY);
  if (mini_acid_.vocalSynth().isSpeaking()) {
    gfx.setTextColor(kActiveColor);
    gfx.drawText(x, y, ">> Speaking...");
  } else if (mini_acid_.isVoiceTrackMuted()) {
    gfx.setTextColor(COLOR_RED);
    gfx.drawText(x, y, "MUTED");
  } else {
    gfx.drawText(x, y, "Space=Preview  M=Mute");
  }
}

void VoicePage::draw(IGfx& gfx) {
  int y = dy() + 2;
  
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

void VoicePage::triggerPreview() {
  withAudioGuard([&]() {
    if (useCustomPhrase_) {
      mini_acid_.speakCustomPhrase(phraseIndex_);
    } else {
      mini_acid_.speakPhrase(phraseIndex_);
    }
  });
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

  return Container::handleEvent(ui_event);
}

const std::string& VoicePage::getTitle() const { return title_; }
