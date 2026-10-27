# Integrated runtime protocol (DSP-B R3)

Same protocol and same instrumentation for every image, so only the synth engine differs.

## Images
| Name | Source | Flags | Notes |
|---|---|---|---|
| BASE | PR #495, SHA 64ae4b8b (`miniacid-prod495`) | `-DGROOVEPUTER_BLOCK_HIST=1` | product + block histogram only |
| EXP  | branch `experiment/dsp-b-analog-integrated` (`miniacid-analog-int`) | `-DGROOVEPUTER_BLOCK_HIST=1 -DGROOVEPUTER_FELUCCA_ANALOG=1` | adds ANALOG to the engine list |

Without the ANALOG flag, EXP is byte-identical to BASE except 76 bytes (the build timestamp and image digest).

## Capture
Run in a terminal before each scenario, and keep it open:
```sh
cat /dev/ttyACM0 | tee logs/<image>-<scenario>.txt      # or: arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```
Lines used: `[PERF]` (audio %, peak %, underruns, ui us, freeInt, largest), `[BLOCK]` (p50/p95/p99/max per 5 s, budget
23 ms for 512 frames), `[USB-DIAG]`, `[MIDI-DISPATCH]`. Stack high-water: `grep -i stack`.

## Scenarios (each 60 s of playback unless stated)
1. **One voice**: Synth A = ENGINE, a 16-step bass line, no drums.
2. **Two voices**: Synth A and Synth B both on ENGINE, different lines.
3. **Two voices + drums**: as 2, plus a drum pattern.
4. **Full FX**: as 3, all FX on (distortion, delay, reverb as in the product's default FX chain).
5. **Long melody**: play a melody through its full length (not the 8-bar loop).
6. **Generation during playback**: start playback, then generate a new pattern from the menu while it plays.
7. **Engine switching**: while playing, switch Synth A ENGINE -> SH101 -> ANALOG -> WAVEMORPH -> TB303, 5 s each; then stop.
8. **Note cleanup**: stop playback during held notes; no hanging notes (check the MIDI-DISPATCH counters and by ear).

ENGINE: BASE uses SH101 and WAVEMORPH (and TB303 for reference); EXP uses ANALOG, SH101, WAVEMORPH.

## Recorded per run
CPU audio % (avg from `[PERF]`, and peak), `[BLOCK]` p50/p95/p99/max, underruns (must stay 0), freeInt and largest
(heap), stack high-water, and the static DRAM from the build's "Global variables" line.

## What cannot be measured here
Subjective quality (listening), and any scenario that needs a key press or a screen check. Those are marked for the
user in the report. Underruns counted by the firmware are the I2S write failures; the block histogram shows how
close blocks come to the budget.
