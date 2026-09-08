# Pattern/Phrase Runtime Lifetime Checkpoint — 2026-09-08

Authoritative base: `7ea609e678d3d3794b5e1d22838b999d90a5b28d`

## Scope

Close only Pattern/Phrase runtime semantic gaps needed for instrument usability and apply lifetime engineering to directly related state. Preserve the existing sequencer, generator, UI, GF2, transport, and mutation architecture.

## Invariants

- Pattern composition remains authoritative; runtime Pattern events are projection only.
- MAKE PHRASE is one-way materialization. Phrase becomes independent after publication.
- Pattern and Phrase are never two writable views of one array.
- Exactly one owner defines an event lifetime at any instant.
- Phrase duration is real musical time: 1/2/4/8 bars at 384 ticks/bar, 96 PPQN.
- Grid is edit/snap resolution only.
- Audio-visible mutations use the existing mutation/barrier contract.
- No persistent state is added for UI or GF2 convenience.
- No memory ceiling or threshold changes.

## Execution

1. Census the current Pattern/Phrase composition authority, runtime projection, material-slot state, active Phrase working state, pending activation, playback lifetime, editing, and source-switch contracts.
2. Write focused RED tests only for behavior that is genuinely absent. Required coverage: Pattern independence, MAKE PHRASE isolation, 1/2/4/8 temporal behavior, cross-bar note lifetime, Phrase edit playback, and Pattern↔Phrase switching.
3. Run the RED test on the exact test-only commit and record the failure as characterization evidence.
4. Implement the smallest production change that restores the semantic contract. Prefer existing representations; any new/increased persistent allocation requires an explicit residency justification.
5. Run focused tests, inherited P2/P3 regressions, host tests, Cardputer build, SEQTRAK build when shared production code changed, and static DRAM checks on one exact HEAD.
6. Record an ownership/lifetime table, memory delta, remaining risks, exclusions, and integration safety for GF2/UI workstreams.

## Lifetime review questions

For each touched persistent object, document: musical identity; sole owner; creation point; musical end; resident interval; whether representation may be shorter-lived; and behavior under source switch, Phrase-length change, transport stop/start, regeneration, MAKE PHRASE, project load, mute, and voice change.

## Non-goals

Lo-Fi/GF2 semantics, UI redesign, general memory optimization, framebuffer/display, TempoDelay, USB, FatFs, MIDI architecture, a second sequencer, a new generator, or allocator tricks.