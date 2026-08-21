# GroovePuter 0.9.9-D3 — Hardware Run Log

## Purpose

Record one physical D3 acceptance run without changing the test specification in `docs/tests/0_9_9_D3_CARDPUTER_ADV_HARDWARE_ACCEPTANCE.md`.

This log is evidence only. It does not replace CI and it must never combine results from different firmware SHAs.

## Current state

```text
Hardware-validation branch: agent/20260821-04-0.9.9-d3-hardware-validation
Reference D3 checkpoint:  a4419e402b65f80ddffd2a349af798f58989e9d2
Hardware candidate:         NOT SELECTED
Run status:                 NOT RUN
```

The reference checkpoint above is diagnostic and is not approved for flashing because its complete Cardputer ADV/fixed-DRAM software gate is not green. Select the later immutable D3 candidate only after the full software matrix passes.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3FN8.
- USB-C data cable.
- Built-in speaker or headphones.
- Yamaha SEQTRAK for the final MIDI/output smoke.
- Development computer with `arduino-cli` and pinned project dependencies.

## Wiring

No D3-specific wiring.

- USB-C: Cardputer power, flash, Serial.
- Audio: built-in ES8311 speaker/headphone path.
- SEQTRAK: existing validated GroovePuter USB-MIDI path.
- PORT.A is unused; if connected, preserve SDA GPIO2 / SCL GPIO1.

## Build / Flash evidence

Before testing, replace the `NOT SELECTED` state above with the exact immutable SHA and record the command outputs in the repository/PR discussion or attached CI evidence.

Commands:

```bash
git rev-parse HEAD
bash tests/run_0_9_9_d3_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Software gate record

| Gate | Status |
|---|---|
| D3 focused | NOT RUN |
| Undo R4-R7 | NOT RUN |
| C / D1 / D2 cumulative | NOT RUN |
| Core host | NOT RUN |
| SDL | NOT RUN |
| ADV normal | NOT RUN |
| ADV fixed DRAM | NOT RUN |
| SEQTRAK MIDI-only | NOT RUN |
| tonal | NOT RUN |
| Synth persistence | NOT RUN |
| sampler | NOT RUN |
| output ownership | NOT RUN |

## Physical test record

| ID | Case | Status | Notes |
|---|---|---|---|
| D3-HW-01A | clear current Synth A | NOT RUN | none |
| D3-HW-01B | clear current Synth B | NOT RUN | none |
| D3-HW-01C | clear current Drums | NOT RUN | none |
| D3-HW-02A | replace current Synth A | NOT RUN | none |
| D3-HW-02B | replace current Synth B | NOT RUN | none |
| D3-HW-02C | replace current Drums | NOT RUN | none |
| D3-HW-03 | Undo before boundary | NOT RUN | none |
| D3-HW-04 | Ctrl+B PLAY A/B boundary switch | NOT RUN | none |
| D3-HW-05 | reverse boundary switch | NOT RUN | none |
| D3-HW-06 | STOP settlement | NOT RUN | none |
| D3-HW-07U | A -> Undo terminal -> B, 10 repeats | NOT RUN | none |
| D3-HW-07S | A -> STOP terminal -> B, 10 repeats | NOT RUN | none |
| D3-HW-08 | 2-3 minute edit stress | NOT RUN | none |
| D3-HW-09 | SEQTRAK MIDI/output smoke | NOT RUN | none |

## Defect-class record

| Defect class | Status | Notes |
|---|---|---|
| mid-row discontinuity | NOT RUN | none |
| stale snapshot after terminal state | NOT RUN | none |
| stuck note / timing discontinuity | NOT RUN | none |
| reset / Guru / watchdog / stack canary / heap corruption | NOT RUN | none |

## Expected behavior

- A current-row clear/replace changes persistent Song truth immediately but does not change current audible truth before the row boundary.
- Undo before boundary restores persistent truth and invalidates the exact pending revision without a gap.
- Ctrl+B changes PLAY A/B once at the row boundary; EDIT A/B navigation never redirects playback.
- Ctrl+R changes direction once at the row boundary.
- STOP settles committed truth and leaves no stale pending snapshot for restart.
- After `A -> terminal -> B`, B can never read A's retired audible snapshot.
- INTERNAL/MIDI/LAYER output ownership remains unchanged.

## Troubleshooting

- If any case produces an immediate mid-row change, stop the run and mark the candidate FAIL.
- If stale material appears after Undo/STOP or after a following B action, mark D3-HW-07 FAIL and capture the exact key sequence.
- If only Drums fail, capture whether the failure is an early dropout, stale hit, or timing jump; the drum microtiming path must obey the same D3 snapshot ownership.
- If a second request reports BUSY while a first audible activation is pending, that is expected backpressure, not a failure.
- If a reset occurs, capture the complete Serial reset reason and do not continue counting later cases as valid evidence for the same run.

## Acceptance checklist

- [ ] One exact firmware SHA used for all physical cases.
- [ ] Full software matrix green on the same SHA.
- [ ] Separate ADV fixed-DRAM PASS recorded.
- [ ] D3-HW-01 through D3-HW-08 all PASS.
- [ ] D3-HW-09 PASS for the SEQTRAK release path.
- [ ] Zero mid-row discontinuity reproductions.
- [ ] Zero stale snapshots after terminal state.
- [ ] Zero stuck-note/timing discontinuity reproductions.
- [ ] Zero crash/reset/watchdog/stack/heap failures.

Only after every item above is true may the run status be changed from `NOT RUN` to `PASS`.