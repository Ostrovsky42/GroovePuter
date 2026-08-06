# Phrase Arranger Stage 2 acceptance summary

This branch is an experimental acceptance vehicle stacked on `integration/genre-song-ui`.

## Automated

- fixed RAM/layout assertions;
- assign, replace, remove and clear operations;
- total-bar calculation;
- full-capacity handling;
- invalid/empty Phrase rejection;
- atomic safe Song write;
- explicit overwrite;
- out-of-range rejection;
- Phrase clear removes arrangement references;
- flat Phrase/arrangement persistence round-trip;
- Phrase page host compilation;
- full Scene round-trip.

## Hardware

Use `PHRASE_ARRANGER_STAGE_2_HARDWARE_TEST.md`.

The branch is not accepted until Cardputer ADV confirms:

- `Tab` opens and closes the arranger;
- `A A B A C A B D` can be entered and edited;
- total bars match saved Phrase lengths;
- normal write is atomic;
- `Alt+W` overwrites on Phrase without toggling Wave Overlay;
- Wave Overlay shortcut still works outside Phrase;
- recovery/save restores the chain;
- no new serial errors or audio instability appear.
