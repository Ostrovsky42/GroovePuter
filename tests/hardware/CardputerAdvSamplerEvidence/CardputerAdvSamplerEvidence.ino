#include <Arduino.h>
#include <M5Cardputer.h>
#include <SD.h>
#include <SPI.h>
#include <esp_heap_caps.h>
#include <atomic>
#include <algorithm>

#include "../../../src/platform/cardputer_adv_hardware.h"
#include "../../../src/sampler/ram_sample_store.h"
#include "../../../src/sampler/drum_sampler_track.h"

// Standalone evidence sketch: include the existing sampler implementation directly
// so the test measures the same 0.9.3 source without modifying production firmware.
#include "../../../src/sampler/sample_loader.cpp"
#include "../../../src/sampler/ram_sample_store.cpp"
#include "../../../src/sampler/sampler_voice.cpp"
#include "../../../src/sampler/sampler_pool.cpp"
#include "../../../src/sampler/drum_sampler_track.cpp"

namespace {
constexpr char kFixturePath[] = "/sampler_evidence_ref.wav";
constexpr uint32_t kFixtureFrames = 8192;
constexpr SampleId kFixtureId{0x093A0001u};
constexpr size_t kEvidencePoolBytes = 32u * 1024u;
constexpr uint32_t kBenchmarkBlocks = 8;
static float g_renderBuffer[kBlockFrames];

class EvidenceSampleStore : public RamSampleStore {
 public:
  size_t poolUsageBytes() const { return currentPoolUsage_; }
  size_t poolCapacityBytes() const { return maxPoolBytes_; }

  size_t residentSlots() const {
    size_t count = 0;
    for (const auto& slot : slots_) {
      if (slot.id.load(std::memory_order_acquire) != 0 &&
          slot.ready.load(std::memory_order_acquire)) {
        ++count;
      }
    }
    return count;
  }

  uint32_t totalReferences() const {
    uint32_t count = 0;
    for (const auto& slot : slots_) {
      count += slot.refCount.load(std::memory_order_acquire);
    }
    return count;
  }
};

EvidenceSampleStore g_store;
DrumSamplerTrack g_track;

struct __attribute__((packed)) WavHeader44 {
  char riff[4];
  uint32_t riffSize;
  char wave[4];
  char fmt[4];
  uint32_t fmtSize;
  uint16_t audioFormat;
  uint16_t channels;
  uint32_t sampleRate;
  uint32_t byteRate;
  uint16_t blockAlign;
  uint16_t bitsPerSample;
  char data[4];
  uint32_t dataSize;
};

static_assert(sizeof(WavHeader44) == 44, "evidence WAV header must be 44 bytes");

void printHeap(const char* stage) {
  const size_t freeInt =
      heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t largestInt =
      heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  const size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  const size_t largest8 = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);

  Serial.printf(
      "[SAMPLER-EVIDENCE][HEAP] stage=%s freeInt=%u largestInt=%u free8=%u largest8=%u "
      "poolUsed=%u poolCap=%u resident=%u refs=%u\n",
      stage,
      static_cast<unsigned>(freeInt),
      static_cast<unsigned>(largestInt),
      static_cast<unsigned>(free8),
      static_cast<unsigned>(largest8),
      static_cast<unsigned>(g_store.poolUsageBytes()),
      static_cast<unsigned>(g_store.poolCapacityBytes()),
      static_cast<unsigned>(g_store.residentSlots()),
      static_cast<unsigned>(g_store.totalReferences()));
}

void printSizes() {
  Serial.printf(
      "[SAMPLER-EVIDENCE][SIZE] SampleSlot=%u SamplerVoice=%u SamplerPool=%u "
      "SamplerPad=%u DrumSamplerTrack=%u RamSampleStore=%u\n",
      static_cast<unsigned>(sizeof(SampleSlot)),
      static_cast<unsigned>(sizeof(SamplerVoice)),
      static_cast<unsigned>(sizeof(SamplerPool)),
      static_cast<unsigned>(sizeof(SamplerPad)),
      static_cast<unsigned>(sizeof(DrumSamplerTrack)),
      static_cast<unsigned>(sizeof(RamSampleStore)));
}

bool mountSd() {
  SPI.begin(GroovePuterHardware::kSdClockPin,
            GroovePuterHardware::kSdMisoPin,
            GroovePuterHardware::kSdMosiPin,
            GroovePuterHardware::kSdChipSelectPin);
  const bool began = SD.begin(GroovePuterHardware::kSdChipSelectPin,
                              SPI,
                              GroovePuterHardware::kSdFrequencyHz);
  const bool mounted = began && SD.cardType() != CARD_NONE;
  Serial.printf("[SAMPLER-EVIDENCE][SD] mounted=%d type=%d\n",
                mounted ? 1 : 0, static_cast<int>(SD.cardType()));
  return mounted;
}

bool writeReferenceWav() {
  SD.remove(kFixturePath);
  File file = SD.open(kFixturePath, FILE_WRITE);
  if (!file) return false;

  WavHeader44 header{{'R','I','F','F'},
                     36u + kFixtureFrames * sizeof(int16_t),
                     {'W','A','V','E'},
                     {'f','m','t',' '},
                     16u,
                     1u,
                     1u,
                     kSampleRate,
                     kSampleRate * sizeof(int16_t),
                     sizeof(int16_t),
                     16u,
                     {'d','a','t','a'},
                     kFixtureFrames * sizeof(int16_t)};

  if (file.write(reinterpret_cast<const uint8_t*>(&header), sizeof(header)) !=
      sizeof(header)) {
    file.close();
    return false;
  }

  for (uint32_t frame = 0; frame < kFixtureFrames; ++frame) {
    const int32_t phase = static_cast<int32_t>(frame % 128u);
    const int32_t triangle = phase < 64 ? phase : 127 - phase;
    const int16_t sample = static_cast<int16_t>((triangle - 32) * 700);
    if (file.write(reinterpret_cast<const uint8_t*>(&sample), sizeof(sample)) !=
        sizeof(sample)) {
      file.close();
      return false;
    }
  }
  file.flush();
  const size_t bytes = file.size();
  file.close();
  Serial.printf("[SAMPLER-EVIDENCE][FIXTURE] path=%s frames=%u bytes=%u\n",
                kFixturePath,
                static_cast<unsigned>(kFixtureFrames),
                static_cast<unsigned>(bytes));
  return bytes == sizeof(WavHeader44) + kFixtureFrames * sizeof(int16_t);
}

void configurePads() {
  for (int pad = 0; pad < 8; ++pad) {
    auto& p = g_track.pad(pad);
    p.id = kFixtureId;
    p.volume = 1.0f;
    p.pitch = 1.0f;
    p.startFrame = 0;
    p.endFrame = 0;
    p.chokeGroup = 0;
    p.reverse = false;
    p.loop = false;
  }
}

void drainVoices() {
  for (uint32_t block = 0; block < 24; ++block) {
    std::fill(g_renderBuffer, g_renderBuffer + kBlockFrames, 0.0f);
    g_track.process(g_renderBuffer, kBlockFrames, g_store);
  }
}

void benchmarkVoices(int voiceCount) {
  drainVoices();
  if (g_store.totalReferences() != 0) {
    Serial.printf("[SAMPLER-EVIDENCE][WARN] refs-before-benchmark=%u\n",
                  static_cast<unsigned>(g_store.totalReferences()));
  }

  for (int pad = 0; pad < voiceCount; ++pad) {
    g_track.triggerPad(pad, 1.0f, g_store);
  }

  uint64_t totalUs = 0;
  uint32_t peakUs = 0;
  for (uint32_t block = 0; block < kBenchmarkBlocks; ++block) {
    std::fill(g_renderBuffer, g_renderBuffer + kBlockFrames, 0.0f);
    const uint32_t started = micros();
    g_track.process(g_renderBuffer, kBlockFrames, g_store);
    const uint32_t elapsed = micros() - started;
    totalUs += elapsed;
    if (elapsed > peakUs) peakUs = elapsed;
  }

  const uint32_t avgUs = static_cast<uint32_t>(totalUs / kBenchmarkBlocks);
  const uint32_t blockBudgetUs =
      static_cast<uint32_t>((1000000ULL * kBlockFrames) / kSampleRate);
  const float avgPct = 100.0f * static_cast<float>(avgUs) /
                       static_cast<float>(blockBudgetUs);
  const float peakPct = 100.0f * static_cast<float>(peakUs) /
                        static_cast<float>(blockBudgetUs);

  Serial.printf(
      "[SAMPLER-EVIDENCE][DSP] voices=%d blocks=%u avgUs=%u peakUs=%u "
      "blockBudgetUs=%u avgPct=%.3f peakPct=%.3f refs=%u\n",
      voiceCount,
      static_cast<unsigned>(kBenchmarkBlocks),
      static_cast<unsigned>(avgUs),
      static_cast<unsigned>(peakUs),
      static_cast<unsigned>(blockBudgetUs),
      avgPct,
      peakPct,
      static_cast<unsigned>(g_store.totalReferences()));

  drainVoices();
  Serial.printf("[SAMPLER-EVIDENCE][DSP-END] voices=%d refs=%u\n",
                voiceCount,
                static_cast<unsigned>(g_store.totalReferences()));
}

void runEvidence() {
  g_store.setPoolSize(kEvidencePoolBytes);

  Serial.println("[SAMPLER-EVIDENCE] base=v0.9.2/dd4528050fd3f53ce166490e83ddd6ed763e76fe");
  Serial.printf("[SAMPLER-EVIDENCE] audio sampleRate=%u blockFrames=%u poolPolicy=%u\n",
                static_cast<unsigned>(kSampleRate),
                static_cast<unsigned>(kBlockFrames),
                static_cast<unsigned>(kEvidencePoolBytes));
  printSizes();
  printHeap("BOOT_BASELINE");

  if (!mountSd()) {
    Serial.println("[SAMPLER-EVIDENCE][FAIL] SD unavailable; no preload benchmark");
    printHeap("SD_MISSING");
    return;
  }
  printHeap("AFTER_SD_INIT");

  if (!writeReferenceWav()) {
    Serial.println("[SAMPLER-EVIDENCE][FAIL] reference WAV creation failed");
    return;
  }

  g_store.registerFile(kFixtureId, kFixturePath);
  printHeap("AFTER_REGISTRY");

  const uint32_t preloadStart = micros();
  const bool preloadOk = g_store.preload(kFixtureId);
  const uint32_t preloadUs = micros() - preloadStart;
  Serial.printf(
      "[SAMPLER-EVIDENCE][PRELOAD] ok=%d id=%u latencyUs=%u poolUsed=%u "
      "poolFree=%u resident=%u\n",
      preloadOk ? 1 : 0,
      static_cast<unsigned>(kFixtureId.value),
      static_cast<unsigned>(preloadUs),
      static_cast<unsigned>(g_store.poolUsageBytes()),
      static_cast<unsigned>(g_store.freePoolBytes()),
      static_cast<unsigned>(g_store.residentSlots()));
  printHeap("AFTER_ONE_PRELOAD");
  if (!preloadOk) return;

  configurePads();
  benchmarkVoices(0);
  printHeap("AFTER_0_VOICES");
  benchmarkVoices(1);
  printHeap("AFTER_1_VOICE");
  benchmarkVoices(4);
  printHeap("AFTER_4_VOICES");
  benchmarkVoices(8);
  printHeap("AFTER_8_VOICES");

  Serial.printf("[SAMPLER-EVIDENCE][DONE] refs=%u resident=%u poolUsed=%u\n",
                static_cast<unsigned>(g_store.totalReferences()),
                static_cast<unsigned>(g_store.residentSlots()),
                static_cast<unsigned>(g_store.poolUsageBytes()));
}
}  // namespace

void setup() {
  Serial.begin(115200);
  delay(500);

  auto cfg = M5.config();
  cfg.internal_mic = false;
  cfg.internal_spk = false;
  M5Cardputer.begin(cfg);

  runEvidence();
}

void loop() {
  delay(1000);
}
