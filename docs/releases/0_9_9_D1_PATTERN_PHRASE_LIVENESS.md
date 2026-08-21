# GroovePuter 0.9.9-D1 — Pattern / Phrase Liveness Ownership

## Purpose

Freeze one liveness decision for reclaim/reuse of Pattern storage before live Phrase/Song arrangement is enabled.

Exact base: `dev_0.9.9 @ b3670ba608b5771e284dbbc1938a7d1278c3ce70` (merged 0.9.9-C).

D1 does not enable live Phrase or Song mutation during PLAY. It only closes the ownership prerequisite.

## Current ownership map

Persistent Pattern identities can be referenced by:

- `Scene.songs[0]` and `Scene.songs[1]` rows for Synth A, Synth B, and Drums;
- valid persisted `PhraseBank` `ReferenceView` slots whose source is `InternalPattern`, `Generated`, or `Derived`;
- generated ownership metadata used by `SongPatternMaterializer` to decide whether an unreferenced generated slot may be reused.

Runtime editor selection, current UI page, cursor position, and selected Pattern slot are navigation state and are not liveness roots.

## Canonical liveness owner

0.9.9-A already established `SongPatternMaterializer::globalPatternReferenceCount()` as the reference-count decision that combines Song and Phrase roots for a concrete track.

D1 removes the remaining private Song-only scan from `PhraseGenerator` and delegates its all-track question to that owner. Therefore both Song generation/reclaim and Phrase allocation use the same persisted reference semantics.

For Phrase allocation a global Pattern identity is live if any editable track reports a Song/Phrase reference.

## Reclaim algorithm

1. Reject invalid Pattern identity.
2. For each editable track (Synth A, Synth B, Drums), query the canonical Song + Phrase reference owner.
3. If any reference exists, the Pattern is live and cannot be reclaimed/reused by Phrase allocation.
4. Only an unreferenced slot may proceed to the existing emptiness/generated-ownership rules of the caller.
5. Reclaim/reuse never changes an unrelated Pattern identity.

D1 adds no queue, cache, history, bitmap, or heap-backed index.

## Persistence implications

No Scene schema changes.

- Song Pattern IDs remain persisted exactly as before.
- Phrase `ReferenceView.patternRefs` remain persisted exactly as before.
- generated ownership continues to use the existing Pattern paging marker.
- no liveness cache is serialized because liveness is derived from current persistent Scene truth.

Save/load, project switch, and backup-recovery compatibility continue to be covered by the existing 0.9.9-A Scene and PatternPaging tests, rerun by the D1 focused gate.

## Legacy compatibility

Legacy scenes without valid Phrase reference slots behave exactly as before: Song references remain liveness roots and unreferenced generated material can be reused according to existing allocator rules.

Invalid/stale Phrase slots are not promoted into roots; the canonical A-stage validation rules remain authoritative.

## Failure cases

- Phrase-only Pattern becomes reusable: FAIL; Phrase root was bypassed.
- Removing a Song reference reclaims a Pattern still referenced by Phrase: FAIL.
- Song + Phrase sharing is counted as unique ownership: FAIL; copy-on-write safety is broken.
- Unrelated Pattern becomes live because another identity is referenced: FAIL.
- New dynamic/history-sized storage appears: FAIL.
- Scene/paging format changes: FAIL unless separately designed; D1 requires none.

## Tests

Focused:

```bash
bash tests/run_0_9_9_d1_tests.sh
```

The runner covers:

- Song reference keeps Pattern alive;
- Phrase reference keeps Pattern alive;
- Song + Phrase shared reference;
- removing one reference while another remains;
- no references -> Pattern is no longer live;
- unrelated Pattern identity is unaffected;
- all valid persisted Phrase reference sources are roots;
- existing Song allocator Phrase-liveness/copy-on-write tests;
- realized Scene round-trip compatibility;
- generated ownership paging/recovery compatibility;
- 0.9.9-A identity/source regressions.

Normal PR CI must additionally keep Core host, SDL, Cardputer ADV normal/fixed-DRAM, SEQTRAK MIDI-only, Output Ownership, Device Profiles, Synth persistence, Undo R2-R7, and 0.9.9 A/B1/B2/C green on the same exact head.

## Memory

Production resident-state delta: **0 bytes by design**.

D1 adds only code/tests/docs. The liveness query derives state from the existing fixed `Scene`; there is no persistent cache and no allocation proportional to edit history.

## Hardware assumptions

No wiring or electrical behavior changes.

- Cardputer ADV remains ESP32-S3 target.
- MIDI/output ownership is unchanged.
- No PORT.A I2C, external display, or SEQTRAK wiring change is introduced.

## Build / flash

For software acceptance use the focused runner and normal repository CI on one exact SHA.

For physical smoke, flash the exact accepted D1 candidate using the repository's normal Cardputer ADV build/flash procedure.

## Expected behavior

There should be no visible UI change. Existing projects load normally. Song/Phrase-linked Patterns remain present after editing, save, reboot, and load.

## Troubleshooting

If a Phrase-only Pattern disappears, first inspect `PhraseBank` metadata validity and `patternRefs`; do not add an editor-selection workaround. If a Pattern cannot be reused after all persistent references are removed, inspect exact Song/Phrase references before changing allocator policy.

## Acceptance checklist

- [ ] D1 focused suite PASS on exact head
- [ ] Core host regressions PASS
- [ ] SDL PASS
- [ ] Cardputer ADV normal build PASS
- [ ] fixed DRAM gate PASS
- [ ] SEQTRAK MIDI-only build PASS
- [ ] Output Ownership / Device Profiles / Synth persistence PASS
- [ ] Undo R2-R7 + 0.9.9 A/B1/B2/C cumulative gates PASS
- [ ] Song reference keeps Pattern alive
- [ ] Phrase reference keeps Pattern alive
- [ ] shared Song + Phrase reference remains live after either one is removed
- [ ] zero persistent references permits existing reuse policy
- [ ] unrelated Pattern identity unaffected
- [ ] save/load and legacy compatibility preserved
- [ ] production resident-state delta 0 bytes
- [ ] old project physical smoke: Song/Phrase Patterns survive save/reboot/load

## Explicit non-goals

- live Phrase generation during PLAY;
- live Song generation/rearrangement;
- a new allocator or Pattern ID format;
- Scene schema migration;
- editor/page selection as a lifetime root;
- a second Undo/revision owner;
- a pending activation queue;
- a new scheduler or musical clock.
