# GroovePuter 0.9.6-B — Canonical Output Ownership Contract

## Purpose

Introduce the smallest authoritative runtime contract for logical GroovePuter track output intent:

```text
Synth A
Synth B
Drums

INTERNAL / MIDI / LAYER
```

This checkpoint creates the owner and executable truth-table tests only. It does not yet alter Pattern, PERFORM, Drums/Sampler, Scene persistence, or UI behavior.

Production baseline for this temporary 0.9.6 line:

```text
0.9.4 FINAL
d3db4e48ebc08862bdaf9f62532414f009839192
```

`dev_0.9.5` still points to the same frozen tree while 0.9.5-A is an unmerged draft. Before 0.9.6 release integration, this branch must be rebased/re-audited if 0.9.5 advances.

## Contract

User-visible modes:

```text
INTERNAL -> local audio only
MIDI     -> external MIDI only
LAYER    -> local audio + external MIDI
```

The contract is orthogonal to:

- mute;
- MIDI channel/route;
- device profile;
- USB connection state;
- synth TYPE/parameters;
- sampler layer enable/pad assignment;
- receiver MONO/POLY;
- transport/clock state.

### Legacy compatibility

Old Scenes have no OutputMode field and frozen behavior is source-dependent:

```text
Pattern/Song -> internal + MIDI
PERFORM      -> MIDI only
Sampler preview -> internal only
```

The runtime therefore reserves raw state `0` as a versioned **legacy-compatibility decode phase**. It is not exposed as a fourth mode. The first explicit user/project mode replaces it with INTERNAL, MIDI, or LAYER.

### Realtime boundary

The owner uses the existing 32-bit `MidiRealtimeWord` primitive. No heap, mutex, filesystem, JSON, USB call, or dynamic route table is introduced.

Three 8-bit external-disable epochs are packed into one aligned word. Later MIDI queue integration consumes these epochs to guarantee scoped cleanup even if several mode changes happen between dispatcher iterations.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- normal no-PSRAM firmware profile
- USB-C data cable
- Yamaha SEQTRAK is optional for this contract-only checkpoint

## Wiring

No new wiring.

PORT.A is unchanged and unused by this test:

```text
SDA GPIO2
SCL GPIO1
```

## Build / Flash steps

Focused host contract:

```bash
bash tests/run_output_ownership_tests.sh
```

Normal repository gates before accepting the checkpoint:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

This B checkpoint has no user-visible runtime behavior, so flashing is only a smoke check:

```bash
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Expected behavior

### Screen

No new page, status label, or key binding appears yet.

### Serial

Normal boot/runtime diagnostics remain unchanged. There is no new realtime logging from OutputOwnership.

### Host test

The focused test must print:

```text
Output ownership contract tests: PASS
0.9.6 output ownership contract gate: PASS
```

It proves:

- all three user-visible modes;
- exact internal/external truth table;
- frozen legacy source-dependent compatibility;
- Synth A/B/Drums target mapping;
- DX remains outside the 0.9.6 groove-track axis;
- INTERNAL -> MIDI -> LAYER -> INTERNAL cycle order;
- MIDI-disable epoch increments only when a transition removes the external side;
- re-applying the same mode is a no-op.

## Troubleshooting

### Host compile fails in `MidiRealtimeWord`

Do not replace it with a heap object or mutex. The output owner deliberately reuses the already accepted cross-core primitive used by MIDI queues.

### Existing project behavior changes

Reject the checkpoint. 0.9.6-B is contract-only; no Pattern/PERFORM/audio/MIDI path should consume the owner yet.

### Fixed DRAM increases unexpectedly

Treat it as a regression. This checkpoint should add only the compact state words and code required by linked callers/tests; it must not allocate per-track objects dynamically.

### 0.9.5 advances

Do not silently merge this branch over a changed sampler/routing baseline. Rebase onto exact 0.9.5 FINAL and repeat the routing/Sampler delta audit before C/D integration.

## Acceptance checklist

- [ ] branch parent recorded as exact production baseline;
- [ ] `tests/run_output_ownership_tests.sh` PASS;
- [ ] INTERNAL/MIDI/LAYER truth table PASS;
- [ ] legacy Pattern/PERFORM/Preview compatibility PASS;
- [ ] disable-epoch semantics PASS;
- [ ] no fourth user-visible OutputMode;
- [ ] no new scheduler/queue/router;
- [ ] no Scene/persistence change;
- [ ] no UI change;
- [ ] no synth/drum/sampler behavior change;
- [ ] full host suite PASS;
- [ ] SDL PASS;
- [ ] Cardputer ADV compile PASS;
- [ ] fixed DRAM gate PASS;
- [ ] SEQTRAK MIDI-only compile PASS;
- [ ] cold boot reaches normal UI with no WDT/reset.

## Next production stages

After B is green:

```text
0.9.6-C  Synth A/B vertical migration
0.9.6-D  Drums/Sampler vertical migration
0.9.6-E  live transition cleanup
0.9.6-F  Scene persistence + existing-page UI
0.9.6-G  Cardputer ADV + SEQTRAK acceptance
```

Do not add Device Profiles, a second MIDI dispatcher, sampler storage changes, Tape recovery, new synth engines, or Song/Phrase rewrites to this checkpoint.
