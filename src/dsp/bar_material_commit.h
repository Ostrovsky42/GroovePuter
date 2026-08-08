#pragma once
#ifndef GROOVEPUTER_BAR_MATERIAL_COMMIT_H
#define GROOVEPUTER_BAR_MATERIAL_COMMIT_H

#include <stdint.h>

class MiniAcid;
class SceneManager;

enum class MaterialAction : uint8_t {
  None = 0,
  Generation,
  Variation,
  Phrase,
  Fill,
  Section,
  SongMaterialization,
  RhythmArchetype,
  Chaos,
};

enum class MaterialQueueResult : uint8_t {
  Failed = 0,
  CommittedNow,
  PendingNextBar,
};

enum class MaterialCommitStatus : uint8_t {
  Idle = 0,
  PendingNextBar,
  Committed,
  CancelledPageMismatch,
  CancelledInvalidTarget,
};

// These functions are control-plane entry points. Call them under the existing
// AudioMutationGate when the audio task is active. Generation is completed
// before a pending transaction becomes visible to the audio thread.
MaterialQueueResult queueSynthGenerationForBar(MiniAcid& engine, int voiceIndex);
MaterialQueueResult queueDrumGenerationForBar(MiniAcid& engine);
MaterialQueueResult queueDrumVoiceGenerationForBar(MiniAcid& engine, int voiceIndex);
MaterialQueueResult queueDrumChaosForBar(MiniAcid& engine);

// Called only from the engine's real 96-PPQN BAR_START boundary. This function
// never generates material and never performs filesystem I/O.
bool commitPendingMaterialAtBarStart(SceneManager& scenes);

bool hasPendingMaterialCommit();
MaterialAction pendingMaterialAction();
MaterialCommitStatus materialCommitStatus();
uint32_t materialCommitSerial();
const char* materialActionLabel(MaterialAction action);

#endif  // GROOVEPUTER_BAR_MATERIAL_COMMIT_H
