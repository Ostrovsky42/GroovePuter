# Tape Workflow (Release-Oriented)

This page documents current Tape behavior in `src/ui/pages/tape_page.cpp` and DSP in `src/dsp/tape_looper.cpp` / `src/dsp/tape_fx.cpp`.

## Goals
- Fast performance actions on a small screen.
- Predictable loop layering without “mix collapse”.
- Safety controls for highs and CPU headroom.

## Core Modes
- `STOP`: looper stopped.
- `REC`: records loop (first pass defines loop length).
- `PLAY`: playback only.
- `DUB`: playback + overdub.

## Smart Workflow (`X`)
- If no loop exists: `X` starts `REC`; next `X` closes into `PLAY`.
- If loop exists: `X` toggles `PLAY <-> DUB`.
- In smart `DUB`, safety is enabled: auto-return to `PLAY` after one loop cycle (`SAFE:DUB1`).

## Performance Actions
- `A` `CAPTURE`: clear loop + arm `REC` + enable FX.
- `S` `THICKEN`: one-cycle safe overdub.
- `D` `WASH`: toggles space/movement macro wash.
- `G` `LOOP MUTE`: toggles looper level mute/unmute.

## Direct Controls
- `Z/C/V`: `STOP` / `DUB` / `PLAY`
- `1/2/3`: speed `0.5x / 1.0x / 2.0x`
- `F`: tape FX on/off
- `Enter`: stutter toggle
- `Space`: clear loop
- `Bksp`/`Del`: eject/reset loop

## Safety / Master
- Master high-cut is fixed in DSP (hardcoded safety LPF), not exposed in UI.
- Current project default: `16kHz` (`kMasterHighCutHz` in `src/dsp/miniacid_engine.h`).
- Recommended code-level presets:
  - `12000 Hz`: strongest protection for compact/bright speakers.
  - `16000 Hz`: balanced default.
  - `18000 Hz`: more top-end air, less protection.

## Mix Recommendations
- Start with `LOOP` level around `35–55%`.
- Use `S` (`THICKEN`) instead of long manual `DUB`.
- Keep `WASH` momentary (toggle on for transition, then off).

## Notes
- DRAM profile uses a practical short looper length (currently 1s) for performance safety.
- Tape FX path includes runtime safety scaling under high CPU load via engine `fxSafetyMix_`.
