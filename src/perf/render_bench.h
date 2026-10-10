#pragma once
#ifndef GROOVEPUTER_PERF_RENDER_BENCH_H
#define GROOVEPUTER_PERF_RENDER_BENCH_H

#include <cstdint>
#include <cstdio>
#include <memory>
#include <new>
#include <string>

#include "../dsp/mini_drumvoices.h"
#include "../dsp/miniacid_engine.h"
#include "../dsp/swappable_synth_voice.h"

// 0.9.19 render benchmark (diagnostic builds only). Renders the voices of a
// busy bar -- two synths on every sixteenth, a full drum bar, the drum bus --
// two ways with the same events:
//   interleaved: every component per sample, the way MiniAcid renders now;
//   blockwise:   each component through the whole block, then the next.
// The same arithmetic in a different order; on the Cardputer the difference
// is instruction-cache misses (16 KB cache, code in flash). Per-component
// blockwise times show what each part costs. Standalone objects only: the
// engine and the user's scene are not touched.
namespace RenderBench {

constexpr int kFrames = 512;

using ClockUs = uint32_t (*)();
using Emit = void (*)(const char*);

inline std::unique_ptr<DrumSynthVoice> makeDrums(const std::string& name, float sr) {
  if (name == "909") return std::unique_ptr<DrumSynthVoice>(new (std::nothrow) TR909DrumSynthVoice(sr));
  if (name == "606") return std::unique_ptr<DrumSynthVoice>(new (std::nothrow) TR606DrumSynthVoice(sr));
  if (name == "CR78") return std::unique_ptr<DrumSynthVoice>(new (std::nothrow) CR78DrumSynthVoice(sr));
  if (name == "KPR77") return std::unique_ptr<DrumSynthVoice>(new (std::nothrow) KPR77DrumSynthVoice(sr));
  if (name == "SP12") return std::unique_ptr<DrumSynthVoice>(new (std::nothrow) SP12DrumSynthVoice(sr));
  return std::unique_ptr<DrumSynthVoice>(new (std::nothrow) TR808DrumSynthVoice(sr));
}

struct Rig {
  std::unique_ptr<SwappableSynthVoice> a;
  std::unique_ptr<SwappableSynthVoice> b;
  std::unique_ptr<DrumSynthVoice> drums;
  std::unique_ptr<TransientShaper> shaper;
  std::unique_ptr<OneKnobCompressor> comp;
  std::unique_ptr<DrumReverb> reverb;
  std::unique_ptr<TubeDistortion> distA;
  std::unique_ptr<TubeDistortion> distB;
  float dcPrev = 0.0f;
  float dcOut = 0.0f;

  bool build(const std::string& drumName, const std::string& synthB, float sr) {
    a.reset(new (std::nothrow) SwappableSynthVoice(sr, SynthEngineType::TB303));
    b.reset(new (std::nothrow) SwappableSynthVoice(sr, SynthEngineType::TB303));
    drums = makeDrums(drumName, sr);
    shaper.reset(new (std::nothrow) TransientShaper());
    comp.reset(new (std::nothrow) OneKnobCompressor());
    reverb.reset(new (std::nothrow) DrumReverb());
    distA.reset(new (std::nothrow) TubeDistortion());
    distB.reset(new (std::nothrow) TubeDistortion());
    if (!a || !b || !drums || !shaper || !comp || !reverb || !distA || !distB) return false;
    drums->setSampleRate(sr);
    b->setEngineName(synthB);
    // Let a requested engine switch (and its crossfade) finish.
    for (int i = 0; i < 4096; ++i) (void)b->process();
    return true;
  }

  // One sixteenth of the busy bar: synths on every step, kick on beats,
  // snare + clap on 2 and 4, hats on every step, open hat on step 14.
  void step(int s) {
    static const int kLine[16] = {0, 12, 0, 3, 0, 7, 12, 10, 0, 12, 5, 3, 0, 7, 10, 12};
    const float fa = 65.406f * powf(2.0f, kLine[s] / 12.0f);
    a->startNote(fa, s % 8 == 4, s % 4 == 3, 100);
    b->startNote(fa * 2.0f, s % 8 == 0, s % 4 == 1, 100);
    if (s % 4 == 0) drums->triggerKick(s == 0, 100);
    if (s == 4 || s == 12) { drums->triggerSnare(false, 100); drums->triggerClap(false, 100); }
    drums->triggerHat(s % 4 == 2, 100);
    if (s == 14) drums->triggerOpenHat(false, 100);
  }

  float drumVoices() {
    drums->beginSample();
    float d = drums->processKick() + drums->processSnare() + drums->processHat() +
              drums->processOpenHat() + drums->processMidTom() +
              drums->processHighTom() + drums->processRim() + drums->processClap();
    return d * 0.6f;
  }

  float drumBus(float d) {
    dcOut = d - dcPrev + 0.995f * dcOut;
    dcPrev = d;
    d = shaper->process(dcOut);
    d = comp->process(d);
    return reverb->process(d);
  }
};

struct Result {
  uint32_t interleavedUs = 0;
  uint32_t blockwiseUs = 0;
  uint32_t synthAUs = 0;
  uint32_t synthBUs = 0;
  uint32_t drumsUs = 0;
  uint32_t busUs = 0;
  float checksum = 0.0f;  // keeps the work observable
};

// `blocks` blocks of 512 frames each way; a sixteenth lasts 5 blocks.
inline Result run(Rig& rig, int blocks, ClockUs now, void (*yieldEveryBlock)()) {
  Result r;
  float bufA[kFrames];
  float bufB[kFrames];
  float bufD[kFrames];
  uint64_t inter = 0, block = 0, sa = 0, sb = 0, sd = 0, sbus = 0;
  for (int pass = 0; pass < 2; ++pass) {
    for (int k = 0; k < blocks; ++k) {
      if (k % 5 == 0) rig.step((k / 5) % 16);
      if (pass == 0) {
        const uint32_t t0 = now();
        for (int i = 0; i < kFrames; ++i) {
          const float va = rig.distA->process(rig.a->process() * 0.5f);
          const float vb = rig.distB->process(rig.b->process() * 0.5f);
          const float d = rig.drumBus(rig.drumVoices());
          r.checksum += va + vb + d;
        }
        inter += now() - t0;
      } else {
        const uint32_t t0 = now();
        for (int i = 0; i < kFrames; ++i) bufA[i] = rig.distA->process(rig.a->process() * 0.5f);
        const uint32_t t1 = now();
        for (int i = 0; i < kFrames; ++i) bufB[i] = rig.distB->process(rig.b->process() * 0.5f);
        const uint32_t t2 = now();
        for (int i = 0; i < kFrames; ++i) bufD[i] = rig.drumVoices();
        const uint32_t t3 = now();
        for (int i = 0; i < kFrames; ++i) bufD[i] = rig.drumBus(bufD[i]);
        const uint32_t t4 = now();
        for (int i = 0; i < kFrames; ++i) r.checksum += bufA[i] + bufB[i] + bufD[i];
        block += t4 - t0;
        sa += t1 - t0;
        sb += t2 - t1;
        sd += t3 - t2;
        sbus += t4 - t3;
      }
      if (yieldEveryBlock) yieldEveryBlock();
    }
  }
  r.interleavedUs = static_cast<uint32_t>(inter / blocks);
  r.blockwiseUs = static_cast<uint32_t>(block / blocks);
  r.synthAUs = static_cast<uint32_t>(sa / blocks);
  r.synthBUs = static_cast<uint32_t>(sb / blocks);
  r.drumsUs = static_cast<uint32_t>(sd / blocks);
  r.busUs = static_cast<uint32_t>(sbus / blocks);
  return r;
}

// Every drum engine with two TB303, then every synth B engine with 808.
inline void runAll(float sampleRate, int blocks, ClockUs now, void (*yieldEveryBlock)(), Emit emit) {
  struct Config { const char* drums; const char* synthB; };
  static const Config kConfigs[] = {
      {"808", "TB303"}, {"909", "TB303"}, {"606", "TB303"}, {"CR78", "TB303"},
      {"KPR77", "TB303"}, {"SP12", "TB303"}, {"808", "SID"}, {"808", "AY"},
      {"808", "SH101"}, {"808", "SN76489"}, {"808", "WAVEMORPH"}};
  char line[200];
  for (const auto& c : kConfigs) {
    Rig rig;
    if (!rig.build(c.drums, c.synthB, sampleRate)) {
      std::snprintf(line, sizeof(line), "[BENCH] drums=%s synthB=%s ALLOC FAILED", c.drums, c.synthB);
      emit(line);
      continue;
    }
    const Result r = run(rig, blocks, now, yieldEveryBlock);
    std::snprintf(line, sizeof(line),
                  "[BENCH] drums=%s synthB=%s interleaved=%u blockwise=%u us/block | "
                  "synthA=%u synthB=%u drums=%u bus=%u (blockwise) chk=%d",
                  c.drums, c.synthB, (unsigned)r.interleavedUs, (unsigned)r.blockwiseUs,
                  (unsigned)r.synthAUs, (unsigned)r.synthBUs, (unsigned)r.drumsUs,
                  (unsigned)r.busUs, static_cast<int>(r.checksum) & 0xFF);
    emit(line);
  }
}

}  // namespace RenderBench

#endif  // GROOVEPUTER_PERF_RENDER_BENCH_H
