# 0.9.10 Performance V1 Integration Plan

> Checkpoint: `0.9.10-PERFORMANCE-V1-INTEGRATION`

## Goal

Converge the fully proven Performance Instrument V1 development line into the current authoritative `dev_0.9.10` without changing GF2 generation semantics, PATTERN playback semantics, or the PATTERN/PHRASE P1C runtime-event representation.

## Frozen inputs

- authoritative development base: `dev_0.9.10 @ 9c01b0c34b80aacb1dd6be66bb07c5cef3ad1c38`
- Performance parent after closure merge: `feature/20260903-01-0.9.9-performance-instrument-v1 @ fb08d0a68920ecfa5e2bfcf57f419592b48fba9b`
- Performance closure PR: `#431`, merged into the parent at `fb08d0a68920ecfa5e2bfcf57f419592b48fba9b`
- parent focused proof: run `33804041729` — PASS
- parent Core proof: run `33804041525` — PASS

## Ancestry audit

Merge base:

`0a2a6211ef00dcf2214dfd4704b6c34b424b1c9d`

The two lines diverged from the accepted 0.9.9 base. Current 0.9.10 is 113 commits ahead of that base; the Performance parent contributes 56 commits.

### Performance-owned changes

- `src/input/performance_keyboard.*`
- `src/input/performance_instrument_types.h`
- `src/input/performance_chord_detector.h`
- `src/input/performance_pulse.h`
- `src/input/internal_synth_output.*`
- `src/ui/pages/perform_page.*`
- Performance-focused tests/docs/workflows
- the Performance-side `MiniAcid` live-projection ownership hooks

### Shared-owner overlap

The following paths exist or changed independently on both lines and require semantic resolution rather than blanket ours/theirs:

- `src/dsp/miniacid_engine.cpp`
- `src/midi/midi_transport.h`
- `src/midi/tee_midi_transport.h`
- `src/midi/uart_midi_transport_core.h`
- `src/midi/usb_midi_transport.h`
- `src/platform/cardputer_uart_midi_transport.cpp`
- `src/platform/cardputer_uart_midi_transport.h`
- `tests/test_midi_transport_source_regressions.py`
- `tests/test_performance_source_regressions.py`
- `tests/test_source_regressions.py`
- `tests/test_tee_midi_transport.cpp`

### Resolution policy

1. Keep current 0.9.10 MIDI transport ownership and its expanded B1 tests. It is the same transport-neutral design as the Performance copy, but is the newer authoritative version and includes current `MidiNoteOwnershipTable` ownership.
2. Add the Performance V1 `MiniAcid` live-projection ownership hooks on top of the current 0.9.10 engine. Do not remove current 0.9.10 engine changes.
3. Keep final Performance keyboard/internal-mono arbitration semantics from the Performance parent.
4. Merge tests so that final Performance assertions target the current 0.9.10 MIDI API/owner (`MidiNoteOwnershipTable` / current ownership surface), rather than restoring stale 0.9.9 names.
5. Preserve `src/phrase/runtime_synth_events.h/.cpp` byte/ABI semantics and keep them inaudible.
6. Do not modify GF2 generation owners or thresholds.

## TDD sequence

### BEFORE

On this integration branch before importing Performance:

- GF2 I1 PASS
- GF2 I2 PASS
- GF2 I2A PASS
- GF2 I3 PASS
- GF2 I4 PASS
- GF2 I5 PASS
- P1C focused PASS
- Core HOST PASS
- SDL PASS
- CARDPUTER_ADV PASS
- FIXED_DRAM PASS
- SEQTRAK MIDI-only PASS

The integration acceptance workflow also intentionally expects the Performance focused job to be RED before the Performance line is present. This proves that the integration test actually distinguishes the pre-integration tree from the target tree.

### INTEGRATE

Merge the complete Performance parent into this branch. Resolve only actual shared-owner conflicts according to the policy above. Do not squash or manually copy the feature line.

### AFTER

Require on one exact integration head:

- Performance closure suite PASS
- GF2 I1/I2/I2A/I3/I4/I5 PASS
- P1C focused PASS
- Core HOST/SDL/CARDPUTER_ADV/FIXED_DRAM/SEQTRAK PASS
- no warning regression
- no GF2 semantic delta
- no P1C semantic delta
- no PATTERN playback semantic delta

Record RAM, Flash, and fixed DRAM headroom from the exact-head Cardputer build and compare them with the BEFORE baseline.

## Hardware gate

Software Cardputer builds are not physical hardware acceptance. Final Cardputer ADV runtime arbitration remains a separate release-hardware gate unless a physical run is performed on the integrated candidate.

## Stop condition

After the accepted Performance V1 integration is present on `dev_0.9.10`, stop. Do not start Performance V2 or PATTERN/PHRASE P2 in this checkpoint.
