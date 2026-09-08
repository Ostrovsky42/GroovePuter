# 0.9.10 MEMORY-R1 WT1 — triangle wavetable caller census

Date: 2026-09-08

## Authoritative start

WT1 starts from the integrated FS1B hardware-accepted line:

- branch: `feature/20260908-0.9.10-memory-r1-dram-recovery`
- exact source HEAD: `3b2bd280e9c39d8d5cbdf425db6ef18126e650be`
- WT1 execution branch: `feature/20260908-0.9.10-memory-r1-wt1`

This checkpoint is limited to proving and removing the dead triangle wavetable representation. Saw and square representations are explicitly outside scope.

## Production caller census

The repository-level WT1 source census scans all C/C++/Arduino source and header files under `src/` for `lookupTriangle` and `triangleTable_`.

On the authoritative WT1 start it found exactly these six occurrences:

- `src/dsp/audio_wavetables.cpp`: static `triangleTable_` definition;
- `src/dsp/audio_wavetables.cpp`: first triangle initialization assignment;
- `src/dsp/audio_wavetables.cpp`: second triangle initialization assignment;
- `src/dsp/audio_wavetables.h`: inline `lookupTriangle` accessor definition;
- `src/dsp/audio_wavetables.h`: accessor read from `triangleTable_`;
- `src/dsp/audio_wavetables.h`: static `triangleTable_` declaration.

No occurrence exists in any other production source file. In particular, there is no production call site for `lookupTriangle` outside its own inline accessor definition.

Conclusion: the triangle wavetable is a self-contained dead representation, not a live waveform consumer path.

## TDD evidence

The first WT1 workflow run is deliberately RED:

- workflow: `0.9.10 MEMORY-R1 WT1`
- run: `34174949735`
- exact head: `b85480386013ffb8e91a46958c9ca0c3842e691c`

The same test first prints `WT1 caller census: PASS`, then fails because the dead triangle representation still exists. This distinguishes a proved no-caller census from a search that merely failed to find a use.

The RED guard also protects scope by requiring `sawTable_` and `squareTable_` to survive the WT1 removal.

## Acceptance order

1. caller census — PASS;
2. dead-representation RED — reproduced;
3. remove only triangle accessor/storage/initialization;
4. rerun focused guard — pending at the time this census was recorded;
5. same-base FS1B ELF before/after measurement;
6. short Cardputer ADV hardware smoke;
7. only then update total recovery accounting.

The expected static recovery is approximately one 1024-entry float table (~4096 B), but WT1 does not book that projection. Only the measured `.dram0.data + .dram0.bss` ELF delta is accepted.
