#pragma once
// Copied into a temporary diagnostic sketch only; never part of product boot.
// Included after miniacid_engine.h: reuse its voice declarations to avoid
// Arduino generated-path aliases of legacy pragma-once-only DSP headers.
#include "src/dsp/sound_quality_presets.h"
#include "esp_heap_caps.h"
#include <cmath>

namespace GroovePuterDspA {
inline void targetBenchmark() {
  // Native CDC drops startup diagnostics without DTR; bound the wait so an
  // unattended diagnostic image still proceeds to the normal project UI.
  const uint32_t serialStart = millis();
  while (!Serial && millis() - serialStart < 10000) delay(10);
  constexpr uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  constexpr SynthEngineType types[] = {SynthEngineType::TB303, SynthEngineType::SH101,
      SynthEngineType::WAVEMORPH, SynthEngineType::AY, SynthEngineType::SID, SynthEngineType::SN76489};
  constexpr const char* names[] = {"TB303", "SH101", "WAVEMORPH", "AY", "SID", "SN76489"};
  Serial.println("[DSP-A-BENCH] begin dry temporary voice; real AudioTask remains stopped/background");
  for (int test = 0; test < 12; ++test) {
    const int engine = test < 6 ? test : test < 9 ? 0 : 2;
    const int recipe = test < 6 ? -1 : test < 9 ? test - 6 : test - 9;
    const uint32_t before = heap_caps_get_free_size(caps);
    const uint32_t largestBefore = heap_caps_get_largest_free_block(caps);
    uint32_t resident = 0, totalUs = 0, maximumUs = 0;
    float peak = 0, tailPeak = 0;
    double energy = 0;
    constexpr int blocks = 88;
    float pcm[kBlockFrames];
    {
      SwappableSynthVoice voice(kSampleRate, types[engine]);
      if (recipe >= 0 && engine == 0) SoundQualityPresets::apply(
          *static_cast<TB303Voice*>(voice.activeVoice()), static_cast<SoundQualityPresets::Tb303>(recipe));
      if (recipe >= 0 && engine == 2) SoundQualityPresets::apply(
          *static_cast<WaveMorphSynthVoice*>(voice.activeVoice()), static_cast<SoundQualityPresets::WaveMorph>(recipe));
      resident = heap_caps_get_free_size(caps);
      voice.startNote(110, false, false, 100);
      for (int block = 0; block < blocks; ++block) {
        const uint32_t begin = micros();
        for (int n = 0; n < kBlockFrames; ++n) {
          const int frame = block * kBlockFrames + n;
          if (frame == static_cast<int>(kSampleRate * 3 / 5)) voice.release();
          pcm[n] = voice.process();
        }
        const uint32_t elapsed = micros() - begin;
        totalUs += elapsed;
        if (elapsed > maximumUs) maximumUs = elapsed;
        // Measure DSP only; analysis and double-precision accumulation are not timed.
        for (int n = 0; n < kBlockFrames; ++n) {
          const float magnitude = std::fabs(pcm[n]);
          if (magnitude > peak) peak = magnitude;
          if (block >= blocks - 2 && magnitude > tailPeak) tailPeak = magnitude;
          energy += static_cast<double>(pcm[n]) * pcm[n];
        }
        delay(0); // Keep the control watchdog serviced outside the timed window.
      }
    }
    const uint32_t after = heap_caps_get_free_size(caps);
    const uint32_t largestAfter = heap_caps_get_largest_free_block(caps);
    Serial.printf("[DSP-A-BENCH] engine=%s recipe=%d sr=%u blocks=%d meanUs=%.2f maxUs=%u budgetUs=%u peak=%.8f rms=%.8f tailPeak=%.8f freeBefore=%u resident=%u freeAfter=%u largestBefore=%u largestAfter=%u integrity=%d\n",
        names[engine], recipe, static_cast<unsigned>(kSampleRate), blocks,
        static_cast<double>(totalUs) / blocks, static_cast<unsigned>(maximumUs),
        static_cast<unsigned>(1000000UL * kBlockFrames / kSampleRate), peak,
        std::sqrt(energy / (blocks * kBlockFrames)), tailPeak, before, resident,
        after, largestBefore, largestAfter, heap_caps_check_integrity_all(false));
  }
  Serial.println("[DSP-A-BENCH] complete; normal project UI continues; no project data changed");
}
}  // namespace GroovePuterDspA
