# Phrase Arranger Stage 2 review response

Addressed before hardware testing:

- publish `PhraseBank=262 bytes`, `PhraseArrangement=18 bytes`, delta `+18`;
- make plain Backspace non-destructive in ARRANGE;
- use `Ctrl+Backspace` to remove one position and `Ctrl+Shift+Backspace` to clear;
- consume Ctrl-modified Backspace in CORE so a missed Tab cannot clear a Phrase;
- state that arrows navigate only and reorder is not implemented;
- add the exact `16 x 8B = 128B` Song boundary test;
- require full host, SDL, Cardputer ADV and fixed-DRAM GitHub Actions validation.
