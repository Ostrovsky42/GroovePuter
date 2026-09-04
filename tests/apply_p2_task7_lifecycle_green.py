#!/usr/bin/env python3
from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one match, found {count}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1))


HEADER = "src/dsp/miniacid_engine.h"
CPP = "src/dsp/miniacid_engine.cpp"
GEN = "src/generation/migration/quantized_generation_undo_owner_impl.h"

# Public runtime-source barrier seam and out-of-line page identity transition.
replace_once(
    HEADER,
    "  void setCurrentPage(int8_t page) { currentPage_.store(page, std::memory_order_release); }\n"
    "  bool rebuildPatternRuntimeEventBank();\n"
    "  bool refreshPatternRuntimeEvents(int synthIndex, int bankIndex, int patternIndex);\n"
    "  const PhraseRuntime::RuntimePatternEventBuffer& activePatternRuntimeEvents(int synthIndex) const;\n"
    "  void requestPageSwitch(int pageIndex);",
    "  void setCurrentPage(int8_t page);\n"
    "  bool rebuildPatternRuntimeEventBank();\n"
    "  bool refreshPatternRuntimeEvents(int synthIndex, int bankIndex, int patternIndex);\n"
    "  const PhraseRuntime::RuntimePatternEventBuffer& activePatternRuntimeEvents(int synthIndex) const;\n"
    "  void barrierPatternRuntimeSourceTransition();\n"
    "  void requestPageSwitch(int pageIndex);",
)
replace_once(
    HEADER,
    "  void hardBarrierPatternPlayback_();\n"
    "  void cleanupLiveNotesForTransportBarrier_(uint8_t patternAuthorityAtEntry);",
    "  void hardBarrierPatternPlayback_(int synthIdx);\n"
    "  void hardBarrierPatternPlayback_();\n"
    "  void cleanupLiveNotesForTransportBarrier_(uint8_t patternAuthorityAtEntry);",
)

# MUTE: RuntimeSynthPlaybackState remains the single Pattern lifetime owner.
replace_once(CPP, "  if (muted) publishPatternNoteOff_(idx);\n  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);",
             "  if (muted) hardBarrierPatternPlayback_(idx);\n  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);")
# There are two identical mute tails; replace_once above patches toggleMute303 first,
# then patch the remaining setMute303 occurrence.
replace_once(CPP, "  if (muted) publishPatternNoteOff_(idx);\n  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);",
             "  if (muted) hardBarrierPatternPlayback_(idx);\n  LedManager::instance().onMuteChanged(muted, sceneManager_.currentScene().led);")

# Pattern identity transitions are target-scoped lifetime barriers.
replace_once(
    CPP,
    "void MiniAcid::set303PatternIndex(int voiceIndex, int16_t patternIndex) {\n"
    "  int idx = clamp303Voice(voiceIndex);\n"
    "  if (playing) publishPatternNoteOff_(idx);\n"
    "  sceneManager_.setCurrentSynthPatternIndex(idx, patternIndex);\n"
    "}",
    "void MiniAcid::set303PatternIndex(int voiceIndex, int16_t patternIndex) {\n"
    "  int idx = clamp303Voice(voiceIndex);\n"
    "  hardBarrierPatternPlayback_(idx);\n"
    "  sceneManager_.setCurrentSynthPatternIndex(idx, patternIndex);\n"
    "}",
)
replace_once(
    CPP,
    "void MiniAcid::shift303PatternIndex(int voiceIndex, int delta) {\n"
    "  int idx = clamp303Voice(voiceIndex);\n"
    "  if (playing) publishPatternNoteOff_(idx);",
    "void MiniAcid::shift303PatternIndex(int voiceIndex, int delta) {\n"
    "  int idx = clamp303Voice(voiceIndex);\n"
    "  hardBarrierPatternPlayback_(idx);",
)
replace_once(
    CPP,
    "void MiniAcid::set303BankIndex(int voiceIndex, int bankIndex) {\n"
    "  int idx = clamp303Voice(voiceIndex);\n"
    "  if (playing) publishPatternNoteOff_(idx);\n"
    "  sceneManager_.setCurrentBankIndex(idx + 1, bankIndex);\n"
    "}",
    "void MiniAcid::set303BankIndex(int voiceIndex, int bankIndex) {\n"
    "  int idx = clamp303Voice(voiceIndex);\n"
    "  hardBarrierPatternPlayback_(idx);\n"
    "  sceneManager_.setCurrentBankIndex(idx + 1, bankIndex);\n"
    "}",
)
replace_once(
    CPP,
    "void MiniAcid::randomize303Pattern(int voiceIndex) {\n"
    "  int idx = clamp303Voice(voiceIndex);\n"
    "  if (playing) publishPatternNoteOff_(idx);",
    "void MiniAcid::randomize303Pattern(int voiceIndex) {\n"
    "  int idx = clamp303Voice(voiceIndex);\n"
    "  hardBarrierPatternPlayback_(idx);",
)

# SONG/PATTERN source changes are all-target barriers, never global MIDI panic.
replace_once(
    CPP,
    "void MiniAcid::setSongMode(bool enabled) {\n"
    "  if (enabled == songMode_) return;\n"
    "  songBarIndex_ = -1;\n"
    "  if (playing) publishPatternAllNotesOff_();",
    "void MiniAcid::setSongMode(bool enabled) {\n"
    "  if (enabled == songMode_) return;\n"
    "  songBarIndex_ = -1;\n"
    "  hardBarrierPatternPlayback_();",
)
replace_once(
    CPP,
    "void MiniAcid::applySongPositionSelection() {\n"
    "  if (!songMode_) return;\n"
    "  if (playing) publishPatternAllNotesOff_();",
    "void MiniAcid::applySongPositionSelection() {\n"
    "  if (!songMode_) return;\n"
    "  hardBarrierPatternPlayback_();",
)

# Synth-engine replacement snapshots backend authority before the Pattern barrier.
# Pattern-owned backends are physically released by the common consumer exactly once;
# otherwise preserve the legacy live/direct backend release before engine replacement.
replace_once(
    CPP,
    "  if (playing) publishPatternNoteOff_(idx);\n"
    "  if (synthVoices_[idx]) synthVoices_[idx]->release();\n"
    "  liveNotes_[idx] = -1;\n"
    "  ++liveInputEpoch_;",
    "  const uint8_t patternAuthorityAtEntry =\n"
    "      patternOwnedMask_.load(std::memory_order_acquire);\n"
    "  hardBarrierPatternPlayback_(idx);\n"
    "  const uint8_t targetMask = static_cast<uint8_t>(1u << idx);\n"
    "  if ((patternAuthorityAtEntry & targetMask) == 0u && synthVoices_[idx]) {\n"
    "    synthVoices_[idx]->release();\n"
    "  }\n"
    "  liveNotes_[idx] = -1;\n"
    "  ++liveInputEpoch_;",
)

# Page identity is itself a source transition and is deliberately out-of-line.
replace_once(
    CPP,
    "void MiniAcid::requestPageSwitch(int pageIndex) {",
    "void MiniAcid::setCurrentPage(int8_t page) {\n"
    "  hardBarrierPatternPlayback_();\n"
    "  currentPage_.store(page, std::memory_order_release);\n"
    "}\n\n"
    "void MiniAcid::requestPageSwitch(int pageIndex) {",
)

# Split the already-approved all-target barrier into target-scoped + delegating forms,
# and expose one backend-neutral seam to non-MiniAcid source owners.
replace_once(
    CPP,
    "void MiniAcid::hardBarrierPatternPlayback_() {\n"
    "  for (int synth = 0; synth < NUM_303_VOICES; ++synth) {\n"
    "    const auto actions = patternPlaybackState_[synth].hardBarrier();\n"
    "    consumePatternPlaybackActions_(synth, actions);\n"
    "  }\n"
    "}",
    "void MiniAcid::hardBarrierPatternPlayback_(int synthIdx) {\n"
    "  const int idx = clamp303Voice(synthIdx);\n"
    "  const auto actions = patternPlaybackState_[idx].hardBarrier();\n"
    "  consumePatternPlaybackActions_(idx, actions);\n"
    "}\n\n"
    "void MiniAcid::hardBarrierPatternPlayback_() {\n"
    "  for (int synth = 0; synth < NUM_303_VOICES; ++synth) {\n"
    "    hardBarrierPatternPlayback_(synth);\n"
    "  }\n"
    "}\n\n"
    "void MiniAcid::barrierPatternRuntimeSourceTransition() {\n"
    "  hardBarrierPatternPlayback_();\n"
    "}",
)

# Pending-generation overlay removal/activation changes the authoritative audible source.
replace_once(
    GEN,
    "  int8_t expectedSlot = static_cast<int8_t>(slot);\n"
    "  g_publishedSlot.compare_exchange_strong(\n"
    "      expectedSlot, -1, std::memory_order_acq_rel, std::memory_order_acquire);\n"
    "  g_slotState[slot].store(\n"
    "      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);\n"
    "  g_status.store(\n"
    "      static_cast<uint8_t>(QuantizedGenerationStatus::CancelledExplicit),",
    "  int8_t expectedSlot = static_cast<int8_t>(slot);\n"
    "  g_publishedSlot.compare_exchange_strong(\n"
    "      expectedSlot, -1, std::memory_order_acq_rel, std::memory_order_acquire);\n"
    "  engine.barrierPatternRuntimeSourceTransition();\n"
    "  g_slotState[slot].store(\n"
    "      static_cast<uint8_t>(SlotState::Empty), std::memory_order_release);\n"
    "  g_status.store(\n"
    "      static_cast<uint8_t>(QuantizedGenerationStatus::CancelledExplicit),",
)
# The non-revision cancellation has the same publication tail but no explicit Empty store.
replace_once(
    GEN,
    "  int8_t expectedSlot = static_cast<int8_t>(slot);\n"
    "  g_publishedSlot.compare_exchange_strong(\n"
    "      expectedSlot, -1, std::memory_order_acq_rel, std::memory_order_acquire);\n"
    "  g_status.store(\n"
    "      static_cast<uint8_t>(QuantizedGenerationStatus::CancelledExplicit),",
    "  int8_t expectedSlot = static_cast<int8_t>(slot);\n"
    "  g_publishedSlot.compare_exchange_strong(\n"
    "      expectedSlot, -1, std::memory_order_acq_rel, std::memory_order_acquire);\n"
    "  engine.barrierPatternRuntimeSourceTransition();\n"
    "  g_status.store(\n"
    "      static_cast<uint8_t>(QuantizedGenerationStatus::CancelledExplicit),",
)
replace_once(
    GEN,
    "  // ACTIVATE is runtime-only: release the old audible overlay and synchronize\n"
    "  // deferred mode/BPM. No Scene write, revision, Undo publication, allocation,\n"
    "  // filesystem access or generation occurs at BAR_START.\n"
    "  activatePreparedGenerationRuntime(*owner, pending);",
    "  // ACTIVATE changes the authoritative audible Pattern source. End any active\n"
    "  // lifetime through the single runtime owner before the overlay disappears.\n"
    "  owner->barrierPatternRuntimeSourceTransition();\n"
    "  // ACTIVATE is runtime-only: synchronize deferred mode/BPM. No Scene write,\n"
    "  // revision, Undo publication, allocation, filesystem access or generation\n"
    "  // occurs at BAR_START.\n"
    "  activatePreparedGenerationRuntime(*owner, pending);",
)

print("P2 Task-7 lifecycle GREEN candidate patch applied")
