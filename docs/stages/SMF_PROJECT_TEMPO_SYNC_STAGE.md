# SMF PROJECT TEMPO SYNC STAGE

## Purpose

Add a second realtime SMF timing mode for playing recognizable MIDI material together with GroovePuter and Yamaha SEQTRAK.

`ORIGINAL` keeps the existing faithful SMF tempo-map player.

`PROJECT` keeps the MIDI file's tick positions and note lengths but converts them onto GroovePuter's existing musical transport. GroovePuter remains the clock master and SEQTRAK receives the already accepted MIDI Start / Clock / Stop stream from the same audio-block timeline.

The intended workflow is:

```text
GroovePuter project tempo
        |
        +--> 24 PPQN USB MIDI Clock --> SEQTRAK
        |
        +--> SMF PROJECT timeline
                 |
                 +--> quantized NEXT BAR notes --> SEQTRAK
```

This stage is deliberately smaller than the future Phrase Engine. It adds the synchronized timing foundation only; track isolation, 1/2/4/8-bar phrase slicing and phrase slots belong in later PRs.

## Hardware list

- M5Stack Cardputer-Adv (ESP32-S3, no PSRAM assumed)
- Yamaha SEQTRAK
- data-capable USB-C connection between Cardputer-Adv and SEQTRAK
- microSD card containing one or more `.mid` files under `/midi`

No PORT.A I2C accessory is required for this test.

If PORT.A is attached for unrelated testing, Cardputer-Adv uses:

```text
SDA GPIO2
SCL GPIO1
```

## Wiring

```text
Cardputer-Adv USB-C
        |
        | USB MIDI
        v
Yamaha SEQTRAK USB
```

GroovePuter internal audio remains active independently.

## Timing contract

Existing project transport remains authoritative:

```text
GroovePuter internal sequencer: 96 PPQN
USB MIDI Clock:                 24 PPQN
SMF file:                       native PPQN division
```

`PROJECT` mode does not quantize SMF notes to 1/16. Instead:

```text
SMF tick delta
    -> quarter-note fraction from file division
    -> GroovePuter project steps
    -> project BPM
    -> blockSequence + frameOffset
```

Therefore triplets, syncopation and note lengths remain musical-time relative while the absolute duration follows the current GroovePuter BPM.

The project transport snapshot is published from the same `MusicalEventQueue::beginMidiRenderBlock()` bracket that already publishes MIDI Clock. There is no independent wall-clock scheduler and no second TinyUSB owner.

## SEQTRAK routing assumptions

The routing model inherited from the preceding SEQTRAK target PR remains unchanged:

```text
CH1  KICK
CH2  SNARE
CH3  CLAP
CH4  HAT1
CH5  HAT2
CH6  PERC1
CH7  PERC2
CH8  SYNTH 1
CH9  SYNTH 2
CH10 DX
CH11 SAMPLER
```

GM source CH10 drums continue to split onto native SEQTRAK drum lanes CH1..7.

DX remains an explicit destination and is never used as the generic fallback for extra melodic channels.

## Controls

On the MIDI Player page:

```text
T           ORIGINAL <-> PROJECT
Space       SMF Play / Pause
Up / Down   ORIGINAL: adjust SMF BPM by 1
            PROJECT:  adjust GroovePuter project BPM by 1
R           restart from MUSIC START
Left/Right  seek -/+ 1 bar
M           RAW / SEQTRAK routing
V           velocity boost
O           ORIGINAL tempo reset
X           player-scoped panic / pause
B           file browser
D           diagnostics
```

In PROJECT mode, selecting a MIDI file does not stop an already running GroovePuter transport. If GroovePuter is stopped, the MIDI page starts project transport through the existing UI/control path before arming the file.

The player then shows `ARMED` and schedules its first MIDI event for a future project bar boundary. If the immediate next boundary is too close to prefill safely, it deliberately chooses the following bar rather than generating a late burst.

## Build / flash

Use the repository's existing Cardputer-Adv build and upload scripts. Do not switch to ESP-IDF for this Arduino firmware.

Run host regressions first:

```bash
./tests/run_host_tests.sh
```

Then run the existing Cardputer-Adv build workflow/script used by the repository and flash over the normal CDC upload path.

The supported M5Stack ESP32 Arduino core remains pinned by the repository build configuration.

## Test procedure

### 1. Baseline

1. Boot Cardputer-Adv normally.
2. Confirm internal GroovePuter audio still works.
3. Confirm PERFORM A / B / DX / DRUMS still reaches expected SEQTRAK targets.
4. Confirm ordinary GroovePuter Start / Clock / Stop still drives SEQTRAK.

### 2. ORIGINAL regression

1. Open MIDI Player.
2. Ensure `TEMPO ORIGINAL` is selected.
3. Load a known MIDI.
4. Verify current faithful realtime playback, seek, restart, velocity and routing behavior remains unchanged.

### 3. PROJECT launch

1. Press `T` until the MIDI Player shows `PROJECT`.
2. Set GroovePuter to 90 BPM.
3. Start GroovePuter transport if it is not already running.
4. Build or play a simple drum groove on SEQTRAK while it follows GroovePuter Clock.
5. Select a recognizable MIDI file.
6. Observe `ARMED`.
7. Verify the MIDI enters on a bar boundary without manually catching beat 1.

### 4. Tempo follow

1. While PROJECT playback is active, change GroovePuter BPM from 90 to 110 using Up/Down on the MIDI page or normal project tempo controls.
2. Verify SEQTRAK Clock follows the new BPM.
3. Verify future SMF events continue using project timing without a catch-up burst.
4. Verify no stuck notes remain after the bounded tempo re-anchor cleanup.

### 5. Musical timing

Use a MIDI containing triplets or obvious syncopation.

Verify that PROJECT mode changes tempo but does not flatten the phrase to a 1/16 grid.

## Expected behavior

- `ORIGINAL` behaves as before this PR.
- `PROJECT` displays `ARMED` before quantized entry.
- GroovePuter remains transport master.
- SEQTRAK receives one coherent Start / Clock / Stop timeline.
- SMF tick spacing follows current project BPM.
- triplets and syncopation remain recognizable.
- pausing SMF does not automatically stop project transport.
- stopping GroovePuter transport stops/disarms PROJECT SMF and releases SMF-owned notes.
- no unrelated PERFORM or PatternPlayer note ownership is cleared by player-scoped cleanup.

## Troubleshooting

### PROJECT remains `WAIT PROJECT PLAY`

GroovePuter transport is not running or the audio-block timeline has not published a valid block yet. Start project transport and confirm normal MIDI Clock is reaching SEQTRAK.

### Phrase starts one bar later than expected

This is intentional when the nearest bar boundary is too close for the bounded SD/parser prefill lead. The scheduler chooses the following bar instead of sending late notes.

### SEQTRAK tempo changes but SMF does not

Confirm the MIDI Player still shows `PROJECT`, not `ORIGINAL`.

### Wrong instrument on SEQTRAK

Check `RAW` versus `SEQTRAK` routing. In SEQTRAK mode the established target model is CH8/CH9/CH10/CH11 plus native drum CH1..7.

### Hanging note after Stop or tempo change

Treat this as a failure. Capture serial diagnostics and reproduce with `X` panic. PROJECT transitions must use the existing SMF generation invalidation and ownership cleanup.

### UI/audio stutter

Open `D` diagnostics and compare queue depth / schedule latency with the previous player stage. PROJECT uses a shorter bounded lookahead than ORIGINAL to make project BPM changes responsive.

## Acceptance checklist

```text
[ ] normal boot succeeds
[ ] internal GroovePuter audio unchanged
[ ] PERFORM A/B/DX/DRUMS unchanged
[ ] PatternPlayer behavior unchanged
[ ] existing MIDI Start/Clock/Stop reaches SEQTRAK
[ ] ORIGINAL SMF playback unchanged
[ ] T switches ORIGINAL <-> PROJECT
[ ] PROJECT file load does not stop an already running project transport
[ ] stopped project transport can be started through the existing UI/control path
[ ] PROJECT shows ARMED before launch
[ ] first PROJECT notes enter on a project bar boundary
[ ] no manual beat-catching is required
[ ] project BPM 90 -> 110 changes both SEQTRAK Clock and SMF timing
[ ] no catch-up MIDI burst after BPM change
[ ] triplets remain triplets
[ ] syncopation remains recognizable
[ ] SMF pause can leave project transport running
[ ] project Stop releases/disarms SMF-owned notes
[ ] restart/seek/panic leave no stuck notes
[ ] RAW routing unchanged
[ ] source melodic CH1 -> SEQTRAK CH8
[ ] source melodic CH2 -> SEQTRAK CH9
[ ] source melodic CH3 -> DX CH10 only
[ ] source melodic CH4+ -> SAMPLER CH11
[ ] GM source CH10 drums -> native CH1..7
[ ] no watchdog/reset
[ ] no sustained audio-underrun regression
[ ] host-tests green
[ ] SDL build green
[ ] Cardputer-Adv build green
```
