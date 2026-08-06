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

## Commands

```text
Tab             PHRASE CORE / PHRASE ARRANGE
Left/Right      Previous/next position
Up/Down         Previous/next row of eight positions
1..4            Assign Phrase A/B/C/D
Backspace       Remove selected position
Shift+Backspace Clear the chain
W               Safe atomic write to Song
Alt+W           Explicit atomic overwrite
```

## Safety

A normal write validates the complete expanded destination before changing any Song row. Any occupied target returns `DestinationOccupied` with the destination unchanged.

Clearing a Phrase removes every arrangement reference to that slot. Persistence sanitization removes invalid arrangement entries while preserving the relative order of valid entries.

## Deferred

- generated arrangement templates;
- genre-aware `MAIN/VAR/BREAK/END` selection;
- musical mutation of derived Phrases;
- independent Phrase event ownership;
- arrangement playback without Song materialization;
- more than 16 positions.
