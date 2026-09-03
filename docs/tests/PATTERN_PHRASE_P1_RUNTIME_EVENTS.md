# PATTERN / PHRASE P1 — Runtime Event Projection Test

## Purpose

Validate the first behavior-neutral production layer for the PATTERN xor PHRASE architecture: a fixed-capacity synth event value with explicit start/duration and a pure projection of the existing 16-step `SynthPattern` semantics.

P1 does not switch playback to the new buffer. It proves that current PATTERN timing/lifetime can be represented without changing MiniAcid, Scene, Undo, UI or MIDI routing.

## Hardware list

Focused P1 validation is host-only:

- Linux runner;
- GCC;
- Clang;
- AddressSanitizer;
- UndefinedBehaviorSanitizer.

Cardputer ADV and SAM2695 are not required for the focused projection test. Normal repository Cardputer/fixed-DRAM/SEQTRAK CI remains required before merge.

## Wiring

None.

No PORT.A, USB MIDI or DIN-UART wiring is changed by P1.

## Build / Flash steps

No firmware flash is required for the focused test.

From repository root:

```sh
bash tests/run_pattern_phrase_p1_runtime_events.sh
```

The runner executes the source firewall, GCC deterministic repeat, Clang parity, ASan and UBSan.

## Expected behavior

GREEN output contains:

```text
P1-A PASS: fixed-capacity ABI and phrase budget frozen
P1-B PASS: onset/articulation projection is explicit data
P1-C PASS: existing Synth A/B gate scaling is represented
P1-D PASS: step15 swing+microtiming wraps to tick 11
P1-E PASS: step0 negative microtiming wraps to tick 361
P1-F PASS: legacy TIE crossing is one explicit-duration event
P1-G PASS: expired legacy TIE does not revive a note
P1-H PASS: subsequent onset clips previous monophonic lifetime
P1-I PASS: invalid projection leaves caller buffer untouched
PATTERN/PHRASE P1 runtime events: OK
PATTERN/PHRASE P1 focused gate: PASS
```

During the TDD RED commit the expected failure is instead a compiler error that `src/phrase/runtime_synth_events.h` is missing.

## Troubleshooting

### Focused test compiles before production implementation

Stop. The RED contract is not testing the new surface. Verify the runtime header does not already exist on the branch.

### Tick 11 or 361 differs

Do not normalize the fixture. Compare the projector scan with `MiniAcid::processSequencerEvents()` and the frozen P0 characterization.

### TIE creates a second event

The projection is wrong. Legacy `note == -2` is a compatibility continuation sentinel, not an onset and not PHRASE storage.

### Runtime buffer size changes

Treat this as a memory-budget change. Review fixed DRAM before accepting it.

### Source firewall fails

P1 scope expanded. Move Scene/Undo/UI/AudioMutationGate/scheduler work to its later PR instead of weakening the guard.

## Acceptance checklist

- [ ] RED observed before `src/phrase/runtime_synth_events.*` exists
- [ ] `RuntimeSynthEvent` = 10 bytes
- [ ] `RuntimeSynthEventBuffer` = 1284 bytes
- [ ] 128-event / 8-bar capacity frozen
- [ ] simple onset/articulation projection PASS
- [ ] Synth A/B current gate scaling represented
- [ ] P0 tick-11 swing+microtiming fixture PASS
- [ ] P0 tick-361 negative microtiming fixture PASS
- [ ] legacy TIE crossing becomes one event whose end crosses tick 384
- [ ] expired TIE does not revive a note
- [ ] next onset clips previous monophonic lifetime
- [ ] invalid synth index leaves output unchanged
- [ ] no GRID dependency
- [ ] no Scene/PhraseBank/Undo/AudioMutationGate dependency
- [ ] no dynamic allocation
- [ ] GCC deterministic repeat PASS
- [ ] Clang parity PASS
- [ ] ASan PASS
- [ ] UBSan PASS
- [ ] normal SDL/Cardputer/fixed-DRAM/SEQTRAK CI PASS before merge
