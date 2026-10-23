# DSP-A: software candidate, hardware acceptance OPEN

Shared base: `64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f`, the accepted MEMORY-PRUNE-A checkpoint (PR #495). Production source: `0847dc0a4b1ed66ee5f0b497beea8441d73b1cc0`. Neither integration PR #494 nor the memory branch is modified.

## Confirmed repairs

- TB303 Chamberlin profiles LP1/Warm/Retro used an unstable Euler update with feedback gain even at zero resonance. A focused baseline impulse test reproduced a persistent tail (80 Hz, resonance 0: late RMS 0.408636); high cutoff could lock at the internal ±50 clamp. A two-state TPT update removes that defect without adding state or allocation. Diode and Ladder algorithms remain unchanged. The affected profiles intentionally sound different; listening acceptance is still required.
- Active, non-slide TB303 and WAVEMORPH retriggers reset amplitude to zero. They now begin the attack from the current amplitude. Idle attacks, NoteOff, reset and slide contracts remain intact. Standard sequence retrigger jumps: TB303 0.102378 → 0.002537; WAVEMORPH 0.220550 → 0.003732.
- TB303 oscillator/filter selections were absent from its four persisted controls. State now appends those two values within the existing six-slot capacity. Real project Save/Load tests cover both voices, all six engines, track/master levels and legacy four-control TB patches. Old four-control patches retain their defaults; no schema or saved value migration occurs. Internal TB trim remains unchanged and is not a persisted control.

Global gain, factory defaults, sample rate (22050 Hz), voice count and crossfade are unchanged. No HGS, generation, Melody, FORM, Song, MIDI routing or production UI changes.

## Presets

Explicit control-thread recipes are in `src/dsp/sound_quality_presets.h/.cpp`; they do not automatically overwrite project settings. No preset-selector UI is added. Recipes can be auditioned in the rendered WAVs or entered using existing controls.

| TB303 | Oscillator / filter | Cutoff | Resonance | Envelope | Decay ms |
|---|---|---:|---:|---:|---:|
| Classic Acid | Saw / Acid | 700 | .60 | 1400 | 260 |
| Deep Bass | Sub / Soft | 650 | .20 | 500 | 650 |
| Aggressive Acid | Saw / Drive | 1100 | .55 | 1800 | 180 |

| WAVEMORPH | Table | Morph | Sub | Cutoff | Resonance | Decay ms |
|---|---|---:|---:|---:|---:|---:|
| Warm Pad-like Lead | Organ (4) | .15 | .10 | 1400 | .10 | 1800 |
| Digital Bass | Digital (7) | .15 | .65 | 1100 | .20 | 300 |
| Bright Lead | Vowel (5) | .30 | .10 | 5000 | .12 | 750 |

WAVEMORPH remains monophonic with its existing attack; “pad-like” describes timbre/sustain.

## Evidence and limits

The real-voice renderer exercises identical MIDI notes, velocities and schedules on TB303, SH101, WAVEMORPH, AY, SID and SN76489. It includes all seven TB filters × five oscillators, extreme accent cases, all eight wave tables × five morph positions, ramps, releases, repeated notes and six preset sequences: 156 cases total.

- Focused host DSP and ASan/UBSan: PASS; NoteOff/reset/retrigger, filter stability, presets, state and automation.
- Project roundtrip for A+B, six engines and legacy TB state: PASS.
- Existing safe engine-switch tests, including 600 switches under sanitizers: PASS.
- SDL and Cardputer ADV product builds: PASS.
- All candidate renders finite, no clipped samples, zero counted audio-process new/delete operations.
- 40 reference renders for SH101/AY/SID/SN76489 are bit-identical. Twenty held-note TB cases for unchanged filter profiles 1/2/4/6 are bit-identical.
- Independent whole-branch review found no critical/important regression. Production code was committed before final capture to make provenance explicit.

These results establish defect repair and software behavior, not improved musical quality or target timing.

| Measure | Accepted base | DSP candidate | Change |
|---|---:|---:|---:|
| .data bytes | 41996 | 41996 | 0 |
| .bss bytes | 142256 | 142256 | 0 |
| Fixed DRAM bytes | 184252 | 184252 | 0 |
| Flash bytes | 1591658 | 1591862 | +204 |
| Target CPU / underruns | not paired | OPEN | unmeasured |
| Target free heap / largest block | 21184 / 12276, historical quiet capture | OPEN | unmeasured |

The existing 191488-byte DRAM gate is unchanged. Historical heap numbers are not a paired DSP measurement. Host render timings must not be substituted for ESP32 CPU measurements. Product hashes and source SHAs are in [build-evidence.json](build-evidence.json).

## Isolated morph experiment

Opposite-polarity adjacent tables can cancel: the Sine/Triangle midpoint has RMS 0.033075 versus endpoint RMS 0.231104/0.196110 in the probe. `make_phase_experiment.py` generates a separate source copy with phase offsets for two pairs. It improves the gain hole but changes the Morph=1 to next-table Morph=0 phase handoff. This is NOT in production; pairs 10/11 are experimental. A full phase/morph design and listening decision remain OPEN.

## Listening and hardware acceptance

Start with [blind pairs 01–03](audio/blind/README.md), then six presets (04–09). Keep playback volume fixed. Report A/B/no preference and clicks, note endings and tone. Labels are randomized per pair; RMS matching uses bounded linear gain, no limiter. Keep REVEAL.json closed until recording ratings. Unscaled source WAVs remain in audio/baseline and audio/candidate. Baseline and candidate analysis JSON files include spectra, peak, RMS, tails and hashes.

OPEN: blind matched-level acceptance; built-in speaker versus 3.5 mm comparison; paired target CPU, free heap/largest block, zero new underruns; physical NoteOff, engine switching and Save → Reset → Load.

Hardware coordination incident: the diagnostic image produced no captured app telemetry. Our return to the accepted MEMORY-PRUNE-A image was interrupted during upload. Another session then owned the USB port for flash reading. Device execution/current contents are unverified; do not access USB until exclusive availability is confirmed. Restore the accepted recovery image before further target experiments. Recovery BIN SHA256: `b5a6a0e3dc8ea6604a20f7022ff548cbca2209d59bf4797ec8ac3c4fe45841e3`.

## Reproduction

Run from the repository root; outputs remain under ignored build directories.

```sh
bash tools/dsp_a/test.sh
bash tools/dsp_a/project_test.sh
bash tests/run_s1_synth_engine_switch_tests.sh
DSP_A_SOURCE_REV=64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f bash tools/dsp_a/run.sh build/dsp-a/repro-base
bash tools/dsp_a/run.sh build/dsp-a/repro-candidate
bash tools/dsp_a/run.sh build/dsp-a/repro-sanitize --sanitize
python3 tools/dsp_a/analyze.py build/dsp-a/repro-candidate
bash tools/dsp_a/build_target.sh candidate product
make -C platform_sdl -j2
```

`target_benchmark.h` is inserted only into a staged diagnostic build, never the production sketch. Its target measurements have not been captured. No merge or musical-quality acceptance is implied by this report.
