# 0.9.7-R6 — Performance route binding

## Purpose

Extend Device Profiles from Pattern output to the existing live/performance MIDI path without introducing runtime route mutation.

## Scope

R6 derives a complete Performance route table for the predefined profiles only:

- `SEQTRAK NATIVE`
- `GENERAL MIDI`
- `GENERIC MIDI`

`CUSTOM` remains explicitly incomplete because the persisted settings model does not contain a complete A/B/DX/live-drum Performance target table. R6 preserves the existing legacy Performance channels for Custom instead of inventing routes.

## Wire contract

### SEQTRAK NATIVE

- Synth A -> CH8
- Synth B -> CH9
- DX -> CH10
- live drum logical lanes 0..6 -> CH1..7, note 60
- receiver MONO/POLY control -> SEQTRAK CC26

### GENERAL MIDI

- Synth A -> CH1
- Synth B -> CH2
- DX -> CH3
- live drums -> CH10 with notes 36, 38, 42, 46, 43, 47, 37
- no vendor receiver-mode control

### GENERIC MIDI

- Synth A -> CH1
- Synth B -> CH2
- DX -> CH3
- live drums disabled because Generic MIDI must not imply a percussion map
- no vendor receiver-mode control

### CUSTOM

- existing `UsbMidiRouteConfig` Performance channels remain the fallback
- no SEQTRAK CC26 is emitted
- a future Custom Performance editor/model must make the table explicit before Custom can be called complete

## Ownership and realtime rules

The single R5 startup snapshot is extended instead of adding another global route owner. `UsbMidiOutput::begin()` consumes the predefined Performance routes once. Running event dispatch never re-reads profile state.

Only seven extra mapped live-drum note bytes and small capability flags remain in `UsbMidiOutput`; profile/settings objects stay control-side.

Physical note ownership is still handled by the existing `wireOwners[channel][note]` table. A Performance owner and an SMF owner may share a physical note without one scoped cleanup silencing the other.

## Validation

```bash
bash tests/run_midi_performance_route_binding_0_9_7_tests.sh
```

The gate executes R1 -> R5 first, then verifies predefined Performance channels, live-drum physical notes, SEQTRAK-only CC26 and shared Performance/SMF ownership.

## Out of scope

- runtime profile switching
- profile persistence selection API
- Device Profile UI
- Custom Performance route editor
- drum INTERNAL/SAMPLE/BOTH/MIDI output ownership

The last item belongs to the separate output-ownership track and is intentionally untouched here.
