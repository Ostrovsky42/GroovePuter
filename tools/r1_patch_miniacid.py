#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/dsp/miniacid_engine.h"
SOURCE = ROOT / "src/dsp/miniacid_engine.cpp"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"R1 patch {label}: expected exactly one match, got {count}")
    return text.replace(old, new, 1)


def insert_after_function_open(text: str, signature: str, line: str, label: str) -> str:
    old = signature + " {\n"
    new = old + line + "\n"
    return replace_once(text, old, new, label)


h = HEADER.read_text()
h = replace_once(
    h,
    '#include "pattern_drum_event_tap.h"\n',
    '#include "pattern_drum_event_tap.h"\n#include "phrase_crossbar_lifetime_runtime.h"\n',
    "header include",
)
h = replace_once(
    h,
    '  void setPatternEventQueue(MusicalEventQueue* queue);\n  int liveNote(int synthIndex) const;\n',
    '  void setPatternEventQueue(MusicalEventQueue* queue);\n'
    '  bool setPhraseCrossBarLifetimeContext(\n'
    '      const GroovePuterPhraseRuntime::PhraseCrossBarLifetimeContext& context);\n'
    '  void clearPhraseCrossBarLifetimeContext();\n'
    '  int liveNote(int synthIndex) const;\n',
    "public runtime handoff",
)
h = replace_once(
    h,
    '  void publishPatternNoteOff_(int synthIdx, uint8_t velocity = 0);\n'
    '  void publishPatternAllNotesOff_();\n'
    '  void triggerDrumVoice_(int voiceIdx, int stepIdx);\n'
    '  void advanceSongBar_();\n',
    '  void publishPatternNoteOff_(int synthIdx, uint8_t velocity = 0);\n'
    '  void publishPatternAllNotesOff_(bool preserveCrossBarHeldSynthB = false);\n'
    '  void releasePatternSynthBHeldNote_(int16_t note);\n'
    '  void invalidatePhraseCrossBarLifetime_();\n'
    '  void triggerDrumVoice_(int voiceIdx, int stepIdx);\n'
    '  void advanceSongBar_();\n',
    "private runtime helpers",
)
h = replace_once(
    h,
    '  void applySongPositionSelection();\n',
    '  void applySongPositionSelection(\n'
    '      bool ordinaryPhraseTransition = false,\n'
    '      bool preserveCrossBarHeldSynthB = false);\n',
    "song transition signature",
)
h = replace_once(
    h,
    '  long gateCountdownA_ = 0;\n  long gateCountdownB_ = 0;\n',
    '  long gateCountdownA_ = 0;\n'
    '  long gateCountdownB_ = 0;\n'
    '  GroovePuterPhraseRuntime::PhraseCrossBarLifetimeExecutor\n'
    '      phraseCrossBarLifetime_{};\n',
    "runtime member",
)
HEADER.write_text(h)

cpp = SOURCE.read_text()
cpp = replace_once(
    cpp,
    'void MiniAcid::reset() {\n'
    '  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);\n',
    'void MiniAcid::reset() {\n'
    '  GroovePuterRhythm::QuantizedGenerationDetail::cancelPendingGenerationActivation(*this);\n'
    '  phraseCrossBarLifetime_.reset();\n',
    "reset clear",
)
cpp = replace_once(
    cpp,
    '  allLiveNotesOff();\n  publishPatternAllNotesOff_();\n  playing = true;\n',
    '  allLiveNotesOff();\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '  publishPatternAllNotesOff_();\n'
    '  playing = true;\n',
    "start barrier",
)
cpp = replace_once(
    cpp,
    '  LOG_PRINTLN("[DSP] STOP command received");\n  publishPatternAllNotesOff_();\n',
    '  LOG_PRINTLN("[DSP] STOP command received");\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '  publishPatternAllNotesOff_();\n',
    "stop barrier",
)
cpp = replace_once(
    cpp,
    '  LOG_PRINTLN("[DSP] PAUSE command received");\n  publishPatternAllNotesOff_();\n',
    '  LOG_PRINTLN("[DSP] PAUSE command received");\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '  publishPatternAllNotesOff_();\n',
    "pause barrier",
)
cpp = replace_once(
    cpp,
    '  LOG_PRINTLN("[DSP] CONTINUE command received");\n  allLiveNotesOff();\n  publishPatternAllNotesOff_();\n',
    '  LOG_PRINTLN("[DSP] CONTINUE command received");\n'
    '  allLiveNotesOff();\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '  publishPatternAllNotesOff_();\n',
    "continue barrier",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::setPatternEventQueue(MusicalEventQueue* queue) {\n'
    '  patternEventQueue_ = queue;\n'
    '  patternMidiNotes_[0] = -1;\n'
    '  patternMidiNotes_[1] = -1;\n'
    '}\n',
    'void MiniAcid::setPatternEventQueue(MusicalEventQueue* queue) {\n'
    '  if (patternEventQueue_ != queue) invalidatePhraseCrossBarLifetime_();\n'
    '  patternEventQueue_ = queue;\n'
    '  patternMidiNotes_[0] = -1;\n'
    '  patternMidiNotes_[1] = -1;\n'
    '}\n\n'
    'bool MiniAcid::setPhraseCrossBarLifetimeContext(\n'
    '    const GroovePuterPhraseRuntime::PhraseCrossBarLifetimeContext& context) {\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '  return phraseCrossBarLifetime_.activate(context);\n'
    '}\n\n'
    'void MiniAcid::clearPhraseCrossBarLifetimeContext() {\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '}\n',
    "runtime handoff definitions",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::publishPatternAllNotesOff_() {\n'
    '  for (int idx = 0; idx < NUM_303_VOICES; ++idx) {\n',
    'void MiniAcid::publishPatternAllNotesOff_(bool preserveCrossBarHeldSynthB) {\n'
    '  for (int idx = 0; idx < NUM_303_VOICES; ++idx) {\n'
    '    if (idx == 1 && preserveCrossBarHeldSynthB &&\n'
    '        phraseCrossBarLifetime_.heldCrossedBoundary()) {\n'
    '      continue;\n'
    '    }\n',
    "selective all notes off",
)
# Put the physical execution helpers immediately before liveNote(), after the
# PatternPlayer publication helpers and runtime handoff definitions.
cpp = replace_once(
    cpp,
    'int MiniAcid::liveNote(int synthIndex) const {\n',
    'void MiniAcid::releasePatternSynthBHeldNote_(int16_t note) {\n'
    '  if (note < 0) return;\n'
    '  gateCountdownB_ = 0;\n'
    '  retrigB_ = {};\n'
    '  if (synthVoices_[1]) synthVoices_[1]->release();\n'
    '  publishPatternNoteOff_(1);\n'
    '}\n\n'
    'void MiniAcid::invalidatePhraseCrossBarLifetime_() {\n'
    '  const int16_t heldNote = phraseCrossBarLifetime_.hardBarrierRelease();\n'
    '  releasePatternSynthBHeldNote_(heldNote);\n'
    '}\n\n'
    'int MiniAcid::liveNote(int synthIndex) const {\n',
    "physical release helpers",
)
cpp = replace_once(
    cpp,
    '      if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0) {\n'
    '        if (synthVoices_[1]) synthVoices_[1]->release();\n'
    '        publishPatternNoteOff_(1);\n'
    '      }\n',
    '      if (gateCountdownB_ > 0 && --gateCountdownB_ <= 0) {\n'
    '        if (phraseCrossBarLifetime_.suppressOrdinaryGateExpiry()) {\n'
    '          gateCountdownB_ = 0;\n'
    '        } else {\n'
    '          if (synthVoices_[1]) synthVoices_[1]->release();\n'
    '          publishPatternNoteOff_(1);\n'
    '        }\n'
    '      }\n',
    "gate expiry suppression",
)
cpp = replace_once(
    cpp,
    '        if (synthVoices_[synthIdx]) synthVoices_[synthIdx]->startNote(noteToFreq(step.note), step.accent, step.slide, (uint8_t)step.velocity);\n'
    '        publishPatternNoteOn_(synthIdx, static_cast<uint8_t>(step.note), static_cast<uint8_t>(step.velocity));\n'
    '        long dur = (long)(samplesPerStep_ * effectiveGateMult);\n',
    '        if (synthIdx == 1) {\n'
    '          const int16_t heldNote =\n'
    '              phraseCrossBarLifetime_.consumeTerminatorBeforeNoteOn();\n'
    '          releasePatternSynthBHeldNote_(heldNote);\n'
    '        }\n'
    '        if (synthVoices_[synthIdx]) synthVoices_[synthIdx]->startNote(noteToFreq(step.note), step.accent, step.slide, (uint8_t)step.velocity);\n'
    '        publishPatternNoteOn_(synthIdx, static_cast<uint8_t>(step.note), static_cast<uint8_t>(step.velocity));\n'
    '        if (synthIdx == 1) {\n'
    '          phraseCrossBarLifetime_.armOutgoingNote(\n'
    '              static_cast<uint8_t>(stepIdx), step.note);\n'
    '        }\n'
    '        long dur = (long)(samplesPerStep_ * effectiveGateMult);\n',
    "terminator and arm",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::applySongPositionSelection() {\n'
    '  if (!songMode_) return;\n'
    '  if (playing) publishPatternAllNotesOff_();\n',
    'void MiniAcid::applySongPositionSelection(\n'
    '    bool ordinaryPhraseTransition,\n'
    '    bool preserveCrossBarHeldSynthB) {\n'
    '  if (!songMode_) {\n'
    '    if (!ordinaryPhraseTransition) invalidatePhraseCrossBarLifetime_();\n'
    '    return;\n'
    '  }\n'
    '  if (!ordinaryPhraseTransition) invalidatePhraseCrossBarLifetime_();\n'
    '  if (playing) {\n'
    '    publishPatternAllNotesOff_(\n'
    '        ordinaryPhraseTransition && preserveCrossBarHeldSynthB);\n'
    '  }\n',
    "song selection barrier",
)
cpp = replace_once(
    cpp,
    '  if (rowIsPause && !rehearsalAcknowledged_) {\n'
    '      waitingForRehearsal_ = true;\n'
    '      // DO NOT advance nextPos - stay on current row\n'
    '      return;\n'
    '  }\n',
    '  if (rowIsPause && !rehearsalAcknowledged_) {\n'
    '      waitingForRehearsal_ = true;\n'
    '      // A held C2 voice may never survive a boundary that does not reach\n'
    '      // its expected incoming phrase bar.\n'
    '      invalidatePhraseCrossBarLifetime_();\n'
    '      // DO NOT advance nextPos - stay on current row\n'
    '      return;\n'
    '  }\n',
    "rehearsal barrier",
)
cpp = replace_once(
    cpp,
    '  // Update SceneManager FIRST so applySongPositionSelection sees new value\n'
    '  sceneManager_.setSongPosition(nextPos);\n'
    '  \n'
    '  // Propagate to UI/Engine state (including playhead local var)\n'
    '  applySongPositionSelection();\n',
    '  bool ordinaryPhraseTransition = false;\n'
    '  bool preserveCrossBarHeldSynthB = false;\n'
    '  if (phraseCrossBarLifetime_.contextActive()) {\n'
    '    const bool forwardAdjacent = !rev && nextPos == currentPos + 1;\n'
    '    if (forwardAdjacent) {\n'
    '      const GroovePuterPhraseRuntime::PhraseBoundaryRuntimeResult result =\n'
    '          phraseCrossBarLifetime_.advanceOrdinarySequentialBoundary();\n'
    '      releasePatternSynthBHeldNote_(result.noteToRelease);\n'
    '      ordinaryPhraseTransition = result.ordinarySequentialAccepted;\n'
    '      preserveCrossBarHeldSynthB =\n'
    '          result.decision ==\n'
    '              GroovePuterPhraseRuntime::LogicalBoundaryDecision::Continue;\n'
    '      if (preserveCrossBarHeldSynthB) {\n'
    '        gateCountdownB_ = 0;\n'
    '        retrigB_ = {};\n'
    '      }\n'
    '    } else {\n'
    '      invalidatePhraseCrossBarLifetime_();\n'
    '    }\n'
    '  }\n\n'
    '  // Update SceneManager FIRST so applySongPositionSelection sees new value\n'
    '  sceneManager_.setSongPosition(nextPos);\n'
    '  \n'
    '  // Propagate to UI/Engine state (including playhead local var)\n'
    '  applySongPositionSelection(ordinaryPhraseTransition,\n'
    '                             preserveCrossBarHeldSynthB);\n',
    "ordinary boundary decision",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::advanceSongBar_() {\n'
    '  const SongCycleBoundary boundary = nextSongCycleBoundary(\n'
    '      songBarIndex_, sceneManager_.currentScene().feel.patternBars);\n'
    '  songBarIndex_ = boundary.barIndex;\n\n'
    '  if (boundary.advanceRow) {\n'
    '    cyclePulseCounter_++;\n'
    '    if (songMode_) {\n'
    '      advanceSongPlayhead();\n'
    '    }\n'
    '  }\n'
    '}\n',
    'void MiniAcid::advanceSongBar_() {\n'
    '  const int previousSongBarIndex = songBarIndex_;\n'
    '  const SongCycleBoundary boundary = nextSongCycleBoundary(\n'
    '      songBarIndex_, sceneManager_.currentScene().feel.patternBars);\n'
    '  songBarIndex_ = boundary.barIndex;\n\n'
    '  if (phraseCrossBarLifetime_.contextActive() &&\n'
    '      previousSongBarIndex >= 0 && !boundary.advanceRow) {\n'
    '    // A C2 phrase bar is one materialized 16-step bar. Repeating the same\n'
    '    // physical Song row cannot be treated as the expected incoming bar.\n'
    '    invalidatePhraseCrossBarLifetime_();\n'
    '  }\n\n'
    '  if (boundary.advanceRow) {\n'
    '    cyclePulseCounter_++;\n'
    '    if (songMode_) {\n'
    '      advanceSongPlayhead();\n'
    '    } else {\n'
    '      invalidatePhraseCrossBarLifetime_();\n'
    '    }\n'
    '  }\n'
    '}\n',
    "bar boundary fail closed",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::setSongPosition(int position) {\n'
    '  int pos = clampSongPosition(position);\n',
    'void MiniAcid::setSongPosition(int position) {\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '  int pos = clampSongPosition(position);\n',
    "seek barrier",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::setSongPattern(int position, SongTrack track, int16_t patternIndex) {\n'
    '  sceneManager_.setSongPattern(position, track, patternIndex);\n',
    'void MiniAcid::setSongPattern(int position, SongTrack track, int16_t patternIndex) {\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '  sceneManager_.setSongPattern(position, track, patternIndex);\n',
    "song pattern replace barrier",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::clearSongPattern(int position, SongTrack track) {\n'
    '  sceneManager_.clearSongPattern(position, track);\n',
    'void MiniAcid::clearSongPattern(int position, SongTrack track) {\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '  sceneManager_.clearSongPattern(position, track);\n',
    "song pattern clear barrier",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::setSongMode(bool enabled) {\n'
    '  if (enabled == songMode_) return;\n',
    'void MiniAcid::setSongMode(bool enabled) {\n'
    '  if (enabled == songMode_) return;\n'
    '  invalidatePhraseCrossBarLifetime_();\n',
    "song mode barrier",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::setSongPlaybackSlot(int slot) {\n'
    '  if (slot < 0) slot = 0;\n',
    'void MiniAcid::setSongPlaybackSlot(int slot) {\n'
    '  invalidatePhraseCrossBarLifetime_();\n'
    '  if (slot < 0) slot = 0;\n',
    "song playback slot barrier",
)
cpp = replace_once(
    cpp,
    'void MiniAcid::setMute303(int voiceIndex, bool muted) {\n'
    '  int idx = clamp303Voice(voiceIndex);\n',
    'void MiniAcid::setMute303(int voiceIndex, bool muted) {\n'
    '  int idx = clamp303Voice(voiceIndex);\n'
    '  if (idx == 1 && muted) invalidatePhraseCrossBarLifetime_();\n',
    "mute barrier",
)
for signature, label in [
    ('void MiniAcid::set303PatternIndex(int voiceIndex, int16_t patternIndex)', 'synth pattern replace'),
    ('void MiniAcid::shift303PatternIndex(int voiceIndex, int delta)', 'synth pattern shift'),
    ('void MiniAcid::set303BankIndex(int voiceIndex, int bankIndex)', 'synth bank replace'),
    ('void MiniAcid::setDrumPatternIndex(int16_t patternIndex)', 'drum pattern replace'),
    ('void MiniAcid::shiftDrumPatternIndex(int delta)', 'drum pattern shift'),
    ('void MiniAcid::setDrumBankIndex(int bankIndex)', 'drum bank replace'),
    ('void MiniAcid::regeneratePatternsWithGenre()', 'regenerate barrier'),
    ('void MiniAcid::applySceneStateFromManager()', 'scene replacement barrier'),
]:
    cpp = insert_after_function_open(
        cpp, signature, '  invalidatePhraseCrossBarLifetime_();', label)

SOURCE.write_text(cpp)
print("R1 MiniAcid patch applied")
