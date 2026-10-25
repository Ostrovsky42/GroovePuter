# G6 + DSP-A + DUAL OSC preset integration

Local experimental branch: `integration/20261025-g6-dsp-ab-presets`.
Production commit: `385585693470236213906f1726932aff4fd6ed0f`. Merge commit: `d29060cd`.

The branch combines the G6 persistence repairs (`0cfb94c3`), the user-tested New/Clear descriptor reset (`317065b5`), DSP-A TB303/WAVEMORPH repairs/presets, and the complete existing DUAL OSC adapter/preset branch ending at `ba5b095f`. Shared base is memory checkpoint `64ae4b8b`. History is preserved with a merge commit. Original checkouts and their uncommitted work were retained. This is the user-requested experimental integration of existing work; it is not an autonomous release acceptance or permission to expand the frozen feature scope.

## Confirmed defects fixed during integration

1. The supplied popup commit still passed a coarse step of five for direct `a/z`. The real UI regression recorded `after_a=5 expected=1`. Only those two keys on the DUAL OSC Preset knob now step by one. Other knobs and TB303/WAVEMORPH retain their previous adjustment logic.
2. A DUAL OSC project could be written but was refused on Load because the Scene validator accepted only the six original engine names. The real-engine Save/Load regression failed after the keyboard fix. The validator now admits `DUAL OSC` only when the existing experimental flag is compiled in. No project format or schema changed, and the default build still validates the original six-engine set.

The final UI test covers both Synth A and B: all 13 popup entries, preview without applying, cancel, MAIN Enter, MORE PRESET Enter, 13-way wrapping, single-step `a/z` and arrows, and Save/Load of the selected preset and all six patch parameters. The G6 suite also retains New/Clear and full Melody/storage refusal regressions. The default six-engine DSP-A project test covers old parameter and level restoration; DSP-A release and ASan/UBSan tests pass.

## Memory and artifact

| Image | Flash usage | .data | .bss | Static DRAM |
| --- | ---: | ---: | ---: | ---: |
| Prior user-tested New/Clear image `22679b20…` | 1,609,742 B | 41,996 B | 142,256 B | 184,252 B |
| Combined experimental image `7b9a98a9…` | 1,622,326 B | 41,996 B | 142,256 B | 184,252 B |
| Difference | +12,584 B | 0 B | 0 B | 0 B |

DRAM limit remains 191,488 B. The candidate BIN is 1,622,480 B; SHA-256 `7b9a98a9123cef335e1014ab9e131335819fb68a61aaefca86ba4dfdd869d61b`. Path: `build/integration-dual-osc-adv/GroovePuter.ino.bin`. Hardware heap, largest block, stack, CPU and underruns for this combined image are OPEN. No prior hardware result is transferred between these BINs.

Build with the experimental adapter enabled:

```sh
GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS='-DGROOVEPUTER_FELUCCA_ANALOG=1' \
BUILD_PATH="$PWD/build/integration-dual-osc-adv" \
bash scripts/build_cardputer_dynbuffers.sh --jobs 2
```

The flag is required to expose DUAL OSC. The default firmware configuration was not changed. The C shim is built as C; the adapter remains at 22,050 Hz.

## Verification

| Check | Result | Evidence |
| --- | --- | --- |
| Full G6 host storage suite, including New/Clear | PASS | `g6-final.log.gz` |
| DSP-A release + ASan/UBSan | PASS | `dsp-quality.log.gz` |
| Original six-engine patch/level Save/Load and preset UI | PASS | `dsp-project.log.gz` |
| DUAL OSC both-voice popup/keys/patch Save/Load | PASS | `ui-final.log.gz` |
| SDL build with DUAL OSC and Cardputer prune flags | PASS | `sdl-final.log.gz` |
| Cardputer ADV experimental build and DRAM gate | PASS | `cardputer-final.log.gz` |
| Combined-image physical acceptance | OPEN | Uploaded/hash verified; app boot not yet confirmed |

The first custom pruned SDL invocation omitted `sample_scene_persistence.cpp` and failed to link `clearSceneSampleReferences`. The final invocation includes the existing module; no product code was changed for that invocation error. The reproducible final SDL build command is `bash docs/integration/g6-dsp-ab-presets/build-sdl.sh`.

## BLOCKERS FOR HARDWARE

No software blocker found within the preset integration scope. Physical qualification remains OPEN: popup navigation/labels on Cardputer, both-voice sound and NoteOff, engine switching, accepted Melody A+B 8B Save → Reset → Load, and runtime memory/underruns. The three previously reported cold cycles apply to the prior New/Clear image, not this candidate. Existing SD-failure and release gates are still OPEN; G5 REAPER remains DEFERRED.

## SDL UX FINDINGS

Scoped preset UI smoke passed on a real X display (`DISPLAY=:1`) with targeted keyboard input and a 240×135 logical window at 2× scale. Screenshots show the MAIN Preset knob, the first five and last five entries of the 13-item list, and the MORE page. Title, selection and row labels are readable. MORE modal entry and both voice bindings are established by the real UI regression; the MORE screenshot itself shows the page rather than its open popup. Wider Pattern/Phrase acceptance was outside this integration smoke.

Images: `sdl-boot.png`, `sdl-dual-main.png`, `sdl-popup-first.png`, `sdl-popup-last.png`, `sdl-more.png`. Runtime console: `sdl-runtime.log.gz`.

## NOT OBSERVABLE IN SDL

Dummy audio was used, so neither listening quality nor physical audio, Cardputer keys, USB MIDI, SD failure behavior, heap/largest block, stack or device underruns was certified. The Felucca DSP implementation did not receive a new sanitizer qualification in this integration; the sanitizer PASS above is specifically DSP-A.

SDL SMOKE: GREEN — proceed to physical hardware (preset integration scope only).

## Remaining known boundaries

The reported hardware Save refusal from the preceding diagnostic session still lacks an exact failure-stage trace; the New/Clear defect was reproduced and repaired separately. Current recovery-scanner directory absence claims remain limited by its already documented filename-prefix filter bug; this merge does not repair or restore old absent payloads. Existing unaccepted working material still requires KEEP/ACCEPT before project saving, as in the current product contract.

The user subsequently requested flashing: the exact combined BIN was uploaded successfully and esptool verified app data hash. The first 60-second serial monitor connected to CDC but captured no application output; boot and functional acceptance remain OPEN. See `flash-record.json`. No push or release-branch merge.
