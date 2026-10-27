#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Diagnostic instrumentation for the integrated comparison, applied identically to the baseline (PR 495) and the
# experimental tree: a per-block render-time histogram of the audio task (32 us bins up to 2 ms; kBlockFrames = 512 is a 23 ms budget), printed with the existing
# [PERF] line every 5 s. Compiled only with -DGROOVEPUTER_BLOCK_HIST=1; with the flag off the code is absent.
#   apply_block_hist.py <tree>
import sys

p = sys.argv[1].rstrip('/') + '/GroovePuter.ino'
s = open(p).read()

HELPERS = '''#if defined(GROOVEPUTER_BLOCK_HIST)
// Per-block render time of generateAudioBuffer: 64 bins of 8 us (last = overflow). Written by the audio task and read
// by the 5 s report; the report reads without a lock (diagnostic only; a torn read shifts one count at most).
static uint32_t g_blockHist[64];
static uint32_t g_blockHistMaxUs = 0;
static void blockHistAdd(uint32_t us) {
  uint32_t bin = us / 32u;
  g_blockHist[bin < 63u ? bin : 63u]++;
  if (us > g_blockHistMaxUs) g_blockHistMaxUs = us;
}
static uint32_t blockHistPercentileUs(uint32_t permille, uint32_t total) {
  if (total == 0) return 0;
  uint32_t target = (total * permille + 999u) / 1000u;
  uint32_t acc = 0;
  for (uint32_t i = 0; i < 64u; ++i) {
    acc += g_blockHist[i];
    if (acc >= target) return i * 32u + 32u;
  }
  return g_blockHistMaxUs;
}
#endif

void audioTask(void *param) {'''
assert s.count('void audioTask(void *param) {') == 1
s = s.replace('void audioTask(void *param) {', HELPERS, 1)

GEN = '      g_miniAcid->generateAudioBuffer(g_audioBuffer, kBlockFrames);\n'
assert s.count(GEN) == 1, 'generate call'
s = s.replace(GEN, '''#if defined(GROOVEPUTER_BLOCK_HIST)
      const uint32_t blockT0 = micros();
#endif
''' + GEN + '''#if defined(GROOVEPUTER_BLOCK_HIST)
      blockHistAdd(micros() - blockT0);
#endif
''', 1)

PERF = '''       g_peakUiDrawUs = g_lastUiDrawUs;
    }
  }
'''
assert s.count(PERF) == 1, 'perf tail'
s = s.replace(PERF, '''#if defined(GROOVEPUTER_BLOCK_HIST)
       {
         uint32_t total = 0;
         for (uint32_t i = 0; i < 64u; ++i) total += g_blockHist[i];
         Serial.printf("[BLOCK] n=%u budget=%uus p50=%uus p95=%uus p99=%uus max=%uus\\n",
             (unsigned)total, (unsigned)(kBlockFrames * 1000000u / kSampleRate),
             (unsigned)blockHistPercentileUs(500, total), (unsigned)blockHistPercentileUs(950, total),
             (unsigned)blockHistPercentileUs(990, total), (unsigned)g_blockHistMaxUs);
         for (uint32_t i = 0; i < 64u; ++i) g_blockHist[i] = 0;
         g_blockHistMaxUs = 0;
       }
#endif
       g_peakUiDrawUs = g_lastUiDrawUs;
    }
  }
''', 1)
open(p, 'w').write(s)
print('instrumented', p)
