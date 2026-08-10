# Generation Stage 11 — Melodic Rhythm and Motif Acceptance

## Purpose

Separate when Synth B speaks from the legacy pitch phrase and give repeated
material a stable, pitch-agnostic motif ordering identity.

## Ownership

- `MelodicMotifPlan` owns onset/continuation topology and source-event order.
- It cannot create pitch values, select register or choose a synth engine.
- The explicit semantic binder maps either Chord or Melodic role to physical
  Synth B; one physical monophonic lane never pretends to host both roles.
- Motif order reuses existing source events and cannot widen the pitch set.

## Acceptance

- valid bars may contain 0, 1, 2 or 3 melodic onsets;
- ten MelodicRhythm and five MotifShape identities are deterministic;
- motif changes source-event order without changing onset topology;
- long-tone continuation and role-relative Feel are explicit;
- Bass/Chord/protected-space collisions are removed;
- non-Dub production routes reach the explicit Melodic Synth B binding;
- AUTO uses append-only `MelodicRhythmSelection` and `MotifSelection` domains;
- GCC, Clang and ASan/UBSan pass;
- hardware listening remains `HARDWARE_PENDING`.
