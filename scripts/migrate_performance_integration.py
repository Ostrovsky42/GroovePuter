#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(relative: str, old: str, new: str) -> None:
    path = ROOT / relative
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one guarded block in {relative}, found {count}: {old[:80]!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


# MiniAcid public live-note adapter and lifecycle epoch.
replace_once(
    "src/dsp/miniacid_engine.h",
    """  void start();
  void stop();
  void setBpm(float bpm);
""",
    """  void start();
  void stop();

  // Live performance input is deliberately separate from pattern storage.
  // PatternPlayer owns both synth voices while transport is running.
  void liveNoteOn(int synthIndex, uint8_t midiNote, uint8_t velocity);
  void liveNoteOff(int synthIndex, uint8_t midiNote);
  void allLiveNotesOff();
  int liveNote(int synthIndex) const;
  uint32_t liveInputEpoch() const { return liveInputEpoch_; }

  void setBpm(float bpm);
""",
)
replace_once(
    "src/dsp/miniacid_engine.h",
    """  long gateCountdownA_ = 0;
  long gateCountdownB_ = 0;
  bool songMode_;
""",
    """  long gateCountdownA_ = 0;
  long gateCountdownB_ = 0;
  int16_t liveNotes_[NUM_303_VOICES] = {-1, -1};
  uint32_t liveInputEpoch_ = 0;
  bool songMode_;
""",
)

replace_once(
    "src/dsp/miniacid_engine.cpp",
    """  if (synthVoices_[0]) synthVoices_[0]->reset();
  if (synthVoices_[1]) synthVoices_[1]->reset();
  LOG_PRINTLN("    - MiniAcid::reset: voices reset");
""",
    """  if (synthVoices_[0]) synthVoices_[0]->reset();
  if (synthVoices_[1]) synthVoices_[1]->reset();
  liveNotes_[0] = -1;
  liveNotes_[1] = -1;
  ++liveInputEpoch_;
  LOG_PRINTLN("    - MiniAcid::reset: voices reset");
""",
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    """void MiniAcid::start() {
  LOG_PRINTLN("[DSP] START command received");
  playing = true;
""",
    """void MiniAcid::start() {
  LOG_PRINTLN("[DSP] START command received");
  // PatternPlayer takes exclusive ownership of the monophonic voices.
  allLiveNotesOff();
  playing = true;
""",
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    """  if (synthVoices_[0]) synthVoices_[0]->release();
  if (synthVoices_[1]) synthVoices_[1]->release();
  drums->reset();
""",
    """  if (synthVoices_[0]) synthVoices_[0]->release();
  if (synthVoices_[1]) synthVoices_[1]->release();
  liveNotes_[0] = -1;
  liveNotes_[1] = -1;
  drums->reset();
""",
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    """void MiniAcid::setBpm(float bpm) {
""",
    """void MiniAcid::liveNoteOn(int synthIndex, uint8_t midiNote, uint8_t velocity) {
  if (playing) return;
  const int idx = clamp303Voice(synthIndex);
  const int note = clamp303Note(static_cast<int>(midiNote));
  if (!synthVoices_[idx]) return;
  if (velocity < 1) velocity = 1;
  if (velocity > 127) velocity = 127;

  synthVoices_[idx]->startNote(noteToFreq(note), false, false, velocity);
  liveNotes_[idx] = static_cast<int16_t>(note);
  if (idx == 0) gateCountdownA_ = 0;
  else gateCountdownB_ = 0;
}

void MiniAcid::liveNoteOff(int synthIndex, uint8_t midiNote) {
  const int idx = clamp303Voice(synthIndex);
  if (liveNotes_[idx] != static_cast<int16_t>(midiNote)) return;
  if (synthVoices_[idx]) synthVoices_[idx]->release();
  liveNotes_[idx] = -1;
}

void MiniAcid::allLiveNotesOff() {
  for (int idx = 0; idx < NUM_303_VOICES; ++idx) {
    if (synthVoices_[idx]) synthVoices_[idx]->release();
    liveNotes_[idx] = -1;
  }
  gateCountdownA_ = 0;
  gateCountdownB_ = 0;
}

int MiniAcid::liveNote(int synthIndex) const {
  return liveNotes_[clamp303Voice(synthIndex)];
}

void MiniAcid::setBpm(float bpm) {
""",
)
replace_once(
    "src/dsp/miniacid_engine.cpp",
    """  if (synthEngineNames_[idx] == targetName) {
    return;
  }

  if (!playing) {
""",
    """  if (synthEngineNames_[idx] == targetName) {
    return;
  }

  if (synthVoices_[idx]) synthVoices_[idx]->release();
  liveNotes_[idx] = -1;
  ++liveInputEpoch_;

  if (!playing) {
""",
)

# Add the Perform page while preserving every existing page index.
replace_once(
    "src/ui/ui_config.h",
    "static constexpr int kPageCount = 12;",
    "static constexpr int kPageCount = 13;",
)
replace_once(
    "src/ui/miniacid_display.h",
    "class IAudioRecorder;\n",
    "class IAudioRecorder;\nclass PerformanceKeyboard;\n",
)
replace_once(
    "src/ui/miniacid_display.h",
    "MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid);",
    "MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid, PerformanceKeyboard& performance_keyboard);",
)
replace_once(
    "src/ui/miniacid_display.h",
    """  void dismissSplash();
  bool handleEvent(UIEvent event);
""",
    """  void dismissSplash();
  bool handleEvent(UIEvent event);
  int currentPageIndex() const { return page_index_; }
""",
)
replace_once(
    "src/ui/miniacid_display.h",
    """  IGfx& gfx_;
  MiniAcid& mini_acid_;
  int page_index_ = 0;
  int previous_page_index_ = 0;  // For Backspace/` toggle
""",
    """  IGfx& gfx_;
  MiniAcid& mini_acid_;
  PerformanceKeyboard& performance_keyboard_;
  int page_index_ = 12;
  int previous_page_index_ = 12;  // For Backspace/` toggle
""",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    '#include "pages/sampler_page.h"\n',
    '#include "pages/sampler_page.h"\n#include "pages/perform_page.h"\n#include "workflow_mode.h"\n',
)
replace_once(
    "src/ui/miniacid_display.cpp",
    """MiniAcidDisplay::MiniAcidDisplay(IGfx& gfx, MiniAcid& mini_acid)
    : gfx_(gfx), mini_acid_(mini_acid) {
""",
    """MiniAcidDisplay::MiniAcidDisplay(IGfx& gfx,
                                 MiniAcid& mini_acid,
                                 PerformanceKeyboard& performance_keyboard)
    : gfx_(gfx),
      mini_acid_(mini_acid),
      performance_keyboard_(performance_keyboard) {
""",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    """    // Only create the first page (PlayPage) to start with
    pages_[0] = createPage_(0);
""",
    """    // PERFORM is the startup context; legacy page indices remain stable.
    pages_[page_index_] = createPage_(page_index_);
    mini_acid_.setCurrentPage(static_cast<int8_t>(page_index_));
""",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    """        case 11: page = std::make_unique<ModePage>(gfx_, mini_acid_, audio_guard_); break;
    }
""",
    """        case 11: page = std::make_unique<ModePage>(gfx_, mini_acid_, audio_guard_); break;
        case 12: page = std::make_unique<PerformPage>(gfx_, mini_acid_, performance_keyboard_); break;
    }
""",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    """    previous_page_index_ = page_index_;
    page_index_ = index;

    IPage* newPage = getPage_(index);
""",
    """    previous_page_index_ = page_index_;
    page_index_ = index;
    mini_acid_.setCurrentPage(static_cast<int8_t>(page_index_));

    IPage* newPage = getPage_(index);
""",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    """    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        if (event.alt && (event.key == 'h' || event.key == 'H')) {
""",
    """    if (event.event_type == GROOVEPUTER_KEY_DOWN) {
        // Fn+Tab cycles the three product-level workflow contexts without
        // changing the existing detailed page carousel.
        if (event.meta && (event.key == '\\t' || event.scancode == GROOVEPUTER_TAB)) {
            const WorkflowMode current = WorkflowPages::modeForPage(page_index_);
            const int direction = event.shift ? -1 : 1;
            goToPage(WorkflowPages::pageForMode(
                WorkflowPages::nextMode(current, direction)));
            return true;
        }

        if (event.alt && (event.key == 'h' || event.key == 'H')) {
""",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    """        if (event.key == ' ') {
            withAudioGuard([&]() {
                if (mini_acid_.isPlaying()) mini_acid_.stop();
                else mini_acid_.start();
            });
            showToast(mini_acid_.isPlaying() ? "Play" : "Stop", 500);
            return true;
        }
""",
    """        if (event.key == ' ') {
            if (!mini_acid_.isPlaying()) performance_keyboard_.setTransportPlaying(true);
            withAudioGuard([&]() {
                if (mini_acid_.isPlaying()) mini_acid_.stop();
                else mini_acid_.start();
            });
            performance_keyboard_.setTransportPlaying(mini_acid_.isPlaying());
            showToast(mini_acid_.isPlaying() ? "Play" : "Stop", 500);
            return true;
        }
""",
)
replace_once(
    "src/ui/miniacid_display.cpp",
    """        if (event.ctrl && event.alt && (event.key == '\\b' || event.key == 0x7F)) {
            withAudioGuard([&]() {
""",
    """        if (event.ctrl && event.alt && (event.key == '\\b' || event.key == 0x7F)) {
            performance_keyboard_.panic();
            withAudioGuard([&]() {
""",
)

# Hardware wiring: centralized page-command-first input routing and matrix diff.
replace_once(
    "GroovePuter.ino",
    '#include "src/ui/key_normalize.h"\n',
    '#include "src/ui/key_normalize.h"\n#include "src/input/performance_keyboard.h"\n#include "src/input/internal_synth_output.h"\n#include "src/ui/workflow_mode.h"\n',
)
replace_once(
    "GroovePuter.ino",
    """static MiniAcid g_miniAcidInstance(kSampleRate, &g_sceneStorage);
MiniAcid* volatile g_miniAcid = nullptr;
Encoder8Miniacid* g_encoder8 = nullptr;
""",
    """static MiniAcid g_miniAcidInstance(kSampleRate, &g_sceneStorage);
static MusicalEventRouter g_musicalEventRouter;
static PerformanceKeyboard g_performanceKeyboard(g_musicalEventRouter);
static InternalSynthOutput g_internalSynthOutput(g_miniAcidInstance, g_audioMutationGate);
static uint32_t g_lastLiveInputEpoch = 0;
MiniAcid* volatile g_miniAcid = nullptr;
Encoder8Miniacid* g_encoder8 = nullptr;
""",
)
replace_once(
    "GroovePuter.ino",
    """  g_miniAcid->init();
  markBootStage(51, "after MiniAcid::init");
""",
    """  g_miniAcid->init();
  g_musicalEventRouter.addSink(g_internalSynthOutput);
  g_lastLiveInputEpoch = g_miniAcid->liveInputEpoch();
  markBootStage(51, "after MiniAcid::init");
""",
)
replace_once(
    "GroovePuter.ino",
    "g_miniDisplay = new (std::nothrow) MiniAcidDisplay(g_display, *g_miniAcid);",
    "g_miniDisplay = new (std::nothrow) MiniAcidDisplay(\n      g_display, *g_miniAcid, g_performanceKeyboard);",
)
replace_once(
    "GroovePuter.ino",
    """void loop() {
  M5Cardputer.update();
  LedManager::instance().update();
""",
    """void loop() {
  M5Cardputer.update();
  LedManager::instance().update();

  if (g_miniAcid && g_miniDisplay) {
    g_performanceKeyboard.setEnabled(
        WorkflowPages::allowsPerformanceKeyboard(g_miniDisplay->currentPageIndex()));
    g_performanceKeyboard.setTransportPlaying(g_miniAcid->isPlaying());
    const uint32_t epoch = g_miniAcid->liveInputEpoch();
    if (epoch != g_lastLiveInputEpoch) {
      g_performanceKeyboard.panic();
      g_lastLiveInputEpoch = epoch;
    }
  }
""",
)
replace_once(
    "GroovePuter.ino",
    """      if (g_miniAcid->isPlaying()) {
        g_miniAcid->stop();
      } else {
        g_miniAcid->start();
      }
    }
    drawUI();
""",
    """      if (g_miniAcid->isPlaying()) {
        g_miniAcid->stop();
      } else {
        g_performanceKeyboard.setTransportPlaying(true);
        g_miniAcid->start();
      }
    }
    g_performanceKeyboard.setTransportPlaying(g_miniAcid->isPlaying());
    drawUI();
""",
)
replace_once(
    "GroovePuter.ino",
    """    if (handled) {
      drawUI();
      return;
    }

    bool needsDraw = false;
""",
    """    g_performanceKeyboard.setTransportPlaying(g_miniAcid->isPlaying());
    if (handled) {
      drawUI();
      return;
    }

    // Page commands have priority. Only unhandled plain keys become notes.
    if (!evt.alt && !evt.ctrl && !evt.shift && !evt.meta &&
        g_miniDisplay &&
        WorkflowPages::allowsPerformanceKeyboard(g_miniDisplay->currentPageIndex()) &&
        g_performanceKeyboard.keyDown(evt.key)) {
      drawUI();
      return;
    }

    bool needsDraw = false;
""",
)
replace_once(
    "GroovePuter.ino",
    """  auto processKeys = [&](const Keyboard_Class::KeysState& ks) {
""",
    """  auto reconcilePerformanceKeys = [&](const Keyboard_Class::KeysState& ks) {
    char pressed[PerformanceKeyboard::kMaxHeldNotes]{};
    size_t count = 0;
    if (!ks.alt && !ks.ctrl && !ks.shift && !ks.fn) {
      for (auto hid : ks.hid_keys) {
        if (hid < 0x04 || hid > 0x1D) continue;
        const char key = static_cast<char>('a' + (hid - 0x04));
        uint8_t degree = 0;
        if (PerformanceKeyboard::scaleDegreeForKey(key, degree) &&
            count < PerformanceKeyboard::kMaxHeldNotes) {
          pressed[count++] = key;
        }
      }
    }
    g_performanceKeyboard.releaseMissingKeys(pressed, count);
  };

  auto processKeys = [&](const Keyboard_Class::KeysState& ks) {
""",
)
replace_once(
    "GroovePuter.ino",
    """    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    processKeys(ks);
""",
    """    Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();
    reconcilePerformanceKeys(ks);
    processKeys(ks);
""",
)
replace_once(
    "GroovePuter.ino",
    """  } else if (!keyPressed) {
    if (hasLastKeys) {
""",
    """  } else if (!keyPressed) {
    g_performanceKeyboard.releaseMissingKeys(nullptr, 0);
    if (hasLastKeys) {
""",
)

# SDL build keeps the new page constructible without adding a second audio path.
replace_once(
    "platform_sdl/sdl_main.cpp",
    '#include "../src/sampler/ram_sample_store.h"\n',
    '#include "../src/sampler/ram_sample_store.h"\n#include "../src/input/performance_keyboard.h"\n',
)
replace_once(
    "platform_sdl/sdl_main.cpp",
    """struct AppState {
  AppState() : audio(kSampleRate) {}
  AudioContext audio;
""",
    """struct AppState {
  AppState() : audio(kSampleRate), keyboard(router) {}
  AudioContext audio;
  MusicalEventRouter router;
  PerformanceKeyboard keyboard;
""",
)
replace_once(
    "platform_sdl/sdl_main.cpp",
    "state.ui = new MiniAcidDisplay(*state.gfx, state.audio.synth);",
    "state.ui = new MiniAcidDisplay(*state.gfx, state.audio.synth, state.keyboard);",
)
replace_once(
    "platform_sdl/Makefile",
    """\t../src/dsp/advanced_pattern_generator.cpp \\
\t../src/ui/miniacid_display.cpp \\
""",
    """\t../src/dsp/advanced_pattern_generator.cpp \\
\t../src/input/performance_keyboard.cpp \\
\t../src/input/internal_synth_output.cpp \\
\t../src/ui/miniacid_display.cpp \\
""",
)
replace_once(
    "platform_sdl/Makefile",
    """\t../src/ui/pages/genre_page.cpp \\
\t../src/ui/pages/feel_texture_page.cpp \\
""",
    """\t../src/ui/pages/genre_page.cpp \\
\t../src/ui/pages/perform_page.cpp \\
\t../src/ui/pages/feel_texture_page.cpp \\
""",
)

# Permanent source-level boundaries for this PR.
source_test = ROOT / "tests/test_source_regressions.py"
text = source_test.read_text(encoding="utf-8")
marker = "\ndef test_ui_redraw_does_not_hold_audio_pause() -> None:\n"
if marker not in text:
    raise RuntimeError("source regression insertion marker missing")
insert = r'''

def test_performance_workflow_boundaries() -> None:
    keyboard = (ROOT / "src/input/performance_keyboard.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/input/musical_event.h").read_text(encoding="utf-8")
    engine = (ROOT / "src/dsp/miniacid_engine.cpp").read_text(encoding="utf-8")
    sketch = (ROOT / "GroovePuter.ino").read_text(encoding="utf-8")
    display = (ROOT / "src/ui/miniacid_display.cpp").read_text(encoding="utf-8")

    require('constexpr char kLowerRow[] = "asdfghjkl";' in keyboard,
            "performance key mapping must stay centralized")
    require('constexpr char kUpperRow[] = "qwertyuiop";' in keyboard,
            "performance key mapping must stay centralized")
    for path in (ROOT / "src/ui/pages").glob("*.cpp"):
        page = path.read_text(encoding="utf-8")
        require("asdfghjkl" not in page and "qwertyuiop" not in page,
                f"keyboard mapping duplicated in {path.name}")

    require("MidiOutput" not in header,
            "MusicalEventTarget must describe logical voices, not output sinks")
    require("USBMIDI" not in sketch and "USB-MIDI" not in sketch,
            "USB MIDI belongs to the later spike")

    live_start = engine.index("void MiniAcid::liveNoteOn")
    live_end = engine.index("void MiniAcid::liveNoteOff", live_start)
    require("if (playing) return;" in engine[live_start:live_end],
            "PatternPlayer must exclusively own Synth A while transport runs")
    require("g_performanceKeyboard.keyDown" in sketch,
            "hardware input must route unhandled keys through PerformanceKeyboard")
    require("releaseMissingKeys" in sketch,
            "keyboard matrix must recover missed key-up events")
    require("case 12: page = std::make_unique<PerformPage>" in display,
            "PERFORM page must remain an additive page instead of reindexing editors")


def test_performance_settings_are_runtime_only() -> None:
    scenes = (ROOT / "scenes.h").read_text(encoding="utf-8")
    storage = (ROOT / "scenes.cpp").read_text(encoding="utf-8")
    require("PerformanceScale" not in scenes and "octaveShift" not in scenes,
            "performance scale/octave must not expand scene schema in this PR")
    require("PerformanceScale" not in storage and "octaveShift" not in storage,
            "performance settings must reset to runtime defaults after reboot")
'''
text = text.replace(marker, insert + marker, 1)
text = text.replace(
    """    test_enter_applies_selected_recipe()
    test_ui_redraw_does_not_hold_audio_pause()
""",
    """    test_enter_applies_selected_recipe()
    test_performance_workflow_boundaries()
    test_performance_settings_are_runtime_only()
    test_ui_redraw_does_not_hold_audio_pause()
""",
    1,
)
source_test.write_text(text, encoding="utf-8")

print("Performance workflow integration migration complete")
