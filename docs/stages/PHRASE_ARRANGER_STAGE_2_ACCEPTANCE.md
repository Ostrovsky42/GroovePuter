# Phrase Arranger Stage 2 acceptance summary

This branch is an experimental acceptance vehicle stacked on
`integration/genre-song-ui`.

## Published memory contract

```text
Phrase Core PhraseBank: 244 bytes
PhraseArrangement:       18 bytes
Stage 2 PhraseBank:      262 bytes
Stage 2 fixed delta:     +18 bytes
```

The repository-wide Cardputer ADV fixed-DRAM gate remains authoritative.

## Automated

- fixed RAM/layout assertions for `PhraseArrangement=18` and
  `PhraseBank=262` bytes;
- assign, replace, remove and clear operations;
- total-bar calculation;
- invalid/empty Phrase rejection;
- atomic safe Song write;
- explicit overwrite;
- Phrase clear removes arrangement references;
- flat Phrase/arrangement persistence round-trip;
- legacy pre-arranger Phrase payload migration;
- Phrase page host compilation;
- full Scene round-trip;
- exact maximum boundary: `16 x 8B = 128B`;
- row-0 exact-capacity write succeeds;
- row-1 exact-capacity write returns RANGE before any mutation;
- Cardputer input source gate for Core/Arrange Backspace separation.

## Required GitHub Actions evidence

The branch is not a hardware-test candidate until one user-triggered Actions run
passes all of the following on the committed tree:

- complete host regression suite;
- focused Phrase Core and Phrase Arranger tests;
- SDL build;
- Cardputer ADV build;
- fixed DRAM budget gate.

Connector-authored commits and PR state changes have not triggered Actions in
this repository, so no green CI claim is made yet.

## Hardware

Use `PHRASE_ARRANGER_STAGE_2_HARDWARE_TEST.md`.

The branch is not accepted until Cardputer ADV confirms:

- `Tab` opens and closes the arranger;
- `A A B A C A B D` can be entered and reassigned;
- arrows navigate positions only; no reorder command is claimed;
- plain Backspace in ARRANGE performs no destructive action;
- `Ctrl+Backspace` removes one arrangement position;
- `Ctrl+Shift+Backspace` clears the arrangement;
- Ctrl-modified Backspace in CORE does not clear the selected Phrase;
- total bars match saved Phrase lengths;
- normal write is atomic;
- `Alt+W` overwrites on Phrase without toggling Wave Overlay;
- Wave Overlay shortcut still works outside Phrase;
- recovery/save restores the chain;
- no new serial errors or audio instability appear.
