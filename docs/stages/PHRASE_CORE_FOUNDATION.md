# Phrase Core foundation

## Purpose

Provide the first real domain object between Pattern and Section/Song without adding a second scheduler, transport, event format, or UI-owned state store.

This stage introduces four fixed working slots:

```text
A MAIN
B VARIATION
C BREAK
D ENDING
```

A slot contains bounded multi-track musical material for `1 / 2 / 4 / 8` bars. In this stage the backing store is explicitly a **REFERENCE VIEW** over existing Synth A, Synth B, and Drums pattern references.

A reference view is not an independent event copy. Editing a referenced pattern changes the material heard through every Phrase that points at that pattern. The UI must show `REF` or `REFERENCE VIEW` and must not claim that the Phrase owns copied events.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- built-in 240×135 display
- built-in QWERTY keyboard
- optional Yamaha SEQTRAK or other MIDI target is not required for this stage

## Wiring

No external wiring is required.

Normal Cardputer ADV audio and MIDI wiring remains unchanged. This stage does not access PORT.A, I2C, SPI, TinyUSB, I2S, or external GPIO.

## Backend contract

Implementation:

```text
src/phrase/phrase_core.h
```

The module is header-only and allocation-free. It owns no singleton and no transport state. The caller owns the `PhraseBank`, performs any required AudioGuard transaction, persists the value through the established Scene/project codec, and advances Scene revision exactly once after a successful user-visible mutation.

### Memory budget

```text
PhraseMetadata: 12 bytes per slot
Reference grid:  48 bytes per slot
PhraseSlot:      60 bytes
Four slots:      240 bytes
Bank/version:    4 bytes
PhraseBank:      244 bytes total
```

Compile-time `static_assert` gates enforce these values.

There is no heap allocation in capture, derive, clear, summary, validation, sanitize, or write-to-Song operations.

### Metadata

Each valid slot exposes:

- stable `phraseId`;
- optional `parentId`;
- length in bars;
- role;
- provenance/source;
- storage mode;
- source Song slot and starting row;
- selected track mask;
- explicit mutable-backing flag.

### Sources

The common source enum already reserves the intended provenance values:

```text
INTERNAL_PATTERN
GENERATED
DERIVED
SMF_REGION
LIVE_CAPTURE
```

Only reference-backed Song/pattern capture is implemented by this stage. `SMF_REGION`, `LIVE_CAPTURE`, and `OWNED EVENTS` are vocabulary reservations, not claims of implemented extraction or capture.

### Atomic operations

The public operations validate all input before committing:

- capture a Song region into A/B/C/D;
- derive a child reference view from another slot;
- clear a slot;
- write a Phrase reference view into a Song destination;
- sanitize loaded persistence state.

Failed operations leave the destination Phrase bank or Song unchanged.

## UI integration contract

The Phrase UI may:

- select A/B/C/D;
- select `1 / 2 / 4 / 8` bars;
- capture the current Song selection or cursor-aligned region;
- show role, source, length, parent, track mask, and `REF` storage mode;
- derive a new slot from an existing valid slot;
- write a Phrase to a Song row after explicit overwrite confirmation;
- render a mini pattern/reference grid and per-bar energy projection;
- provide current footer hints and `Alt+H` help.

The Phrase UI must not:

- create a second Phrase struct or page-local persistent store;
- modify Scene fields directly outside the established mutation gateway;
- claim `COPIED`, `OWNED`, `RECORDED`, or `EXTRACTED` for a reference view;
- scan or hash pattern contents every frame;
- allocate in playback paths;
- start a new scheduler or transport mode;
- implement retrospective live capture in this stage.

## Build / flash

### Focused host contract

```bash
g++ \
  -std=c++17 \
  -Wall \
  -Wextra \
  -Werror \
  -I. \
  tests/test_phrase_core.cpp \
  -o /tmp/test_phrase_core

/tmp/test_phrase_core
```

Expected output:

```text
Phrase Core tests passed
```

### Repository gates

```bash
./tests/run_host_tests.sh
./scripts/build_sdl.sh
./scripts/build_cardputer_adv.sh
```

Use the repository's current build scripts/profile names if they differ on the checked-out `dev` revision.

### Flash

Build and flash the combined backend/UI branch with the normal Cardputer ADV profile:

```text
PSRAM=disabled
PartitionScheme=huge_app
```

## Expected behavior

Backend-only branch:

- no visible firmware behavior changes;
- focused Phrase Core host test passes;
- exact RAM gates compile;
- old/zero-initialized Phrase persistence sanitizes to four empty slots.

Combined UI/backend branch:

1. Open Song and select or position the cursor on material spanning `1 / 2 / 4 / 8` bars.
2. Open Phrase UI.
3. Capture into slot A.
4. Screen shows `A MAIN`, selected length, source, tracks, and `REF`.
5. Derive A into B as `VARIATION`.
6. B shows A's phrase ID as parent and receives a new phrase ID.
7. Write B to an empty Song destination.
8. The destination contains the same bounded pattern references.
9. Attempting a non-overwrite write onto occupied cells is rejected without partial changes.
10. Save/load restores the Phrase bank once Scene codec integration is present.

## Troubleshooting

### Phrase capture says EMPTY

The selected bars contain no non-negative Synth A, Synth B, or Drums pattern references for the selected track mask. Generate or assign material first.

### Phrase capture says RANGE

The requested length extends beyond the current Song length or beyond row 128. Move the cursor earlier or choose a shorter Phrase length.

### Write says OCCUPIED

At least one selected destination track already contains a pattern reference. Choose an empty destination or explicitly confirm overwrite in the UI.

### Edited pattern changes an existing Phrase

Expected in this stage. The screen must show `REF`: the Phrase references pattern slots and does not own copied events.

### Phrase disappears after reboot

The UI branch has not yet integrated `PhraseBank` with the established Scene/project codec, or the loaded state was rejected by `sanitize()`. Do not add a sidecar persistence system; integrate through the current codec.

## Acceptance checklist

Backend/domain:

- [ ] `sizeof(PhraseMetadata) == 12`.
- [ ] `sizeof(PhraseSlot) == 60`.
- [ ] `sizeof(PhraseBank) == 244`.
- [ ] Four slots exist with stable A/B/C/D identities.
- [ ] Only `1 / 2 / 4 / 8` lengths are accepted.
- [ ] Capture is atomic on invalid length, invalid range, empty material, and invalid parent.
- [ ] Derivation creates a new ID and records the parent ID.
- [ ] Non-overwrite Song write rejects occupied destinations without partial writes.
- [ ] Old/unknown persistence version resets safely to empty slots.
- [ ] No dynamic allocation is introduced in Phrase operations.

Combined hardware/UI:

- [ ] Phrase page remains readable in both themes at 240×135.
- [ ] Footer and `Alt+H` show the same active keys.
- [ ] A/B/C/D, role, length, source, parent, tracks, and `REF` are visible.
- [ ] Capture from Song works for 1, 2, 4, and 8 bars.
- [ ] Derive A→B works without modifying A.
- [ ] Write-to-Song works on an empty destination.
- [ ] Occupied destination rejection leaves Song unchanged.
- [ ] Save/load restores slots after Scene codec integration.
- [ ] Stop, mute, route, audio, MIDI clock, and playback behavior are unchanged.
