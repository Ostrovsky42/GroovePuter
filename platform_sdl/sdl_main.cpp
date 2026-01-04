#include <cmath>
#include <functional>
#include <stdio.h>
#include <string>

#include <SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "sdl_display.h"
#include "../cardputer_display.h"
#include "../src/ui/miniacid_display.h"
#include "../src/dsp/miniacid_engine.h"
#include "scene_storage_sdl.h"
#ifndef __EMSCRIPTEN__
#include "wav_recorder.h"
#endif

struct AudioContext {
  explicit AudioContext(float sampleRate) : storage(), synth(sampleRate, &storage), device(0) {}
  SceneStorageSdl storage;
  MiniAcid synth;
  SDL_AudioDeviceID device;
#ifndef __EMSCRIPTEN__
  WavRecorder recorder;
#endif
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
#ifndef __EMSCRIPTEN__
  ctx->recorder.writeSamples(out, frames);
#endif
}

static void handleEvents(AppState& s) {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) {
      s.running = false;
    } else if (e.type == SDL_KEYDOWN) {
      if (s.ui) s.ui->dismissSplash();
      SDL_Scancode sc = e.key.keysym.scancode;
#ifndef __EMSCRIPTEN__
      if (sc == SDL_SCANCODE_R && (e.key.keysym.mod & KMOD_CTRL) != 0 && s.sdl && e.key.repeat == 0) {
        SDL_LockAudioDevice(s.audio.device);
        if (s.audio.recorder.isRecording()) {
          s.audio.recorder.stop();
          printf("WAV Recording stopped: %s\n", s.audio.recorder.filename().c_str());
        } else if (s.audio.recorder.start(SAMPLE_RATE, 1)) {
          printf("WAV Recording started: %s\n", s.audio.recorder.filename().c_str());
        } else {
          fprintf(stderr, "Failed to start WAV recording\n");
        }
        SDL_UnlockAudioDevice(s.audio.device);
        continue;
      }
#endif
      UIEvent miniacidEvent{};
      miniacidEvent.event_type = MINIACID_KEY_DOWN;
      miniacidEvent.alt = (e.key.keysym.mod & KMOD_ALT) != 0;
      switch(sc) {
        case SDL_SCANCODE_DOWN:
          miniacidEvent.scancode = MINIACID_DOWN;
          break;
        case SDL_SCANCODE_UP:
          miniacidEvent.scancode = MINIACID_UP;
          break;
        case SDL_SCANCODE_LEFT:
          miniacidEvent.scancode = MINIACID_LEFT;
          break;
        case SDL_SCANCODE_RIGHT:
          miniacidEvent.scancode = MINIACID_RIGHT;
          break;
        case SDL_SCANCODE_ESCAPE:
          miniacidEvent.scancode = MINIACID_ESCAPE;
          break;
        default:
          break;
      }
      SDL_Keycode keycode = e.key.keysym.sym;
      if (keycode == SDLK_RETURN || keycode == SDLK_KP_ENTER) {
        miniacidEvent.key = '\n';
      } else if (keycode == SDLK_TAB) {
        miniacidEvent.key = '\t';
      } else if (keycode == SDLK_BACKSPACE) {
        miniacidEvent.key = '\b';
      } else if (keycode >= 32 && keycode < 127) {
        miniacidEvent.key = static_cast<char>(keycode);
      }

      bool handledByUI = s.ui ? s.ui->handleEvent(miniacidEvent) : false;
      if (handledByUI) continue;

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
      } else if (sc == SDL_SCANCODE_O) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.randomize303Pattern(1);
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_P) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.randomizeDrumPattern();
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_1) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMute303(0);
        bool muted = s.audio.synth.is303Muted(0);
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_2) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMute303(1);
        bool muted = s.audio.synth.is303Muted(1);
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_3) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteKick();
        bool muted = s.audio.synth.isKickMuted();
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_4) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteSnare();
        bool muted = s.audio.synth.isSnareMuted();
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_5) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteHat();
        bool muted = s.audio.synth.isHatMuted();
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_6) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteOpenHat();
        bool muted = s.audio.synth.isOpenHatMuted();
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_7) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteMidTom();
        bool muted = s.audio.synth.isMidTomMuted();
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_8) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteHighTom();
        bool muted = s.audio.synth.isHighTomMuted();
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_9) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteRim();
        bool muted = s.audio.synth.isRimMuted();
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_0) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.toggleMuteClap();
        bool muted = s.audio.synth.isClapMuted();
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_K) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.setBpm(s.audio.synth.bpm() - 5.0f);
        SDL_UnlockAudioDevice(s.audio.device);
      } else if (sc == SDL_SCANCODE_L) {
        SDL_LockAudioDevice(s.audio.device);
        s.audio.synth.setBpm(s.audio.synth.bpm() + 5.0f);
        SDL_UnlockAudioDevice(s.audio.device);
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
#ifndef __EMSCRIPTEN__
  if (s.audio.recorder.isRecording()) {
    SDL_LockAudioDevice(s.audio.device);
    s.audio.recorder.stop();
    SDL_UnlockAudioDevice(s.audio.device);
    printf("WAV Recording stopped: %s\n", s.audio.recorder.filename().c_str());
  }
#endif
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

  SDL_PauseAudioDevice(state.audio.device, 0); // start playback

  state.ui = new MiniAcidDisplay(*state.gfx, state.audio.synth);
  state.ui->setAudioGuard([&](const std::function<void()>& fn) {
    SDL_LockAudioDevice(state.audio.device);
    fn();
    SDL_UnlockAudioDevice(state.audio.device);
  });

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
