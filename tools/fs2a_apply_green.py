#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
HEADER = ROOT / "src/dsp/miniacid_engine.h"
CPP = ROOT / "src/dsp/miniacid_engine.cpp"


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{label}: expected exactly one match, found {count}")
    return text.replace(old, new, 1)


header = HEADER.read_text()
cpp = CPP.read_text()

header = replace_once(
    header,
    '#include "src/state/material_slot.h"\n#include "src/state/material_slot_access.h"\n#include "src/state/working_material_storage.h"\n',
    '#include "src/state/material_slot.h"\n#include "src/state/material_slot_access.h"\n#include "src/state/material_version.h"\n#include "src/state/working_material_storage.h"\n',
    "material_version include",
)

header = replace_once(
    header,
    '  // Called on a musical boundary. A value copy and two assignments: no\n'
    '  // allocation, no I/O, nothing that can fail halfway.\n'
    '  void activatePendingMaterial();\n'
    '  float getStepProgress() const;\n',
    '  // Called on a musical boundary. A value copy and two assignments: no\n'
    '  // allocation, no I/O, nothing that can fail halfway.\n'
    '  void activatePendingMaterial();\n\n'
    '  // FS2A: session-only CURRENT/NEXT lifecycle. ACCEPT remains the separate\n'
    '  // durable CURRENT -> CANONICAL boundary. Lifecycle NEXT is always bound\n'
    '  // to the exact accepted reference/version it was prepared against.\n'
    '  enum class NextPrepareResult : uint8_t {\n'
    '    Prepared = 0,\n'
    '    Replaced,\n'
    '    InvalidVoice,\n'
    '    InvalidCandidate,\n'
    '    RejectedCurrentDirty,\n'
    '    UnsupportedCurrentState,\n'
    '    PendingUnavailable,\n'
    '  };\n\n'
    '  enum class NextActivationResult : uint8_t {\n'
    '    Activated = 0,\n'
    '    InvalidVoice,\n'
    '    NoPending,\n'
    '    UnboundPending,\n'
    '    RejectedReferenceMismatch,\n'
    '    RejectedCanonicalChanged,\n'
    '    RejectedCurrentDirty,\n'
    '    UnsupportedCurrentState,\n'
    '  };\n\n'
    '  NextPrepareResult prepareNextMelody(\n'
    '      int voiceIndex,\n'
    '      const PhraseRuntime::RuntimeSynthEventBuffer& melody);\n'
    '  bool cancelNextMaterial(int voiceIndex);\n'
    '  NextActivationResult activateNextMaterialAtBoundary(int voiceIndex);\n'
    '  float getStepProgress() const;\n',
    "public FS2A API",
)

header = replace_once(
    header,
    '  bool current303MaterialReference_(\n'
    '      int voiceIndex, GroovePuterMaterial::MaterialReference& out) const;\n'
    '  const SynthPattern& synthPattern(int synthIndex) const;\n',
    '  bool current303MaterialReference_(\n'
    '      int voiceIndex, GroovePuterMaterial::MaterialReference& out) const;\n'
    '  enum class CurrentNextState : uint8_t {\n'
    '    CleanAcceptedPattern = 0,\n'
    '    DirtyCurrent,\n'
    '    UnsupportedCurrentState,\n'
    '  };\n'
    '  CurrentNextState classifyCurrentForNext_(\n'
    '      int voiceIndex, GroovePuterMaterial::MaterialReference& reference,\n'
    '      GroovePuterMaterial::MaterialVersionToken& acceptedVersion) const;\n'
    '  bool activatePendingMaterialForVoice_(int voiceIndex);\n'
    '  const SynthPattern& synthPattern(int synthIndex) const;\n',
    "private FS2A helpers",
)

header = replace_once(
    header,
    '  struct PendingMaterial {\n'
    '    PhraseRuntime::RuntimeSynthEventBuffer* melody = nullptr;\n'
    '    uint16_t slot = 0;\n'
    '    GroovePuterMaterial::MaterialKind kind =\n'
    '        GroovePuterMaterial::MaterialKind::Pattern;\n'
    '    bool queued = false;\n'
    '  };\n',
    '  struct PendingMaterial {\n'
    '    PhraseRuntime::RuntimeSynthEventBuffer* melody = nullptr;\n'
    '    uint16_t slot = 0;\n'
    '    GroovePuterMaterial::MaterialKind kind =\n'
    '        GroovePuterMaterial::MaterialKind::Pattern;\n'
    '    bool queued = false;\n\n'
    '    // FS2A causal stamp. These are metadata only; the existing Melody\n'
    '    // buffer remains the sole NEXT musical payload owner.\n'
    '    GroovePuterMaterial::MaterialReference preparedFor{};\n'
    '    GroovePuterMaterial::MaterialVersionToken acceptedVersion{};\n'
    '    bool lifecycleBound = false;\n'
    '  };\n',
    "pending causal metadata",
)

fs2a_impl = r'''MiniAcid::CurrentNextState MiniAcid::classifyCurrentForNext_(
    int voiceIndex, GroovePuterMaterial::MaterialReference& reference,
    GroovePuterMaterial::MaterialVersionToken& acceptedVersion) const {
  using GroovePuterMaterial::MaterialKind;

  reference = {};
  acceptedVersion = {};
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return CurrentNextState::UnsupportedCurrentState;
  }

  const int idx = clamp303Voice(voiceIndex);
  if (!current303MaterialReference_(idx, reference)) {
    return CurrentNextState::UnsupportedCurrentState;
  }

  const Scene& scene = sceneManager_.currentScene();
  const int residentSlot = GroovePuterMaterial::residentSlotFor(reference.address);
  if (GroovePuterMaterial::residentKind(scene, idx, residentSlot) !=
      MaterialKind::Pattern) {
    // Accepted Melody requires filesystem resolution to prove canonical bytes.
    // FS2A deliberately performs no SD I/O in the NEXT lifecycle gate.
    return CurrentNextState::UnsupportedCurrentState;
  }

  const int bank = current303BankIndex(idx);
  const int pattern = display303LocalPatternIndex(idx);
  if (bank < 0 || bank >= kBankCount || pattern < 0 ||
      pattern >= Bank<SynthPattern>::kPatterns) {
    return CurrentNextState::UnsupportedCurrentState;
  }

  const SynthPattern& accepted = idx == 0
      ? scene.synthABanks[bank].patterns[pattern]
      : scene.synthBBanks[bank].patterns[pattern];
  acceptedVersion = GroovePuterMaterial::versionForPattern(accepted);

  const auto& working = workingMaterial_[idx];
  if (working.empty()) return CurrentNextState::CleanAcceptedPattern;
  if (working.holdsMelody()) return CurrentNextState::DirtyCurrent;
  if (!working.patternMatches(reference)) {
    // Retained Working that cannot be proven to belong to CURRENT is not safe
    // to overwrite. Fail closed instead of guessing from address alone.
    return CurrentNextState::UnsupportedCurrentState;
  }
  if (GroovePuterMaterial::versionForPattern(working.pattern()) !=
      acceptedVersion) {
    return CurrentNextState::DirtyCurrent;
  }
  return CurrentNextState::CleanAcceptedPattern;
}

MiniAcid::NextPrepareResult MiniAcid::prepareNextMelody(
    int voiceIndex,
    const PhraseRuntime::RuntimeSynthEventBuffer& melody) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return NextPrepareResult::InvalidVoice;
  }
  if (!RuntimePhraseEdit::validate(melody)) {
    return NextPrepareResult::InvalidCandidate;
  }

  PendingMaterial& pending = pendingMaterial_[voiceIndex];
  if (pending.melody == nullptr) {
    return NextPrepareResult::PendingUnavailable;
  }

  GroovePuterMaterial::MaterialReference reference{};
  GroovePuterMaterial::MaterialVersionToken acceptedVersion{};
  const CurrentNextState state =
      classifyCurrentForNext_(voiceIndex, reference, acceptedVersion);
  if (state == CurrentNextState::DirtyCurrent) {
    return NextPrepareResult::RejectedCurrentDirty;
  }
  if (state != CurrentNextState::CleanAcceptedPattern) {
    return NextPrepareResult::UnsupportedCurrentState;
  }

  const bool replacingLifecycleCandidate =
      pending.queued && pending.lifecycleBound;

  // stagePendingMaterial validates/copies before publishing queued metadata, so
  // causal metadata is replaced only after the new payload is fully staged.
  if (!stagePendingMaterial(
          voiceIndex, static_cast<uint16_t>(reference.address.globalSlot),
          GroovePuterMaterial::MaterialKind::Melody, &melody)) {
    return NextPrepareResult::PendingUnavailable;
  }

  pending.preparedFor = reference;
  pending.acceptedVersion = acceptedVersion;
  pending.lifecycleBound = true;
  return replacingLifecycleCandidate ? NextPrepareResult::Replaced
                                     : NextPrepareResult::Prepared;
}

bool MiniAcid::cancelNextMaterial(int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;
  PendingMaterial& pending = pendingMaterial_[voiceIndex];
  if (!pending.queued || !pending.lifecycleBound) return false;

  pending.queued = false;
  pending.lifecycleBound = false;
  pending.preparedFor = {};
  pending.acceptedVersion = {};
  return true;
}

MiniAcid::NextActivationResult MiniAcid::activateNextMaterialAtBoundary(
    int voiceIndex) {
  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) {
    return NextActivationResult::InvalidVoice;
  }

  PendingMaterial& pending = pendingMaterial_[voiceIndex];
  if (!pending.queued) return NextActivationResult::NoPending;
  if (!pending.lifecycleBound) return NextActivationResult::UnboundPending;

  GroovePuterMaterial::MaterialReference currentReference{};
  if (!current303MaterialReference_(voiceIndex, currentReference)) {
    return NextActivationResult::UnsupportedCurrentState;
  }
  if (currentReference.address != pending.preparedFor.address ||
      currentReference.id != pending.preparedFor.id) {
    return NextActivationResult::RejectedReferenceMismatch;
  }

  GroovePuterMaterial::MaterialReference provenReference{};
  GroovePuterMaterial::MaterialVersionToken currentVersion{};
  const CurrentNextState state =
      classifyCurrentForNext_(voiceIndex, provenReference, currentVersion);
  if (state == CurrentNextState::UnsupportedCurrentState) {
    return NextActivationResult::UnsupportedCurrentState;
  }
  if (currentVersion != pending.acceptedVersion) {
    return NextActivationResult::RejectedCanonicalChanged;
  }
  if (state == CurrentNextState::DirtyCurrent) {
    return NextActivationResult::RejectedCurrentDirty;
  }

  if (pending.kind != GroovePuterMaterial::MaterialKind::Melody ||
      pending.melody == nullptr) {
    return NextActivationResult::UnboundPending;
  }

  // Boundary publication is bounded: value copy into the existing Working
  // owner, runtime descriptor publication, then candidate-state clear.
  if (!activatePendingMaterialForVoice_(voiceIndex)) {
    return NextActivationResult::UnboundPending;
  }
  return NextActivationResult::Activated;
}

'''

cpp = replace_once(
    cpp,
    'bool MiniAcid::stagePendingMaterial(\n',
    fs2a_impl + 'bool MiniAcid::stagePendingMaterial(\n',
    "FS2A implementation insertion",
)

cpp = replace_once(
    cpp,
    'void MiniAcid::activatePendingMaterial() {\n'
    '  for (int voice = 0; voice < NUM_303_VOICES; ++voice) {\n'
    '    PendingMaterial& pending = pendingMaterial_[voice];\n'
    '    if (!pending.queued) continue;\n'
    '    if (pending.kind == GroovePuterMaterial::MaterialKind::Melody &&\n'
    '        pending.melody != nullptr) {\n'
    '      workingMaterial_[voice].storeMelody(*pending.melody);\n'
    '    }\n'
    '    publishActiveMaterial(voice, pending.slot, pending.kind);\n'
    '    pending.queued = false;\n'
    '  }\n'
    '}\n',
    'bool MiniAcid::activatePendingMaterialForVoice_(int voiceIndex) {\n'
    '  if (voiceIndex < 0 || voiceIndex >= NUM_303_VOICES) return false;\n'
    '  PendingMaterial& pending = pendingMaterial_[voiceIndex];\n'
    '  if (!pending.queued) return false;\n'
    '  if (pending.kind == GroovePuterMaterial::MaterialKind::Melody &&\n'
    '      pending.melody != nullptr) {\n'
    '    workingMaterial_[voiceIndex].storeMelody(*pending.melody);\n'
    '  }\n'
    '  publishActiveMaterial(voiceIndex, pending.slot, pending.kind);\n'
    '  pending.queued = false;\n'
    '  pending.lifecycleBound = false;\n'
    '  return true;\n'
    '}\n\n'
    'void MiniAcid::activatePendingMaterial() {\n'
    '  for (int voice = 0; voice < NUM_303_VOICES; ++voice) {\n'
    '    (void)activatePendingMaterialForVoice_(voice);\n'
    '  }\n'
    '}\n',
    "per-voice activation factor",
)

HEADER.write_text(header)
CPP.write_text(cpp)
print("FS2A production patch applied")
