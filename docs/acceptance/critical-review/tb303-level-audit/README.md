# TB303 quiet Acid investigation — 2026-10-26

Status: investigation, no DSP/gain/default change. The user clarified that the Acid and Moog filters are quiet and sound normal only with DST enabled. Actual Cutoff/Res settings and speaker versus jack/headphone output were requested; no matching hardware patch was captured yet.

## Fresh deterministic comparison

Accepted pre-DSP-A memory checkpoint `64ae4b8b` versus current production `9e6c3ddf` at22,050 Hz, same renderer, explicit preset recipes, events/velocity and control values. Both render commands exited0;156 cases each. 156/156 parameter dumps match exactly. `comparison.csv` records every case. No clipped candidate samples.

- Classic Acid sequence: RMS 0.032025091 → 0.032041041, +0.0043 dB. No meaningful level loss in this test.
- All20 held-note Diode/Ladder cases (Acid/Moog/Soft/Drive, five oscillators) are float-bit-identical. The Acid filter itself was not changed by DSP-A.
- All40 SH101/AY/SID/SN76489 reference cases remain float-bit-identical.
- Changed TPT profiles have input-dependent differences; the extreme accented LP1 case falls about4.69 dB after removal of the previously unstable behavior. This does not establish that the user's quiet sound was LP1 or that the output needs compensation.

WAV references for Classic Acid, a standard sequence and an Acid-filter held note are included before/current. These are unnormalized mono dry references, not speaker/jack recordings or blind listening results.

## Separate engine-balance observation

The reproducible `level_probe.cpp` exercises TB defaults/three presets and all13 DUAL OSC presets at MIDI36/48/60, velocity100. Full results are `relative-levels.csv`. For MIDI48 (C3), active window0.15–0.45s, TB303 Classic Acid is -27.513 dBFS RMS; DUAL OSC ACID is -12.953 dBFS, a 14.560 dB difference. This confirms a substantial relative-level difference in one controlled case. It does not prove that the user's hardware sound changed at the DSP-A commit or justify automatic gain normalization across musical presets.

## Reproduction

```sh
DSP_A_SOURCE_REV=64ae4b8bcf13fc165a04b2b79a80b7e8b3a1f71f bash tools/dsp_a/run.sh build/tb303-level-audit/baseline
bash tools/dsp_a/run.sh build/tb303-level-audit/current
gcc -std=c11 -O2 -I. -c src/dsp/felucca_analog_c.c -o /tmp/gp-level-felucca.o
g++ -std=c++17 -O2 -DGROOVEPUTER_FELUCCA_ANALOG=1 -I. docs/acceptance/critical-review/tb303-level-audit/level_probe.cpp src/dsp/mini_tb303.cpp src/dsp/filter.cpp src/dsp/audio_wavetables.cpp src/dsp/felucca_analog_voice.cpp src/dsp/sound_quality_presets.cpp src/dsp/wave_morph_synth_voice.cpp /tmp/gp-level-felucca.o -o /tmp/gp-level-probe
/tmp/gp-level-probe
```

## Required next observation

Capture the actual quiet project/engine/oscillator/filter/cutoff/resonance/envelope/decay, velocity/accent, track/master levels, FX and output route. Compare with an explicit known Classic Acid preset at the same fixed note, first isolated and then in the mix, through jack/speaker. Save/Load should preserve those values; host roundtrip tests already pass but do not substitute for this device observation.

G4 remains OPEN. No speculative patch was applied and no project/SD data was touched by this host-only analysis.

## Acid/Moog clarification: filter and full-mixer measurements

The user clarified this concerns Acid and Moog filters, with DST required for normal perceived level. A new840-case voice probe covers all7 filters, MIDI36/48/60, cutoff200/400/800/1600/4000 Hz, resonance0/.3/.6/.9, envelope0/400. Each case renders dry and default DST drive8 from the identical voice signal, including the mixer's0.5 per-synth factor. Results/source are `filter-levels.csv` / `filter_levels.cpp`.

A second36-case real-MiniAcid probe includes the output chain, master0.6, track1.0, Acid mode, cutoff800, envelope400, no drums/other voice/delay, resonance0/.6, and DST off/on. Source/results are `mix_levels.cpp` / `mix-levels.csv`; this is a host test of real engine state, not hardware listening or timing. With resonance0.6:

| Note | Filter | DST OFF dBFS RMS | DST ON dBFS RMS | Difference |
| --- | --- | ---: | ---: | ---: |
| C2 / MIDI36 | LP1 | -36.362 | -26.217 | +10.145 dB |
| C2 / MIDI36 | Acid | -41.958 | -29.896 | +12.062 dB |
| C2 / MIDI36 | Moog | -38.082 | -27.476 | +10.606 dB |
| C3 / MIDI48 | LP1 | -36.404 | -26.377 | +10.027 dB |
| C3 / MIDI48 | Acid | -43.064 | -30.781 | +12.283 dB |
| C3 / MIDI48 | Moog | -37.572 | -26.790 | +10.782 dB |
| C4 / MIDI60 | LP1 | -35.535 | -25.656 | +9.879 dB |
| C4 / MIDI60 | Acid | -44.151 | -31.566 | +12.585 dB |
| C4 / MIDI60 | Moog | -39.170 | -27.828 | +11.342 dB |

This establishes substantial DST gain and a real filter-level difference under these settings. It does not identify the user's exact case, establish that the unchanged Diode/Ladder algorithms regressed at DSP-A, or select a safe compensation value. Cutoff/resonance, pitch and speaker bass response still matter. No DSP/gain or default was modified. Existing parameter values, output routing and the installed firmware are unchanged.
