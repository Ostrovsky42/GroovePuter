#!/usr/bin/env python3
"""One-time guarded migration wiring AudioMutationGate into Cardputer runtime."""

from pathlib import Path


PATH = Path("miniacid.ino")


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 occurrence, found {count}")
    return text.replace(old, new, 1)


def main() -> None:
    text = PATH.read_text(encoding="utf-8")

    text = replace_once(
        text,
        '#include "src/audio/audio_diagnostics.h"\n',
        '#include "src/audio/audio_diagnostics.h"\n'
        '#include "src/audio/audio_mutation_gate.h"\n'
        '#include "src/platform/cardputer_adv_hardware.h"\n',
        "runtime includes",
    )

    text = replace_once(
        text,
        "TaskHandle_t g_audioTaskHandle = nullptr;\n",
        "TaskHandle_t g_audioTaskHandle = nullptr;\n"
        "static AudioMutationGate g_audioMutationGate;\n",
        "gate global",
    )

    text = replace_once(
        text,
        """  while (true) {
    uint32_t now = micros();
""",
        """  while (true) {
    g_audioMutationGate.waitAtAudioBoundary();
    uint32_t now = micros();
""",
        "audio boundary",
    )

    text = replace_once(
        text,
        """  // ENABLE hardware amplifier (PA_EN = G21)
  pinMode(21, OUTPUT); digitalWrite(21, HIGH);
""",
        """  // Enable the Cardputer ADV power amplifier. This pin is not RGB data.
  pinMode(GroovePuterHardware::kPowerAmplifierEnablePin, OUTPUT);
  digitalWrite(GroovePuterHardware::kPowerAmplifierEnablePin, HIGH);
""",
        "amplifier pin",
    )

    text = replace_once(
        text,
        """  // Set audio guard to protect audio task from concurrent access
  // Configure Audio Guard for synchronization
  AudioGuard guard;
  guard.lock = [](void*) {
      // In a real dual-core ESP32 setup, we could use a mutex here
      // For now, grooveputer uses a single-threaded DSP model with volatile flags
  };
  guard.unlock = [](void*) {};
  g_miniDisplay->setAudioGuard(guard);
""",
        """  // Pause the renderer only at a block boundary while existing UI mutation
  // lambdas update engine state. No mutex is held while DSP is rendering.
  AudioGuard guard;
  guard.context = &g_audioMutationGate;
  guard.lock = [](void* context) {
      static_cast<AudioMutationGate*>(context)->lockControl();
  };
  guard.unlock = [](void* context) {
      static_cast<AudioMutationGate*>(context)->unlockControl();
  };
  g_miniDisplay->setAudioGuard(guard);
""",
        "AudioGuard wiring",
    )

    text = replace_once(
        text,
        """  } else {
    Serial.printf("[DEBUG] AudioTask created successful, handle: %p\\n", (void*)g_audioTaskHandle);
  }
  markBootStage(81, "after AudioTask create");
""",
        """  } else {
    Serial.printf("[DEBUG] AudioTask created successful, handle: %p\\n", (void*)g_audioTaskHandle);
    g_audioMutationGate.setAudioTaskActive(true);
  }
  markBootStage(81, "after AudioTask create");
""",
        "activate gate",
    )

    text = replace_once(
        text,
        """  if (M5Cardputer.BtnA.wasClicked()) {
    if (g_miniAcid->isPlaying()) {
      g_miniAcid->stop();
    } else {
      g_miniAcid->start();
    }
    drawUI();
  }
""",
        """  if (M5Cardputer.BtnA.wasClicked()) {
    {
      AudioMutationScope mutationScope(g_audioMutationGate);
      if (g_miniAcid->isPlaying()) {
        g_miniAcid->stop();
      } else {
        g_miniAcid->start();
      }
    }
    drawUI();
  }
""",
        "button transport guard",
    )

    text = replace_once(
        text,
        "  auto handleWithFallback = [&](UIEvent evt) {\n",
        "  auto handleWithFallback = [&](UIEvent evt) {\n"
        "    AudioMutationScope mutationScope(g_audioMutationGate);\n",
        "keyboard event guard",
    )

    PATH.write_text(text, encoding="utf-8")


if __name__ == "__main__":
    main()
