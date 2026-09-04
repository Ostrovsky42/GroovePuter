#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")


def write(path: str, value: str) -> None:
    (ROOT / path).write_text(value, encoding="utf-8")


def replace_once(path: str, old: str, new: str) -> None:
    value = text(path)
    count = value.count(old)
    if count != 1:
        raise RuntimeError(f"{path}: expected one exact match, got {count}: {old[:80]!r}")
    write(path, value.replace(old, new, 1))


def replace_regex(path: str, pattern: str, replacement: str) -> None:
    value = text(path)
    value2, count = re.subn(pattern, replacement, value, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f"{path}: regex match count={count}: {pattern[:100]!r}")
    write(path, value2)


# ---------------------------------------------------------------------------
# MiniAcid owns the single Pattern runtime playback state and immutable source.
# ---------------------------------------------------------------------------
H = "src/dsp/miniacid_engine.h"
replace_once(
    H,
    '#include "../phrase/runtime_pattern_event_bank.h"\n',
    '#include "../phrase/runtime_pattern_event_bank.h"\n#include "../phrase/runtime_synth_playback.h"\n',
)
replace_once(
    H,
    "  void processSequencerEvents(uint32_t absoluteTick);\n"
    "  void triggerSynthStep_(int synthIdx, int stepIdx);\n"
    "  void publishPatternNoteOn_(int synthIdx, uint8_t note, uint8_t velocity);\n",
    "  void processSequencerEvents(uint32_t absoluteTick);\n"
    "  void triggerSynthStep_(int synthIdx,\n"
    "                         const PhraseRuntime::RuntimeSynthEvent& event,\n"
    "                         uint32_t absoluteStartSubtick);\n"
    "  void consumePatternPlaybackActions_(\n"
    "      int synthIdx,\n"
    "      const PhraseRuntime::RuntimeSynthPlaybackActions& actions);\n"
    "  uint32_t currentAbsoluteSubtick_() const;\n"
    "  void publishPatternNoteOn_(int synthIdx, uint8_t note, uint8_t velocity);\n",
)
replace_once(
    H,
    "  long gateCountdownA_ = 0;\n"
    "  long gateCountdownB_ = 0;\n"
    "  ClampedLiveNoteIdentity liveNotes_[NUM_303_VOICES] = {-1, -1};\n"
    "  PhraseRuntime::RuntimePatternEventBank patternRuntimeBank_;\n",
    "  // P2 compatibility fields remain physically present for this cutover\n"
    "  // commit, but they no longer own Pattern backend lifetime decisions.\n"
    "  long gateCountdownA_ = 0;\n"
    "  long gateCountdownB_ = 0;\n"
    "  ClampedLiveNoteIdentity liveNotes_[NUM_303_VOICES] = {-1, -1};\n"
    "  PhraseRuntime::RuntimePatternEventBank patternRuntimeBank_;\n"
    "  PhraseRuntime::RuntimeSynthPlaybackState patternPlaybackState_[NUM_303_VOICES]{};\n"
    "  PhraseRuntime::RuntimeSynthEvent patternRetrigEvent_[NUM_303_VOICES]{};\n",
)

CPP = "src/dsp/miniacid_engine.cpp"
replace_once(
    CPP,
    "  LOG_PRINTLN(\"  - MiniAcid::init: applySceneStateFromManager()...\");\n"
    "  applySceneStateFromManager();\n"
    "  LOG_PRINTLN(\"  - MiniAcid::init: Done\");\n",
    "  LOG_PRINTLN(\"  - MiniAcid::init: applySceneStateFromManager()...\");\n"
    "  applySceneStateFromManager();\n"
    "  // P2 startup publication is a playback precondition. Playback is still\n"
    "  // stopped here: publish a complete resident bank and only then mirror\n"
    "  // the actual paging identity into MiniAcid runtime selection.\n"
    "  const int residentPage = PatternPagingService::activePageIndex();\n"
    "  if (rebuildPatternRuntimeEventBank() &&\n"
    "      patternRuntimeBank_.pageIdentity() == residentPage) {\n"
    "    setCurrentPage(static_cast<int8_t>(residentPage));\n"
    "  } else {\n"
    "    patternRuntimeBank_.invalidatePageIdentity();\n"
    "    LOG_PRINTLN(\"  - MiniAcid::init: Pattern runtime bank publication failed\");\n"
    "  }\n"
    "  LOG_PRINTLN(\"  - MiniAcid::init: Done\");\n",
)

# Named scene loads also leave no normal-path window where playback can select
# the new Scene with an old/invalid derived bank.
replace_once(
    CPP,
    "  Serial.println(\"[LoadScene] Applying scene state...\");\n"
    "  applySceneStateFromManager();\n"
    "  Serial.println(\"[LoadScene] SUCCESS\");\n"
    "  return true;\n",
    "  Serial.println(\"[LoadScene] Applying scene state...\");\n"
    "  applySceneStateFromManager();\n"
    "  const int residentPage = PatternPagingService::activePageIndex();\n"
    "  if (!rebuildPatternRuntimeEventBank() ||\n"
    "      patternRuntimeBank_.pageIdentity() != residentPage) {\n"
    "    patternRuntimeBank_.invalidatePageIdentity();\n"
    "    Serial.println(\"[LoadScene] runtime Pattern bank publication failed\");\n"
    "    return false;\n"
    "  }\n"
    "  setCurrentPage(static_cast<int8_t>(residentPage));\n"
    "  Serial.println(\"[LoadScene] SUCCESS\");\n"
    "  return true;\n",
)
replace_once(
    CPP,
    "  sceneManager_.wipeToZero();\n"
    "  applySceneStateFromManager();\n"
    "  if (saveSceneToStorage()) return true;\n",
    "  sceneManager_.wipeToZero();\n"
    "  applySceneStateFromManager();\n"
    "  const int residentPage = PatternPagingService::activePageIndex();\n"
    "  if (!rebuildPatternRuntimeEventBank() ||\n"
    "      patternRuntimeBank_.pageIdentity() != residentPage) {\n"
    "    patternRuntimeBank_.invalidatePageIdentity();\n"
    "    sceneStorage_->setCurrentSceneName(previousName);\n"
    "    return false;\n"
    "  }\n"
    "  setCurrentPage(static_cast<int8_t>(residentPage));\n"
    "  if (saveSceneToStorage()) return true;\n",
)

# Reset runtime execution state without making reset itself a second backend
# decision owner. Hard-barrier convergence remains the next P2 slice.
replace_once(
    CPP,
    "  // Reset Retrig States\n"
    "  retrigA_ = {};\n"
    "  retrigB_ = {};\n",
    "  // Reset Retrig States\n"
    "  retrigA_ = {};\n"
    "  retrigB_ = {};\n"
    "  patternPlaybackState_[0] = {};\n"
    "  patternPlaybackState_[1] = {};\n"
    "  patternRetrigEvent_[0] = {};\n"
    "  patternRetrigEvent_[1] = {};\n",
)

# Pending quantized generation retains the old audible immutable events in the
# existing owner. Resident bank selection is the fallback only after pending.
replace_regex(
    CPP,
    r"const PhraseRuntime::RuntimePatternEventBuffer&\nMiniAcid::activePatternRuntimeEvents\(int synthIndex\) const \{.*?\n\}\n\nvoid MiniAcid::updateDrumReverbDecay",
    '''const PhraseRuntime::RuntimePatternEventBuffer&
MiniAcid::activePatternRuntimeEvents(int synthIndex) const {
  if (synthIndex < 0 || synthIndex >= NUM_303_VOICES) {
    return patternRuntimeBank_.empty();
  }
  if (const PhraseRuntime::RuntimePatternEventBuffer* pending =
          GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleSynthRuntime(
              *this, synthIndex)) {
    return *pending;
  }
  const int bankIndex = current303BankIndex(synthIndex);
  const int patternIndex = current303PatternIndex(synthIndex);
  if (bankIndex < 0 || bankIndex >= kBankCount ||
      patternIndex < 0 || patternIndex >= Bank<SynthPattern>::kPatterns) {
    return patternRuntimeBank_.empty();
  }
  return patternRuntimeBank_.selectForPage(
      currentPageIndex(),
      static_cast<uint8_t>(synthIndex),
      static_cast<uint8_t>(bankIndex),
      static_cast<uint8_t>(patternIndex));
}

void MiniAcid::updateDrumReverbDecay''',
)

# Synth scheduling now consumes the immutable prepared event carrier. Drum
# scheduling remains on the frozen legacy path in this checkpoint.
replace_regex(
    CPP,
    r"void MiniAcid::processSequencerEvents\(uint32_t absoluteTick\) \{.*?\n\}\n\n\n\nvoid MiniAcid::generateAudioBuffer",
    '''void MiniAcid::processSequencerEvents(uint32_t absoluteTick) {
  uint32_t barTick = absoluteTick % 384;
  currentStepIndex = barTick / 24;

  if (barTick == 0) {
    // Keep the accepted BAR_START pending owner and Song ordering intact.
    if (genreManager_.commitPendingRecipe()) {
      regeneratePatternsWithGenre();
    }
    advanceSongBar_();
    LedManager::instance().onBeat(currentStepIndex, sceneManager_.currentScene().led);
  } else if (barTick % 24 == 0) {
    LedManager::instance().onBeat(currentStepIndex, sceneManager_.currentScene().led);
  }

  // P2 Synth A/B: prepared immutable events are authoritative for onset
  // placement. Invalid/wrong-page bank selection is canonical silence.
  const uint32_t absoluteStartSubtick =
      absoluteTick * static_cast<uint32_t>(PhraseRuntime::kSubticksPerTick);
  for (int synth = 0; synth < NUM_303_VOICES; ++synth) {
    const PhraseRuntime::RuntimePatternEventBuffer& events =
        activePatternRuntimeEvents(synth);
    for (uint8_t i = 0; i < events.count; ++i) {
      const PhraseRuntime::RuntimeSynthEvent& event = events.events[i];
      if (event.startTick != barTick) continue;
      triggerSynthStep_(synth, event, absoluteStartSubtick);
    }
  }

  // Drums retain the accepted legacy timing path in P2 executor cutover.
  int swingPct = GroovePuterRhythm::QuantizedGenerationDetail::audibleGenerationSwingPct(
      *this, sceneManager_.currentScene().feel.swingPct);
  if (swingPct < 50) swingPct = 50;
  if (swingPct > 75) swingPct = 75;
  int swingDelay = (int)std::round((swingPct - 50.0f) * 24.0f / 50.0f);
  uint16_t swingMask = sceneManager_.currentScene().feel.swingMask;

  int nominalStep = barTick / 24;
  for (int sIdx = nominalStep - 1; sIdx <= nominalStep + 1; ++sIdx) {
    int s = (sIdx + 16) % 16;
    uint32_t nominalT = s * 24;
    const DrumPatternSet* pendingDrums =
        GroovePuterRhythm::QuantizedGenerationDetail::pendingAudibleDrumPatternSet(*this);
    const DrumPatternSet& dSet = pendingDrums
        ? *pendingDrums
        : sceneManager_.getCurrentDrumPattern();
    for (int v = 0; v < 8; ++v) {
      VoiceId vId = (VoiceId)((int)VoiceId::DrumKick + v);
      int swingD = (s % 2 != 0 && (swingMask & (1 << (int)vId))) ? swingDelay : 0;
      int microD = dSet.voices[v].steps[s].timing;
      if ((nominalT + swingD + microD + 384) % 384 == barTick) {
        triggerDrumVoice_(v, s);
      }
    }
  }
}



void MiniAcid::generateAudioBuffer''',
)

# Legacy sample gate countdowns remain as dead compatibility fields. Natural
# expiry comes from the existing monotonic Q32.32 transport phase at 1/16 tick.
replace_once(
    CPP,
    "      if (gateCountdownA_ > 0 && --gateCountdownA_ <= 0) {\n"
    "        if (synthVoices_[0]) synthVoices_[0]->release();\n"
    "        publishPatternNoteOff_(0);\n"
    "      }\n"
    "      if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0) {\n"
    "        if (synthVoices_[1]) synthVoices_[1]->release();\n"
    "        publishPatternNoteOff_(1);\n"
    "      }\n",
    "      const uint32_t absoluteSubtick = currentAbsoluteSubtick_();\n"
    "      for (int synth = 0; synth < NUM_303_VOICES; ++synth) {\n"
    "        consumePatternPlaybackActions_(\n"
    "            synth, patternPlaybackState_[synth].releaseDue(absoluteSubtick));\n"
    "      }\n",
)

replace_regex(
    CPP,
    r"    // Retrig Logic \(omitted for brevity in this view\? No, I must keep it!\).*?\n    for \(int v = 0; v < NUM_DRUM_VOICES; \+\+v\) \{",
    '''    // Retrig Logic. Legacy sample counters keep only timing authority;
    // RuntimeSynthPlaybackState owns the logical Release/Start decision and
    // the common consumer fans it to both internal synth and Pattern MIDI.
    if (playing && retrigA_.active) {
      if (--retrigA_.counter <= 0 && retrigA_.countRemaining > 0) {
        consumePatternPlaybackActions_(
            0, patternPlaybackState_[0].acceptRetrigger(patternRetrigEvent_[0]));
        retrigA_.counter = retrigA_.interval;
        retrigA_.countRemaining--;
        if (retrigA_.countRemaining <= 0) retrigA_.active = false;
      }
    }
    if (playing && retrigB_.active) {
      if (--retrigB_.counter <= 0 && retrigB_.countRemaining > 0) {
        consumePatternPlaybackActions_(
            1, patternPlaybackState_[1].acceptRetrigger(patternRetrigEvent_[1]));
        retrigB_.counter = retrigB_.interval;
        retrigB_.countRemaining--;
        if (retrigB_.countRemaining <= 0) retrigB_.active = false;
      }
    }
    for (int v = 0; v < NUM_DRUM_VOICES; ++v) {''',
)

# Replace the legacy SynthStep/gate-countdown executor with one runtime action
# stream. RETRIG is a composite backend trace but remains one owner action.
replace_regex(
    CPP,
    r"void MiniAcid::triggerSynthStep_\(int synthIdx, int stepIdx\) \{.*?\n\}\n\nvoid MiniAcid::triggerDrumVoice_",
    '''void MiniAcid::consumePatternPlaybackActions_(
    int synthIdx,
    const PhraseRuntime::RuntimeSynthPlaybackActions& actions) {
  const int idx = clamp303Voice(synthIdx);
  for (uint8_t i = 0; i < actions.count; ++i) {
    const PhraseRuntime::RuntimeSynthPlaybackAction& action = actions.values[i];
    const PhraseRuntime::RuntimeSynthEvent& event = action.event;
    const bool accent = (event.flags & PhraseRuntime::kEventAccent) != 0;
    const bool slide = (event.flags & PhraseRuntime::kEventSlide) != 0;

    switch (action.type) {
      case PhraseRuntime::RuntimeSynthPlaybackActionType::Release:
        if (synthVoices_[idx]) synthVoices_[idx]->release();
        publishPatternNoteOff_(idx);
        break;

      case PhraseRuntime::RuntimeSynthPlaybackActionType::Start:
        if (synthVoices_[idx]) {
          synthVoices_[idx]->startNote(
              noteToFreq(event.note), accent, slide, event.velocity);
        }
        publishPatternNoteOn_(idx, event.note, event.velocity);
        LedManager::instance().onVoiceTriggered(
            idx == 0 ? VoiceId::SynthA : VoiceId::SynthB,
            sceneManager_.currentScene().led);
        break;

      case PhraseRuntime::RuntimeSynthPlaybackActionType::Retrigger:
        if (synthVoices_[idx]) synthVoices_[idx]->release();
        publishPatternNoteOff_(idx);
        if (synthVoices_[idx]) {
          synthVoices_[idx]->startNote(
              noteToFreq(event.note), accent, slide, event.velocity);
        }
        publishPatternNoteOn_(idx, event.note, event.velocity);
        LedManager::instance().onVoiceTriggered(
            idx == 0 ? VoiceId::SynthA : VoiceId::SynthB,
            sceneManager_.currentScene().led);
        break;

      default:
        break;
    }
  }
}

uint32_t MiniAcid::currentAbsoluteSubtick_() const {
  const uint32_t fractionalSubtick = static_cast<uint32_t>(
      (tickPhaseAccum_ & 0xFFFFFFFFULL) >> 28);
  return currentTick_ * static_cast<uint32_t>(PhraseRuntime::kSubticksPerTick) +
         fractionalSubtick;
}

void MiniAcid::triggerSynthStep_(
    int synthIdx,
    const PhraseRuntime::RuntimeSynthEvent& event,
    uint32_t absoluteStartSubtick) {
  const int songPattern = songPatternIndexForTrack(
      synthIdx == 0 ? SongTrack::SynthA : SongTrack::SynthB);
  if (songPattern < 0) return;
  if (synthIdx == 0 && mute303) return;
  if (synthIdx == 1 && mute303_2) return;

  // Preserve legacy RNG consumption exactly: ghost first, probability only
  // after ghost accepts. Projection is deterministic and never consumes RNG.
  const bool ghost = (event.flags & PhraseRuntime::kEventGhost) != 0;
  if (ghost && (rand() % 100 >= 80)) return;
  if (event.probability < 100 && (rand() % 100 >= event.probability)) return;

  consumePatternPlaybackActions_(
      synthIdx,
      patternPlaybackState_[synthIdx].acceptOnset(event, absoluteStartSubtick));

  RetrigState& retrig = synthIdx == 0 ? retrigA_ : retrigB_;
  retrig.active = false;
  patternRetrigEvent_[synthIdx] = event;
  if (event.fx == static_cast<uint8_t>(StepFx::Retrig) && event.fxParam > 0) {
    retrig.countRemaining = event.fxParam;
    retrig.interval = static_cast<int>(samplesPerStep_ / (event.fxParam + 1));
    if (retrig.interval < 1) retrig.interval = 1;
    retrig.counter = retrig.interval;
    retrig.active = true;
  }
}

void MiniAcid::triggerDrumVoice_''',
)

# ---------------------------------------------------------------------------
# Existing quantized generation pending owner gains old-audible compact events.
# ---------------------------------------------------------------------------
G = "src/generation/migration/quantized_generation_commit_impl.h"
replace_once(
    G,
    "  SynthPattern synth[2]{};\n"
    "  DrumPatternSet drums{};\n",
    "  SynthPattern synth[2]{};\n"
    "  PhraseRuntime::RuntimePatternEventBuffer synthRuntime[2]{};\n"
    "  DrumPatternSet drums{};\n",
)

U = "src/generation/migration/quantized_generation_undo_owner_impl.h"
replace_regex(
    U,
    r"inline void applyPreparedGenerationPersistent\(\n    MiniAcid& engine,\n    const PendingGeneration& pending\) \{.*?\n\}\n\ninline void activatePreparedGenerationRuntime",
    '''inline void applyPreparedGenerationPersistent(
    MiniAcid& engine,
    const PendingGeneration& pending) {
  SceneManager& scenes = engine.sceneManager();
  Scene& scene = scenes.currentScene();
  if (pending.scope == QuantizedGenerationScope::Full) {
    scene.synthABanks[pending.target.synthBank[0]]
        .patterns[pending.target.synthSlot[0]] = pending.synth[0];
    scene.synthBBanks[pending.target.synthBank[1]]
        .patterns[pending.target.synthSlot[1]] = pending.synth[1];
    scene.drumBanks[pending.target.drumBank]
        .patterns[pending.target.drumSlot] = pending.drums;
    scene.genre = pending.genre;
    scene.feel.swingPct = pending.swingPct;
    // Persistent truth is complete at COMMIT. Runtime mode/BPM stay on the
    // old audible truth until ACTIVATE when transport is playing.
    scenes.setMode(pending.mode);
    scenes.setBpm(pending.bpm);
    // Swing is projection input for every resident Synth Pattern, therefore a
    // FULL generation commit must settle the complete derived bank, not only
    // the two target slots. Old audible target events remain in PendingGeneration.
    (void)engine.rebuildPatternRuntimeEventBank();
    return;
  }

  const int voice = pending.scope == QuantizedGenerationScope::SynthA ? 0 : 1;
  Bank<SynthPattern>& bank = voice == 0
      ? scene.synthABanks[pending.target.synthBank[0]]
      : scene.synthBBanks[pending.target.synthBank[1]];
  bank.patterns[pending.target.synthSlot[voice]] = pending.synth[voice];
  (void)engine.refreshPatternRuntimeEvents(
      voice, pending.target.synthBank[voice], pending.target.synthSlot[voice]);
}

inline void activatePreparedGenerationRuntime''',
)
replace_once(
    U,
    "  activation.synth[0] = before.synth[0];\n"
    "  activation.synth[1] = before.synth[1];\n"
    "  activation.drums = before.drums;\n",
    "  activation.synth[0] = before.synth[0];\n"
    "  activation.synth[1] = before.synth[1];\n"
    "  // Capture the exact currently audible prepared events before persistent\n"
    "  // COMMIT changes resident Scene truth. This extends the existing owner.\n"
    "  activation.synthRuntime[0] = engine.activePatternRuntimeEvents(0);\n"
    "  activation.synthRuntime[1] = engine.activePatternRuntimeEvents(1);\n"
    "  activation.drums = before.drums;\n",
)
replace_once(
    U,
    "inline const DrumPatternSet* pendingAudibleDrumPatternSet(\n",
    "inline const PhraseRuntime::RuntimePatternEventBuffer*\n"
    "pendingAudibleSynthRuntime(const MiniAcid& engine, int voice) {\n"
    "  const PendingGeneration* pending = pendingAudibleActivation(engine);\n"
    "  if (pending == nullptr || voice < 0 || voice > 1) return nullptr;\n"
    "  const bool included = pending->scope == QuantizedGenerationScope::Full ||\n"
    "      (voice == 0 && pending->scope == QuantizedGenerationScope::SynthA) ||\n"
    "      (voice == 1 && pending->scope == QuantizedGenerationScope::SynthB);\n"
    "  if (!included ||\n"
    "      !targetStillActive(engine.sceneManager(), pending->target)) return nullptr;\n"
    "  return &pending->synthRuntime[voice];\n"
    "}\n\n"
    "inline const DrumPatternSet* pendingAudibleDrumPatternSet(\n",
)
replace_once(
    U,
    "  activation.synth[voice] = audibleBefore;\n"
    "  activation.genre = engine.sceneManager().currentScene().genre;\n",
    "  activation.synth[voice] = audibleBefore;\n"
    "  activation.synthRuntime[voice] = engine.activePatternRuntimeEvents(voice);\n"
    "  activation.genre = engine.sceneManager().currentScene().genre;\n",
)

# ---------------------------------------------------------------------------
# SDL full runtime build must link the newly authoritative playback owner.
# ---------------------------------------------------------------------------
M = "platform_sdl/Makefile"
make = text(M)
if "../src/phrase/runtime_synth_playback.cpp" not in make:
    anchor = "../src/phrase/runtime_synth_events.cpp"
    if anchor not in make:
        raise RuntimeError("platform_sdl/Makefile missing runtime_synth_events.cpp anchor")
    make = make.replace(anchor, anchor + " ../src/phrase/runtime_synth_playback.cpp", 1)
    write(M, make)

# ---------------------------------------------------------------------------
# P0 characterization observes the new owner while preserving musical facts.
# Direct private test edits explicitly settle derived runtime events because
# they intentionally bypass the production mutation owner.
# ---------------------------------------------------------------------------
P0S = "tests/test_pattern_phrase_p0_source_contract.py"
replace_once(
    P0S,
    '''require(
    "(nominalT + swingA + microA + 384) % 384 == barTick" in sequencer,
    "Synth A swing/microtiming modulo scheduling changed",
)
require(
    "(nominalT + swingB + microB + 384) % 384 == barTick" in sequencer,
    "Synth B swing/microtiming modulo scheduling changed",
)
''',
    '''require(
    "activePatternRuntimeEvents" in sequencer
    and "event.startTick" in sequencer,
    "P2 Synth scheduling is not driven by prepared runtime event startTick",
)
require(
    "activeSynthPattern(0).steps" not in sequencer
    and "activeSynthPattern(1).steps" not in sequencer,
    "mutable SynthPattern timing re-entered the P2 executor",
)
''',
)

P0 = "tests/test_pattern_phrase_p0_runtime.cpp"
replace_once(
    P0,
    "    engine.setBpm(120.0f);\n"
    "    engine.setPatternEventQueue(&queue);\n",
    "    engine.setBpm(120.0f);\n"
    "    assert(engine.rebuildPatternRuntimeEventBank());\n"
    "    engine.setCurrentPage(static_cast<int8_t>(PatternPagingService::activePageIndex()));\n"
    "    engine.setPatternEventQueue(&queue);\n",
)
replace_once(
    P0,
    "  long gateCountdown(int synth) const {\n"
    "    return synth == 0 ? engine.gateCountdownA_ : engine.gateCountdownB_;\n"
    "  }\n",
    "  bool runtimeActive(int synth) const {\n"
    "    return engine.patternPlaybackState_[synth].active();\n"
    "  }\n\n"
    "  uint32_t runtimeDeadline(int synth) const {\n"
    "    return engine.patternPlaybackState_[synth].releaseAtSubtick();\n"
    "  }\n",
)
replace_once(
    P0,
    "  for (std::size_t i = 0; i + 1 < notes.size(); i += 2) {\n"
    "    const int step = notes[i];\n"
    "    const int note = notes[i + 1];\n"
    "    const int8_t timing = timings.empty()\n"
    "        ? 0\n"
    "        : timings[static_cast<std::size_t>(step)];\n"
    "    setStep(pattern, step, note, 100, timing);\n"
    "  }\n"
    "}\n",
    "  for (std::size_t i = 0; i + 1 < notes.size(); i += 2) {\n"
    "    const int step = notes[i];\n"
    "    const int note = notes[i + 1];\n"
    "    const int8_t timing = timings.empty()\n"
    "        ? 0\n"
    "        : timings[static_cast<std::size_t>(step)];\n"
    "    setStep(pattern, step, note, 100, timing);\n"
    "  }\n"
    "  assert(f.engine.refreshPatternRuntimeEvents(\n"
    "      synth, f.engine.current303BankIndex(synth), patternIndex));\n"
    "}\n",
)

# Song boundary: trigger through the actual immutable scheduler; lifecycle
# asymmetry remains characterized until the dedicated barrier slice.
replace_once(
    P0,
    "  f.engine.playing = true;\n"
    "  f.beginRender();\n"
    "  f.engine.triggerSynthStep_(synth, 15);\n"
    "  f.endRender();\n"
    "  f.dispatchLikeProduction();\n\n"
    "  assert(f.noteHeld(synth));\n"
    "  const long gateBefore = f.gateCountdown(synth);\n"
    "  assert(gateBefore > 0);\n",
    "  f.engine.playing = true;\n"
    "  f.beginRender();\n"
    "  processTick(f, 360);\n"
    "  f.endRender();\n"
    "  f.dispatchLikeProduction();\n\n"
    "  assert(f.noteHeld(synth));\n"
    "  assert(f.runtimeActive(synth));\n"
    "  const uint32_t deadlineBefore = f.runtimeDeadline(synth);\n"
    "  assert(deadlineBefore > 360u * PhraseRuntime::kSubticksPerTick);\n",
)
replace_once(
    P0,
    "  assert(f.noteHeld(synth));\n"
    "  assert(f.gateCountdown(synth) == gateBefore);\n",
    "  assert(f.noteHeld(synth));\n"
    "  assert(f.runtimeActive(synth));\n"
    "  assert(f.runtimeDeadline(synth) == deadlineBefore);\n",
)

# TIE is now folded into immutable duration before onset. There is intentionally
# no second runtime token at the tie tick.
replace_once(
    P0,
    "  processTick(f, 383);  // step15 onset shifted +23 ticks\n"
    "  const long gateBeforeTie = f.gateCountdown(synth);\n"
    "  assert(gateBeforeTie > 0);\n"
    "  processTick(f, 384);  // exact physical boundary\n"
    "  assert(f.gateCountdown(synth) == gateBeforeTie);\n"
    "  processTick(f, 385);  // step0 TIE shifted to barTick 1\n"
    "  const long gateAfterTie = f.gateCountdown(synth);\n",
    "  processTick(f, 383);  // step15 onset shifted +23 ticks\n"
    "  assert(f.runtimeActive(synth));\n"
    "  const uint32_t deadline = f.runtimeDeadline(synth);\n"
    "  assert(deadline > 384u * PhraseRuntime::kSubticksPerTick);\n"
    "  processTick(f, 384);  // exact physical boundary\n"
    "  assert(f.runtimeActive(synth));\n"
    "  assert(f.runtimeDeadline(synth) == deadline);\n"
    "  processTick(f, 385);  // folded TIE token itself emits no onset\n"
    "  assert(f.runtimeActive(synth));\n",
)
replace_once(
    P0,
    "  assert(gateAfterTie > gateBeforeTie);\n"
    "  assert(f.engine.patternMidiNotes_[synth] == 60);\n",
    "  assert(f.runtimeDeadline(synth) == deadline);\n"
    "  assert(f.engine.patternMidiNotes_[synth] == 60);\n",
)

replace_once(
    P0,
    "  scene.feel.swingMask = static_cast<uint16_t>(\n"
    "      1u << static_cast<int>(synth == 0 ? VoiceId::SynthA : VoiceId::SynthB));\n"
    "}\n",
    "  scene.feel.swingMask = static_cast<uint16_t>(\n"
    "      1u << static_cast<int>(synth == 0 ? VoiceId::SynthA : VoiceId::SynthB));\n"
    "  assert(f.engine.rebuildPatternRuntimeEventBank());\n"
    "}\n",
)

for old, new in (
    ("assert(f.gateCountdown(synth) == 0);", "assert(!f.runtimeActive(synth));"),
    ("assert(f.gateCountdown(synth) > 0);", "assert(f.runtimeActive(synth));"),
):
    value = text(P0)
    if old not in value:
        raise RuntimeError(f"{P0}: missing expected gate assertion {old}")
    write(P0, value.replace(old, new))

print("P2 executor cutover patch applied")
