# 0.9.7-R4 — MIDI Output Route Projection

## Purpose

Create one pure, fixed-size projection from the existing global `MidiOutputSettings` + device capabilities into physical MIDI route facts that a later USB binding can consume.

R4 does **not** modify `UsbMidiOutput`, active-note ownership, Pattern producers, PerformanceKeyboard, persistence schema, UI, or the audio callback.

Stack:

```text
R1 contracts  @ 246ff995f962d285fe7d986e16c63607054931b5
R2 capability @ f4ebc439406c68cd780e58ddb2b9c3565bcec4fb
R3 runtime    @ 9c3448c473345fbc34f42a1c1dc014f1e025e114
R4 projection
```

## Why a projection is required

The current USB output is still SEQTRAK-oriented:

- Pattern drum channels are hardcoded in `UsbMidiOutput::patternDrumChannel()`;
- Pattern drum producers publish physical note `60`;
- Performance DRUMS publishes native SEQTRAK channels `0..6` + note `60`;
- receiver-mode CC26 is hardcoded in `UsbMidiOutput`.

Those rules cannot represent the existing `GeneralMidi` settings, where all drums use CH10 with distinct notes, or `GenericMidi`, where no drum mapping is assumed.

R4 makes the intended physical projection explicit before the live output path is changed.

## Projection model

`MidiOutputRouteProjection` contains only fixed-size values:

```text
profile
selected live route: enabled / target / channel
Pattern Synth A: enabled / channel
Pattern Synth B: enabled / channel
Pattern Drums[8]: enabled / channel / note
drum gate ms
receiver-mode control capability
completePerformanceTargetTable = false
```

No heap allocation or variable-size container is introduced.

### SEQTRAK

Pattern drums project to the validated native mapping with note 60. `receiverModeControl` is explicitly `SeqtrakCc26`.

### General MIDI

Pattern drums project to channel 10 with conventional per-voice notes 36/38/42/46/43/47/37/39. No SEQTRAK CC26 capability is present.

### Generic MIDI

No default drum routes are enabled. No vendor receiver-mode control is present.

### Custom

The user's exact valid per-voice channel/note/enabled map is preserved.

## Ownership

Projection combines global and per-route enable state but does not mutate the source settings.

```text
wire route enabled =
    master MIDI enabled
    && domain enabled
    && route enabled
```

Device capabilities contribute facts such as `SeqtrakCc26`; they do not change user-owned enable/live/clock intent.

## Known incomplete boundary

The historical `MidiOutputSettings` model owns one selected live `target + channel`. It does **not** own the complete PerformanceKeyboard A/B/DX/Drums target table currently hardcoded by `UsbMidiOutput` / `PerformanceKeyboard`.

Therefore R4 explicitly publishes:

```text
completePerformanceTargetTable = false
```

R5 must not pretend otherwise. DX and full multi-target performance routing require an explicit ownership decision before UI profile switching can be considered complete.

## Hardware list

None for R4. This is a platform-neutral route-model checkpoint.

## Wiring

None. No USB, GPIO, I2C, I2S, SD, or audio wiring changes.

## Build / test

```bash
bash tests/run_midi_output_route_projection_0_9_7_tests.sh
```

The runner executes R1 -> R2 -> R3 first, then R4 source/executable tests.

## Expected behavior

- SEQTRAK projection reproduces current Pattern A/B and all eight drum routes;
- GM projection carries both CH10 and distinct physical drum notes;
- Generic MIDI exposes no assumed drum map;
- Custom preserves user route values;
- master/domain/per-route enable flags compose deterministically;
- only SEQTRAK exposes `SeqtrakCc26` receiver-mode control;
- current selected live target/channel is represented;
- full PerformanceKeyboard target table remains explicitly incomplete;
- production USB routing remains unchanged.

## Troubleshooting

If R4 needs Arduino/TinyUSB/UI/audio headers, the projection boundary is wrong. Keep it platform-neutral.

If GM notes disappear from the projection, the model cannot replace the current hardcoded note-60 path later.

If Generic MIDI enables a default drum route, Generic and General MIDI have been conflated again.

If `UsbMidiOutput` starts including the projection header in R4, live binding leaked forward into the model checkpoint; move that work to R5.

## Acceptance checklist

- [ ] fixed-size/no-allocation projection;
- [ ] R1/R2/R3 contracts still pass;
- [ ] SEQTRAK 8-voice mapping + note 60 preserved;
- [ ] General MIDI CH10 + per-voice notes preserved;
- [ ] Generic MIDI default drums disabled;
- [ ] Custom exact drum map preserved;
- [ ] global/domain/per-route enables compose correctly;
- [ ] SEQTRAK CC26 represented as an explicit capability;
- [ ] no vague vendor flag is used to authorize CC26;
- [ ] incomplete Performance target table is explicit;
- [ ] no USB routing/active-note/producer changes;
- [ ] dedicated R4 host gate passes.

## Next

R5 is the first live-routing stage. It must consume this projection with active-note-safe route replacement, remove SEQTRAK physical channel/note assumptions from the Pattern drum wire path, and gate CC26 by `MidiReceiverModeControl::SeqtrakCc26`.

Do not expose profile switching in UI until that route application is real and tested.
