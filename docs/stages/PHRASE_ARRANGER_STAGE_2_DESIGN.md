# Phrase Arranger Stage 2 design boundary

## Status

Experimental, stacked on `integration/genre-song-ui`. Do not merge into `dev`.

## Goal

Make the existing four Phrase slots useful as a visible composition form:

```text
A A B A C A B D
```

## Fixed model

- 16 arrangement positions;
- every position stores one Phrase slot index `A/B/C/D`;
- Phrase events remain reference-backed and mutable;
- an item expands using the Phrase's saved `1/2/4/8B` length;
- no heap allocation;
- no new event container;
- no DSP, transport, pattern generator or MIDI scheduler changes.

## Published memory contract

```text
Phrase Core PhraseBank: 244 bytes
PhraseArrangement:       18 bytes
Stage 2 PhraseBank:      262 bytes
Stage 2 fixed delta:     +18 bytes
```

`PhraseArrangement` is exactly 16 slot bytes plus `length` and `reserved`.
These structure sizes do not replace the repository-wide Cardputer ADV fixed
DRAM budget; the normal ELF gate remains mandatory.

## Commands

```text
Tab                 PHRASE CORE / PHRASE ARRANGE
Left/Right          Previous/next position
Up/Down             Previous/next row of eight positions
1..4                Assign Phrase A/B/C/D
Backspace           No destructive action in ARRANGE
Ctrl+Backspace      Remove selected ARRANGE position
Ctrl+Shift+Backspace Clear the complete ARRANGE chain
W                   Safe atomic write to Song
Alt+W               Explicit atomic overwrite
```

In PHRASE CORE, unmodified Backspace retains the existing clear-selected-Phrase
command. Ctrl-modified Backspace is consumed in CORE so a missed Tab cannot
clear the Phrase while the user intended to edit the arrangement.

## Safety

A normal write validates the complete expanded destination before changing any
Song row. Any occupied target returns `DestinationOccupied` with the destination
unchanged.

Clearing a Phrase removes every arrangement reference to that slot. Persistence
sanitization removes invalid arrangement entries while preserving the relative
order of valid entries.

The exact maximum boundary is `16 positions x 8 bars = 128 bars`, equal to the
Song capacity. Writing from row 0 must succeed exactly; writing the same chain
from row 1 must return `RegionOutOfRange` before any modification.

## Navigation versus reorder

Arrows navigate the arrangement cursor only. Stage 2 does not implement a
move/swap/reorder command. Changing `A A B` into `A B A` currently requires
reassigning the affected positions.

## Deferred

- move/swap/reorder commands;
- generated arrangement templates;
- genre-aware `MAIN/VAR/BREAK/END` selection;
- musical mutation of derived Phrases;
- independent Phrase event ownership;
- arrangement playback without Song materialization;
- more than 16 positions.
