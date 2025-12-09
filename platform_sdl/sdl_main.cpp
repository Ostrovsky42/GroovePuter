#include <SDL.h>
#include <stdio.h>
#include <string>
#include <cmath>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "sdl_display.h"
#include "../cardputer_display.h"
#include "../miniacid_display.h"
#include "../dsp_engine.h"
#include "scene_storage_sdl.h"

struct AudioContext {
  explicit AudioContext(float sampleRate) : storage(), synth(sampleRate, &storage), device(0) {}
  SceneStorageSdl storage;
  MiniAcid synth;
  SDL_AudioDeviceID device;
};

struct AppState {
  AppState() : audio(SAMPLE_RATE) {}
  AudioContext audio;
  IGfx* gfx = nullptr;
  SDLDisplay* sdl = nullptr;
  CardputerDisplay* card = nullptr;
  MiniAcidDisplay* ui = nullptr;
  bool running = true;
  bool cleaned_up = false;
  unsigned long lastUIUpdate = 0;
};

static void audioCallback(void *userdata, Uint8 *stream, int len) {
  AudioContext *ctx = static_cast<AudioContext *>(userdata);
  int16_t *out = reinterpret_cast<int16_t *>(stream);
  size_t frames = static_cast<size_t>(len) / sizeof(int16_t);

  // Fill the output buffer using the synth
  ctx->synth.generateAudioBuffer(out, frames);
}

static void handleEvents(AppState& s) {
  SDL_Event e;
  auto patternIndexFromScan = [](SDL_Scancode code) -> int {
    switch (code) {
      case SDL_SCANCODE_Q: return 0;
      case SDL_SCANCODE_W: return 1;
      case SDL_SCANCODE_E: return 2;
      case SDL_SCANCODE_R: return 3;
      case SDL_SCANCODE_T: return 4;
      case SDL_SCANCODE_Y: return 5;
      case SDL_SCANCODE_U: return 6;
      case SDL_SCANCODE_I: return 7;
      default: return -1;
    }
  };
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) {
      s.running = false;
    } else if (e.type == SDL_KEYDOWN) {
      if (s.ui) s.ui->dismissSplash();
      SDL_Scancode sc = e.key.keysym.scancode;
      int voiceIndex = (s.ui && s.ui->is303ControlPage()) ? s.ui->active303Voice() : 0;
      if (s.ui && s.ui->isPatternEditPage()) {
        int editVoice = s.ui->activePatternVoice();
        bool handled = false;
        if (sc == SDL_SCANCODE_LEFT) {
          s.ui->movePatternCursor(-1);
          handled = true;
        } else if (sc == SDL_SCANCODE_RIGHT) {
          s.ui->movePatternCursor(1);
          handled = true;
        } else if (sc == SDL_SCANCODE_UP) {
          s.ui->movePatternCursorVertical(-1);
          handled = true;
        } else if (sc == SDL_SCANCODE_DOWN) {
          s.ui->movePatternCursorVertical(1);
          handled = true;
        } else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
          if (s.ui->patternRowFocused(editVoice)) {
            int cursor = s.ui->activePatternCursor();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.set303PatternIndex(editVoice, cursor);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          }
        } else if (sc == SDL_SCANCODE_BACKSPACE) {
          if (!s.ui->patternRowFocused(editVoice)) {
            int step = s.ui->activePatternStep();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.clear303StepNote(editVoice, step);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          }
        }

        if (!handled) {
          auto ensureStepFocus = [&]() {
            if (s.ui->patternRowFocused(editVoice)) {
              s.ui->focusPatternSteps(editVoice);
            }
          };

          int patternIdx = patternIndexFromScan(sc);
          bool reservedPatternKey = (sc == SDL_SCANCODE_Q || sc == SDL_SCANCODE_W);
          if (patternIdx >= 0 && (!reservedPatternKey || s.ui->patternRowFocused(editVoice))) {
            s.ui->focusPatternRow(editVoice);
            s.ui->setPatternCursor(editVoice, patternIdx);
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.set303PatternIndex(editVoice, patternIdx);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          } else if (sc == SDL_SCANCODE_Q) {
            ensureStepFocus();
            int step = s.ui->activePatternStep();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.toggle303SlideStep(editVoice, step);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          } else if (sc == SDL_SCANCODE_W) {
            ensureStepFocus();
            int step = s.ui->activePatternStep();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.toggle303AccentStep(editVoice, step);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          } else if (sc == SDL_SCANCODE_A) {
            ensureStepFocus();
            int step = s.ui->activePatternStep();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.adjust303StepNote(editVoice, step, 1);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          } else if (sc == SDL_SCANCODE_Z) {
            ensureStepFocus();
            int step = s.ui->activePatternStep();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.adjust303StepNote(editVoice, step, -1);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          } else if (sc == SDL_SCANCODE_S) {
            ensureStepFocus();
            int step = s.ui->activePatternStep();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.adjust303StepOctave(editVoice, step, 1);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          } else if (sc == SDL_SCANCODE_X) {
            ensureStepFocus();
            int step = s.ui->activePatternStep();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.adjust303StepOctave(editVoice, step, -1);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          }
        }

        if (handled) {
          if (s.ui) s.ui->update();
          continue;
        }
      } else if (s.ui && s.ui->isDrumSequencerPage()) {
        bool handled = false;
        if (sc == SDL_SCANCODE_LEFT) {
          s.ui->moveDrumCursor(-1);
          handled = true;
        } else if (sc == SDL_SCANCODE_RIGHT) {
          s.ui->moveDrumCursor(1);
          handled = true;
        } else if (sc == SDL_SCANCODE_UP) {
          s.ui->moveDrumCursorVertical(-1);
          handled = true;
        } else if (sc == SDL_SCANCODE_DOWN) {
          s.ui->moveDrumCursorVertical(1);
          handled = true;
        } else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
          if (s.ui->drumPatternRowFocused()) {
            int cursor = s.ui->activeDrumPatternCursor();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.setDrumPatternIndex(cursor);
            SDL_UnlockAudioDevice(s.audio.device);
          } else {
            int step = s.ui->activeDrumStep();
            int voice = s.ui->activeDrumVoice();
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.toggleDrumStep(voice, step);
            SDL_UnlockAudioDevice(s.audio.device);
          }
          handled = true;
        }

        if (!handled) {
          int patternIdx = patternIndexFromScan(sc);
          if (patternIdx >= 0) {
            s.ui->focusDrumPatternRow();
            s.ui->setDrumPatternCursor(patternIdx);
            SDL_LockAudioDevice(s.audio.device);
            s.audio.synth.setDrumPatternIndex(patternIdx);
            SDL_UnlockAudioDevice(s.audio.device);
            handled = true;
          }
        }

        if (handled) {
          if (s.ui) s.ui->update();
          continue;
        }
      }
      if (sc == SDL_SCANCODE_ESCAPE) {
        // s.running = false;
      } else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
        if (s.ui) s.ui->dismissSplash();
        if (s.ui) s.ui->update();
      } else if (sc == SDL_SCANCODE_SPACE) {
        SDL_LockAudioDevice(s.audio.device);
        if (s.audio.synth.isPlaying()) {
          s.audio.synth.stop();
        } else {
          s.audio.synth.start();
        }
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_LEFTBRACKET) {
        if (s.ui) s.ui->previousPage();
        if (s.ui) s.ui->update();
      } else if (sc == SDL_SCANCODE_RIGHTBRACKET) {
        if (s.ui) s.ui->nextPage();
        if (s.ui) s.ui->update();
      } else if (sc == SDL_SCANCODE_I) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.randomize303Pattern(0);
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Randomized 303A pattern\n");
      } else if (sc == SDL_SCANCODE_O) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.randomize303Pattern(1);
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Randomized 303B pattern\n");
      } else if (sc == SDL_SCANCODE_P) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.randomizeDrumPattern();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Randomized drums\n");
      } else if (sc == SDL_SCANCODE_M) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleDelay303(voiceIndex);
        bool enabled = s.audio.synth.is303DelayEnabled(voiceIndex);
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303A delay %c %s\n", voiceIndex == 0 ? 'A' : 'B', enabled ? "on" : "off");
      } else if (sc == SDL_SCANCODE_A) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.adjust303Parameter(TB303ParamId::Cutoff, 1, voiceIndex);
        float cf = s.audio.synth.parameter303(TB303ParamId::Cutoff, voiceIndex).value();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303%c cutoff: %.1f Hz\n", voiceIndex == 0 ? 'A' : 'B', cf);
      } else if (sc == SDL_SCANCODE_Z) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.adjust303Parameter(TB303ParamId::Cutoff, -1, voiceIndex);
        float cf = s.audio.synth.parameter303(TB303ParamId::Cutoff, voiceIndex).value();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303%c cutoff: %.1f Hz\n", voiceIndex == 0 ? 'A' : 'B', cf);
      } else if (sc == SDL_SCANCODE_S) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.adjust303Parameter(TB303ParamId::Resonance, 1, voiceIndex);
        float res = s.audio.synth.parameter303(TB303ParamId::Resonance, voiceIndex).value();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303%c resonance: %.2f\n", voiceIndex == 0 ? 'A' : 'B', res);
      } else if (sc == SDL_SCANCODE_X) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.adjust303Parameter(TB303ParamId::Resonance, -1, voiceIndex);
        float res = s.audio.synth.parameter303(TB303ParamId::Resonance, voiceIndex).value();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303%c resonance: %.2f\n", voiceIndex == 0 ? 'A' : 'B', res);
      } else if (sc == SDL_SCANCODE_D) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.adjust303Parameter(TB303ParamId::EnvAmount, 1, voiceIndex);
        float amt = s.audio.synth.parameter303(TB303ParamId::EnvAmount, voiceIndex).value();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303%c env amount: %.0f Hz\n", voiceIndex == 0 ? 'A' : 'B', amt);
      } else if (sc == SDL_SCANCODE_C) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.adjust303Parameter(TB303ParamId::EnvAmount, -1, voiceIndex);
        float amt = s.audio.synth.parameter303(TB303ParamId::EnvAmount, voiceIndex).value();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303%c env amount: %.0f Hz\n", voiceIndex == 0 ? 'A' : 'B', amt);
      } else if (sc == SDL_SCANCODE_F) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.adjust303Parameter(TB303ParamId::EnvDecay, 1, voiceIndex);
        float dec = s.audio.synth.parameter303(TB303ParamId::EnvDecay, voiceIndex).value();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303%c decay: %.0f ms\n", voiceIndex == 0 ? 'A' : 'B', dec);
      } else if (sc == SDL_SCANCODE_V) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.adjust303Parameter(TB303ParamId::EnvDecay, -1, voiceIndex);
        float dec = s.audio.synth.parameter303(TB303ParamId::EnvDecay, voiceIndex).value();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303%c decay: %.0f ms\n", voiceIndex == 0 ? 'A' : 'B', dec);
      } else if (sc == SDL_SCANCODE_1) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMute303(0);
        bool muted = s.audio.synth.is303Muted(0);
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303A %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_2) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMute303(1);
        bool muted = s.audio.synth.is303Muted(1);
        SDL_UnlockAudioDevice(s.audio.device);
        printf("303B %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_3) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteKick();
        bool muted = s.audio.synth.isKickMuted();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Kick %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_4) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteSnare();
        bool muted = s.audio.synth.isSnareMuted();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Snare %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_5) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteHat();
        bool muted = s.audio.synth.isHatMuted();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Hat %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_6) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteOpenHat();
        bool muted = s.audio.synth.isOpenHatMuted();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Open hat %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_7) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteMidTom();
        bool muted = s.audio.synth.isMidTomMuted();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Mid tom %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_8) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteHighTom();
        bool muted = s.audio.synth.isHighTomMuted();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("High tom %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_9) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteRim();
        bool muted = s.audio.synth.isRimMuted();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Rim %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_0) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteClap();
        bool muted = s.audio.synth.isClapMuted();
        SDL_UnlockAudioDevice(s.audio.device);
        printf("Clap %s\n", muted ? "muted" : "unmuted");
      } else if (sc == SDL_SCANCODE_K) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.setBpm(s.audio.synth.bpm() - 5.0f);
        SDL_UnlockAudioDevice(s.audio.device);
        printf("BPM: %.1f\n", s.audio.synth.bpm());
      } else if (sc == SDL_SCANCODE_L) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.setBpm(s.audio.synth.bpm() + 5.0f);
        SDL_UnlockAudioDevice(s.audio.device);
        printf("BPM: %.1f\n", s.audio.synth.bpm());
      }
    }
  }
}

static void updateUI(AppState& s) {
  unsigned long now = SDL_GetTicks();
  if (now - s.lastUIUpdate > 80) {
    s.lastUIUpdate = now;
    if (s.ui) s.ui->update();
  }
}

static void cleanup(AppState& s) {
  if (s.cleaned_up) return;
  SDL_CloseAudioDevice(s.audio.device);
  SDL_Quit();
  s.cleaned_up = true;
}

static void mainLoopTick(void* userdata) {
  AppState* s = static_cast<AppState*>(userdata);
  handleEvents(*s);
  updateUI(*s);
  if (!s->running) {
#ifdef __EMSCRIPTEN__
    emscripten_cancel_main_loop();
#endif
    cleanup(*s);
  }
}

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_VIDEO) != 0) {
    fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return 1;
  }

  AppState state;

  int winw = 240;
  int winh = 135;

  if (argc > 1 && std::string(argv[1]) == "card") {
    state.card = new CardputerDisplay();
    state.gfx = state.card;
  } else {
    state.sdl = new SDLDisplay(winw, winh, "MiniAcid");
    state.gfx = state.sdl;
  }

  if (!state.gfx) {
    fprintf(stderr, "Failed to create gfx backend\n");
    SDL_Quit();
    return 1;
  }

  state.gfx->begin();
  state.audio.synth.init();

  SDL_AudioSpec desired{};
  desired.freq = SAMPLE_RATE;
  desired.format = AUDIO_S16SYS;
  desired.channels = 1;
  desired.samples = AUDIO_BUFFER_SAMPLES;
  desired.callback = audioCallback;
  desired.userdata = &state.audio;

  SDL_AudioSpec obtained{};
  state.audio.device = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
  if (state.audio.device == 0) {
    fprintf(stderr, "Failed to open audio: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  printf("M I N I A C I D\n");
  printf("Sample rate: %d Hz, buffer: %d samples\n", obtained.freq,
         obtained.samples);
  SDL_PauseAudioDevice(state.audio.device, 0); // start playback

  state.ui = new MiniAcidDisplay(*state.gfx, state.audio.synth);

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(mainLoopTick, &state, 0, 1);
#else
  while (state.running) {
    mainLoopTick(&state);
    if (!state.running) break;
    SDL_Delay(10);
  }
  cleanup(state);
#endif

  return 0;
}
