# SH101 and SN76489 synth engines

## Purpose

Add two lightweight monophonic engines for Synth A and Synth B without changing pattern, MIDI, transport, or scene-file structure:

- `SH101`: an SH-101 / MC-202-style subtractive voice with saw/pulse mix, sub oscillator, optional noise, resonant low-pass filtering, envelope decay, accent, and slide;
- `SN76489`: a Sega/TI PSG-style voice with three divider-quantized square channels, selectable stacks, 4-bit amplitude, white/periodic LFSR noise, accent, and slide.

These are musical style models, not circuit-accurate emulations.

The previous `OPL2` engine is removed because it exceeds the stable real-time budget on Cardputer ADV and can cause audio/UI stalls. Its persisted numeric slot is retained only for compatibility: loading an older scene that selected OPL2 safely substitutes `TB303`.

## Hardware

- M5Stack Cardputer ADV;
- built-in speaker or 3.5 mm audio output;
- USB-C cable for build and flash;
- optional headphones or powered monitor for noise-floor comparison.

No external GPIO or I2C wiring is required.

## Wiring

```text
Cardputer ADV USB-C -> development computer
Cardputer ADV 3.5 mm -> headphones / powered monitor (optional)
```

Keep the output device at a moderate level during first playback. Both engines can produce bright square-wave material.

## Build and flash

From the repository root:

```bash
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
```

Flash the generated Cardputer ADV firmware using the repository's normal upload command or Arduino CLI workflow.

Desktop validation:

```bash
cd platform_sdl
make clean all CXX=g++
```

Focused host test:

```bash
mkdir -p build/host-tests
g++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter -I. \
  tests/test_new_synth_voices.cpp \
  src/dsp/swappable_synth_voice.cpp \
  src/dsp/mini_tb303.cpp \
  src/dsp/filter.cpp \
  src/dsp/audio_wavetables.cpp \
  src/dsp/sid_synth_voice.cpp \
  src/dsp/sid_synth.cpp \
  src/dsp/ay_synth_voice.cpp \
  src/dsp/sh101_synth_voice.cpp \
  src/dsp/sn76489_synth_voice.cpp \
  -o build/host-tests/test_new_synth_voices
build/host-tests/test_new_synth_voices
```

`-Wno-unused-parameter` is scoped to this focused link because the pre-existing SID adapter intentionally ignores several interface arguments.

## Controls

### Synth settings list

Open Synth A or Synth B and press `Tab`:

- select `Engine` with Up/Down;
- change it with Left/Right;
- edit any of the six engine parameters below it.

The selectable engine order is:

```text
TB303 -> SID -> AY -> SH101 -> SN76489
```

`OPL2` must not appear.

The settings page uses the existing content rectangle. The global status chrome remains in the top 16-pixel header and no longer covers the engine selector or local title.

### Knob page

Open the Synth A or Synth B parameter page:

- `[` / `]`: previous / next engine;
- Left/Right: move focus;
- Up/Down: adjust the focused control;
- Ctrl or Shift: fine adjustment;
- `A/Z`, `S/X`, `D/C`, `F/V`: adjust knobs 1-4;
- `T/G`: adjust parameter 5;
- `Y/H`: adjust parameter 6;
- `N`: distortion toggle;
- `M`: delay toggle.

For TB303, `T/G` and `Y/H` retain their oscillator and filter-type behavior.

## Expected behavior

### SH101

- `Wave` changes between Saw, Pulse, and Mix;
- `Sub` adds a square sub-octave;
- `Noise=0` produces no intentional oscillator noise;
- cutoff and resonance produce a subtractive mono-synth response;
- slide changes pitch without restarting an already active envelope;
- a first pattern step marked slide still starts audibly.

### SN76489

- `Stack` changes the three tone-channel pitch relationship;
- tone frequency follows the SN76489 divider grid;
- `Noise=0` disables intentional PSG noise;
- `NMode` selects white/periodic noise and its clock source;
- output level follows a 4-bit-style envelope;
- a first pattern step marked slide still starts audibly.

Both voices must remain finite, bounded to `[-1, 1]`, and decay to digital silence after release.

### Removed OPL2 scenes

When a scene or older project contains OPL2:

- firmware must not instantiate the removed OPL2 DSP voice;
- the track must load as `TB303`;
- playback and UI must remain responsive;
- saving the project again stores the active TB303 engine name.

## Noise and hiss diagnosis

Use this order to distinguish DSP noise from the analog output floor:

1. Set SH101 `Noise=0` or SN76489 `Noise=0`.
2. Disable Tape, Lo-Fi, distortion, delay feedback, drum reverb, and the voice track.
3. Stop transport and mute both synth tracks.
4. Compare the built-in speaker with the 3.5 mm output.
5. Record a WAV internally and compare it with the physical output.

Interpretation:

- hiss present in the WAV: generated in DSP or an enabled effect;
- clean WAV but noisy speaker/headphone output: codec, power amplifier, grounding, or analog gain staging;
- noise only on high notes or bright waveforms: likely aliasing rather than a constant noise floor;
- hiss only while `Noise`, Tape, Lo-Fi, reverb, or distortion is active: intentional or effect-generated noise.

Do not add a master noise gate until this split is measured. A gate can hide the floor between notes but will damage quiet releases and delay/reverb tails.

## Troubleshooting

### New engines are not visible

Open the knob page and use `[`/`]`, or open `Tab -> SETTINGS` and change the `Engine` row. Reflash the exact PR artifact if the list does not contain SH101 and SN76489.

### OPL2 still appears

The wrong or an older firmware image is running. In this build, both selectors skip OPL2 and the OPL2 source files are absent.

### An old OPL2 project opens as TB303

This is intentional compatibility behavior. OPL2 is not executed; the persisted legacy slot is normalized to TB303 to prevent the Cardputer ADV stalls observed during hardware testing.

### Header covers the engine selector

Confirm the build contains the synth-settings content-layout fix. The selector must begin below the global 16-pixel status header.

### First slide step is silent

Run `tests/test_new_synth_voices.cpp`. Both engines explicitly treat slide as legato only when a voice is already active.

### Output is harsh or hissy

Set the engine noise parameter to zero and repeat the diagnosis above. PolyBLEP reduces oscillator-edge aliasing, but the project-wide 22.05 kHz sample rate still limits the clean high-frequency range.

### Audio underruns increase

Disable Tape and reverb, then compare `[PERF]` diagnostics with TB303, SH101, and SN76489 under the same pattern. Report the exact engine, both Synth A/B assignments, and peak audio CPU.

## Acceptance checklist

- [ ] `SH101` and `SN76489` appear for both Synth A and Synth B.
- [ ] `OPL2` is absent from `Tab -> SETTINGS` and from the `[`/`]` cycle.
- [ ] An older OPL2 scene loads as TB303 without hanging.
- [ ] `[`/`]` changes engine on the knob page without a click, crash, or stuck note.
- [ ] `Tab -> SETTINGS` keeps all text below the status chrome.
- [ ] The knob page shows parameters 1-4 on knobs and parameters 5-6 in the lower row.
- [ ] TB303 oscillator/filter shortcuts still behave exactly as before.
- [ ] SH101 produces saw, pulse, sub, filter, accent, and slide changes.
- [ ] SN76489 produces three-tone stacks and selectable white/periodic noise.
- [ ] A first step marked slide sounds on both engines.
- [ ] Stop/release leaves no persistent digital output or stuck note.
- [ ] Host voice test, SDL build, and Cardputer ADV build pass.
- [ ] No sustained increase in audio underruns when both synth tracks use the new engines.
- [ ] With engine noise and effects disabled, compare internal WAV, speaker, and 3.5 mm noise floors.
