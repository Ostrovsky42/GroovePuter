#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include "dsp_engine.h"
#include "cardputer_display.h"
#include <cstdarg>
#include <cstdio>
#include "miniacid_display.h"
#include "scene_storage_cardputer.h"

static constexpr IGfxColor CP_WHITE = IGfxColor::White();
static constexpr IGfxColor CP_BLACK = IGfxColor::Black();

CardputerDisplay g_display;
MiniAcidDisplay* g_miniDisplay = nullptr;
SceneStorageCardputer g_sceneStorage;

int16_t g_audioBuffer[AUDIO_BUFFER_SAMPLES];

TaskHandle_t g_audioTaskHandle = nullptr;

MiniAcid g_miniAcid(SAMPLE_RATE, &g_sceneStorage);

void audioTask(void *param) {
  while (true) {
    if (!g_miniAcid.isPlaying()) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    while (M5Cardputer.Speaker.isPlaying()) {
      vTaskDelay(1 / portTICK_PERIOD_MS);
    }

    g_miniAcid.generateAudioBuffer(g_audioBuffer, AUDIO_BUFFER_SAMPLES);

    M5Cardputer.Speaker.playRaw(g_audioBuffer, AUDIO_BUFFER_SAMPLES,
                                SAMPLE_RATE, false);
  }
}


void drawUI() {
  if (g_miniDisplay) g_miniDisplay->update();
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg);

  Serial.begin(115200);

  g_display.setRotation(1);
  g_display.begin();
  g_display.clear(CP_BLACK);

  M5Cardputer.Speaker.begin();
  M5Cardputer.Speaker.setVolume(200); // 0-255

  g_miniAcid.init();
  g_miniDisplay = new MiniAcidDisplay(g_display, g_miniAcid);

  xTaskCreatePinnedToCore(audioTask, "AudioTask",
                          4096, // stack
                          nullptr,
                          3, // priority
                          &g_audioTaskHandle,
                          1 // core
  );

  drawUI();
}

void loop() {
  M5Cardputer.update();

  if (M5Cardputer.BtnA.wasClicked()) {
    if (g_miniAcid.isPlaying()) {
      g_miniAcid.stop();
    } else {
      g_miniAcid.start();
    }
    drawUI();
  }

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    if (g_miniDisplay) g_miniDisplay->dismissSplash();
    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    auto patternIndexFromChar = [](char ch) -> int {
      switch (ch) {
        case 'q': case 'Q': return 0;
        case 'w': case 'W': return 1;
        case 'e': case 'E': return 2;
        case 'r': case 'R': return 3;
        case 't': case 'T': return 4;
        case 'y': case 'Y': return 5;
        case 'u': case 'U': return 6;
        case 'i': case 'I': return 7;
        default: return -1;
      }
    };
    auto handleDrumEnter = [&]() {
      if (g_miniDisplay->drumPatternRowFocused()) {
        int cursor = g_miniDisplay->activeDrumPatternCursor();
        g_miniDisplay->setDrumPatternCursor(cursor);
        g_miniAcid.setDrumPatternIndex(cursor);
      } else {
        int step = g_miniDisplay->activeDrumStep();
        int voice = g_miniDisplay->activeDrumVoice();
        g_miniAcid.toggleDrumStep(voice, step);
      }
      drawUI();
    };
    bool drumEnterPressed = false;
    bool drumEnterHandled = false;
    if (g_miniDisplay && g_miniDisplay->isPatternEditPage()) {
      int voice = g_miniDisplay->activePatternVoice();
      bool needsDraw = false;
      for (auto hid : ks.hid_keys) {
        if (hid == 0x36) { // ,
          g_miniDisplay->movePatternCursor(-1);
          needsDraw = true;
        } else if (hid == 0x38) { // /
          g_miniDisplay->movePatternCursor(1);
          needsDraw = true;
        } else if (hid == 0x33) { // ;
          g_miniDisplay->movePatternCursorVertical(-1);
          needsDraw = true;
        } else if (hid == 0x37) { // .
          g_miniDisplay->movePatternCursorVertical(1);
          needsDraw = true;
        } else if (hid == KEY_BACKSPACE) {
          if (!g_miniDisplay->patternRowFocused(voice)) {
            int step = g_miniDisplay->activePatternStep();
            g_miniAcid.clear303StepNote(voice, step);
            needsDraw = true;
          }
        }
      }
      if (needsDraw) drawUI();
    }
    else if (g_miniDisplay && g_miniDisplay->isDrumSequencerPage()) {
      bool needsDraw = false;
      for (auto hid : ks.hid_keys) {
        if (hid == 0x36) { // , - same as arrow left in the cardputer keypad
          g_miniDisplay->moveDrumCursor(-1); 
          needsDraw = true;
        } else if (hid == 0x38) { // / - same as arrow right in the cardputer keypad
          g_miniDisplay->moveDrumCursor(1);
          needsDraw = true;
        } else if (hid == 0x33) { // ; - same as arrow up in the cardputer keypad
          g_miniDisplay->moveDrumCursorVertical(-1);
          needsDraw = true;
        } else if (hid == 0x37) { // . - same as arrow down in the cardputer keypad
          g_miniDisplay->moveDrumCursorVertical(1);
          needsDraw = true;
        } else if (hid == 0x28 || hid == 0x58) { // enter / keypad enter
          drumEnterPressed = true;
        }
      }
      if (needsDraw) drawUI();
    }
    for (auto c : ks.word) {
      int voiceIndex = (g_miniDisplay && g_miniDisplay->is303ControlPage())
                        ? g_miniDisplay->active303Voice()
                        : 0;
      if (g_miniDisplay && g_miniDisplay->isPatternEditPage()) {
        int editVoice = g_miniDisplay->activePatternVoice();
        auto ensureStepFocus = [&]() {
          if (g_miniDisplay->patternRowFocused(editVoice)) {
            g_miniDisplay->focusPatternSteps(editVoice);
          }
        };

        if ((c == '\n' || c == '\r') && g_miniDisplay->patternRowFocused(editVoice)) {
          int cursor = g_miniDisplay->activePatternCursor();
          g_miniDisplay->setPatternCursor(editVoice, cursor);
          g_miniAcid.set303PatternIndex(editVoice, cursor);
          drawUI();
          continue;
        }

        int patternIdx = patternIndexFromChar(c);
        bool patternKeyIsReserved = (c == 'q' || c == 'Q' || c == 'w' || c == 'W');
        if (patternIdx >= 0 && (!patternKeyIsReserved || g_miniDisplay->patternRowFocused(editVoice))) {
          g_miniDisplay->focusPatternRow(editVoice);
          g_miniDisplay->setPatternCursor(editVoice, patternIdx);
          g_miniAcid.set303PatternIndex(editVoice, patternIdx);
          drawUI();
          continue;
        }

        if (c == 'q' || c == 'Q') {
          ensureStepFocus();
          int step = g_miniDisplay->activePatternStep();
          g_miniAcid.toggle303SlideStep(editVoice, step);
          drawUI();
          continue;
        } else if (c == 'w' || c == 'W') {
          ensureStepFocus();
          int step = g_miniDisplay->activePatternStep();
          g_miniAcid.toggle303AccentStep(editVoice, step);
          drawUI();
          continue;
        } else if (c == 'a' || c == 'A') {
          ensureStepFocus();
          int step = g_miniDisplay->activePatternStep();
          g_miniAcid.adjust303StepNote(editVoice, step, 1);
          drawUI();
          continue;
        } else if (c == 'z' || c == 'Z') {
          ensureStepFocus();
          int step = g_miniDisplay->activePatternStep();
          g_miniAcid.adjust303StepNote(editVoice, step, -1);
          drawUI();
          continue;
        } else if (c == 's' || c == 'S') {
          ensureStepFocus();
          int step = g_miniDisplay->activePatternStep();
          g_miniAcid.adjust303StepOctave(editVoice, step, 1);
          drawUI();
          continue;
        } else if (c == 'x' || c == 'X') {
          ensureStepFocus();
          int step = g_miniDisplay->activePatternStep();
          g_miniAcid.adjust303StepOctave(editVoice, step, -1);
          drawUI();
          continue;
        } else if (c == '\b' || c == KEY_BACKSPACE) {
          ensureStepFocus();
          int step = g_miniDisplay->activePatternStep();
          g_miniAcid.clear303StepNote(editVoice, step);
          drawUI();
          continue;
        }
      } else if (g_miniDisplay && g_miniDisplay->isDrumSequencerPage()) {
        if ((c == '\n' || c == '\r')) {
          handleDrumEnter();
          drumEnterHandled = true;
          continue;
        }

        int patternIdx = patternIndexFromChar(c);
        if (patternIdx >= 0) {
          g_miniDisplay->focusDrumPatternRow();
          g_miniDisplay->setDrumPatternCursor(patternIdx);
          g_miniAcid.setDrumPatternIndex(patternIdx);
          drawUI();
          continue;
        }
      }
      if (c == '\n' || c == '\r') {
        if (g_miniDisplay) g_miniDisplay->dismissSplash();
        drawUI();
      } else if (c == '[') {
        if (g_miniDisplay) g_miniDisplay->previousPage();
        drawUI();
      } else if (c == ']') {
        if (g_miniDisplay) g_miniDisplay->nextPage();
        drawUI();
      } else if (c == 'i' || c == 'I') {
        g_miniAcid.randomize303Pattern(0);
        drawUI();
      } else if (c == 'o' || c == 'O') {
        g_miniAcid.randomize303Pattern(1);
        drawUI();
      } else if (c == 'p' || c == 'P') {
        g_miniAcid.randomizeDrumPattern();
        drawUI();
      } else if (c == 'm' || c == 'M') {
        g_miniAcid.toggleDelay303(voiceIndex);
        drawUI();
      } else if (c == 'a' || c == 'A') {
        g_miniAcid.adjust303Parameter(TB303ParamId::Cutoff, 1, voiceIndex);
        drawUI();
      } else if (c == 'z' || c == 'Z') {
        g_miniAcid.adjust303Parameter(TB303ParamId::Cutoff, -1, voiceIndex);
        drawUI();
      } else if (c == 's' || c == 'S') {
        g_miniAcid.adjust303Parameter(TB303ParamId::Resonance, 1, voiceIndex);
        drawUI();
      } else if (c == 'x' || c == 'X') {
        g_miniAcid.adjust303Parameter(TB303ParamId::Resonance, -1, voiceIndex);
        drawUI();
      } else if (c == 'd' || c == 'D') {
        g_miniAcid.adjust303Parameter(TB303ParamId::EnvAmount, 1, voiceIndex);
        drawUI();
      } else if (c == 'c' || c == 'C') {
        g_miniAcid.adjust303Parameter(TB303ParamId::EnvAmount, -1, voiceIndex);
        drawUI();
      } else if (c == 'f' || c == 'F') {
        g_miniAcid.adjust303Parameter(TB303ParamId::EnvDecay, 1, voiceIndex);
        drawUI();
      } else if (c == 'v' || c == 'V') {
        g_miniAcid.adjust303Parameter(TB303ParamId::EnvDecay, -1, voiceIndex);
        drawUI();
      } else if (c == '1') {
        g_miniAcid.toggleMute303(0);
        drawUI();
      } else if (c == '2') {
        g_miniAcid.toggleMute303(1);
        drawUI();
      } else if (c == '3') {
        g_miniAcid.toggleMuteKick();
        drawUI();
      } else if (c == '4') {
        g_miniAcid.toggleMuteSnare();
        drawUI();
      } else if (c == '5') {
        g_miniAcid.toggleMuteHat();
        drawUI();
      } else if (c == '6') {
        g_miniAcid.toggleMuteOpenHat();
        drawUI();
      } else if (c == '7') {
        g_miniAcid.toggleMuteMidTom();
        drawUI();
      } else if (c == '8') {
        g_miniAcid.toggleMuteHighTom();
        drawUI();
      } else if (c == '9') {
        g_miniAcid.toggleMuteRim();
        drawUI();
      } else if (c == '0') {
        g_miniAcid.toggleMuteClap();
        drawUI();
      } else if (c == 'k' || c == 'K') {
        g_miniAcid.setBpm(g_miniAcid.bpm() - 5.0f);
        drawUI();
      } else if (c == 'l' || c == 'L') {
        g_miniAcid.setBpm(g_miniAcid.bpm() + 5.0f);
        drawUI();
      }
      else if (c == ' ')
      {
        if (g_miniAcid.isPlaying()){
          g_miniAcid.stop();
        }
        else {
          g_miniAcid.start();
        }
        drawUI();
      }
    }

    if (drumEnterPressed && !drumEnterHandled && g_miniDisplay && g_miniDisplay->isDrumSequencerPage()) {
      handleDrumEnter();
    }
  }

  static unsigned long lastUIUpdate = 0;
  if (millis() - lastUIUpdate > 80) {
    lastUIUpdate = millis();
    if (g_miniDisplay) g_miniDisplay->update();
  }

  delay(5);
}
