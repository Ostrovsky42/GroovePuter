# 0.9.9-P1b — Phrase KEEP / canonical Undo backing ownership

## Purpose

P1b closes the ownership gap discovered by P1a/P1a2: a Phrase audition candidate can become a persistent `PhraseSlot`, but its physical `Pattern` backing must participate in the same canonical one-slot Undo/Redo mutation as the `PhraseBank` references.

This checkpoint adds a production KEEP transaction callable by a later UI stage. It does not add Phrase Lab controls.

Stack base:

- branch: `agent/20260823-06-0.9.9-p1a2-pattern-lease-generalization`
- SHA: `4c570acc0620ee4f1e03c01807ff3749957ecb0a`

## Before ownership problem

Before P1b the canonical Phrase Undo payload retained only the prior `PhraseCore::PhraseBank`:

```text
candidate Pattern lease
        |
        | KEEP
        v
PhraseSlot refs become persistent
        |
        | Ctrl+Z
        v
PhraseBank refs exchange back
```

The physical candidate bytes were outside Phrase Undo ownership. Clearing them immediately on Ctrl+Z would break Redo. Never clearing them would leave orphan non-empty Pattern bytes after the one-slot Undo receipt was superseded.

Both are incorrect.

## Source audit

### Canonical UndoOwner

`src/state/undo_owner.h` owns one `BoundedUndoSlot<1536>` and one committed revision. `togglePrepared()` is exchange-style: the retained payload and live value swap, then the same slot becomes the inverse transition. There is no second resident redo payload.

This exchange model determines the P1b solution: physical backing needed by the opposite side of the retained pair must remain available until the canonical receipt expires or is superseded.

### PhraseUndoPayload

`src/state/undo_receipts.h` keeps the Phrase domain receipt bounded to:

- page identity;
- one fixed `PhraseBank` before-state.

It does not contain a `Scene` and does not contain `SynthPattern` or `DrumPatternSet` snapshots.

### Existing generated-Phrase-to-Song receipt

`src/dsp/generated_phrase_song.h` already has `GeneratedPhraseUndoPayload` with Song before-state plus generated range metadata (`pageIndex`, `firstLocalSlot`, `bars`, etc.). Its current `restoreUndo()` clears the generated physical range when restoring Song state.

That model is useful evidence that range/reference metadata is sufficient to identify physical generated material, but its immediate-clear Undo semantics are not directly reusable for Phrase KEEP because Phrase Undo is exchange-style and must support the second Ctrl+Z as Redo.

### Phrase mutations and Ctrl+Z

Existing Phrase capture/derive/clear used canonical `UndoKind::Phrase` and `togglePrepared<PhraseUndoPayload>()`. P1b preserves that owner and routes those Phrase commits through the same backing-aware publication helper so a later Phrase mutation can safely supersede a KEEP receipt.

### Generation revisions

`UndoOwner::commitPrepared*()` captures the pre-mutation `SceneRevisionState`, applies one synchronous persistent mutation, calls `markSceneMutated()` once, and publishes one committed revision. Undo/Redo restores/exchanges the corresponding revision state using the existing contract.

### Pattern references

`SongPatternMaterializer` already counts Song and Phrase references. P1b separates:

- `persistentGlobalPatternReferenceCount()` — actual Song + Phrase references;
- `globalPatternReferenceCount()` — persistent references plus retained canonical Undo backing.

Allocators use the effective count. Cleanup/persistence sanitation use persistent references plus explicit ownership checks.

### Page persistence

`PatternPagingService` writes raw Synth A, Synth B and Drum banks. Therefore redo-only runtime backing must not silently become persisted user material.

### P1a2 PatternLease transfer

P1b consumes the P1a2 frozen sequence:

```text
preparePersistentTransfer()
canonical persistent mutation
completePersistentTransfer()
```

`completePersistentTransfer()` remains Scene-independent and bookkeeping-only after a correct prepare + commit.

## Chosen model B

P1b uses model B:

> Physical Pattern backing stays in Pattern storage while the canonical one-slot Undo receipt retains a transition that can require that backing for Redo.

The receipt stores only bounded ownership metadata: global Pattern addresses plus track masks. It does **not** copy physical Pattern payloads.

No second Undo stack, hidden Phrase history, Scene snapshot, or backing heap allocation is introduced.

## Canonical lifecycle metadata

`BoundedUndoSlot` reserves a fixed 112-byte tail inside the existing 1536-byte payload byte array when a receipt needs resource lifecycle ownership.

The lifecycle metadata contains:

- cleanup context;
- cleanup callback;
- detached-persistence sanitation callback;
- at most 20 `{globalPattern, trackMask, kind}` resource records;
- page identity.

The same metadata is republished unchanged by `togglePrepared()`, so Undo and Redo retain the same physical backing ownership.

Normal receipts without lifecycle metadata retain the original 1536-byte payload capacity. Backing-aware receipts have 1424 bytes available for their ordinary payload.

## KEEP transaction

`src/phrase/phrase_keep.h` exposes the production transaction used by future P1c UI.

For a valid active 1/2/4-bar lease:

1. validate target slot / role / lease shape;
2. call `preparePersistentTransfer()` before any persistent Phrase reference exists;
3. capture the canonical `PhraseUndoPayload` and all generated backing already owned by the before-bank;
4. prepare the new `PhraseBank` with `Source::Generated`, `StorageMode::ReferenceView`, mutable backing and the lease track mask;
5. add the accepted lease addresses/mask to the same canonical lifecycle metadata;
6. publish one `UndoKind::Phrase` receipt through `commitPhrasePrepared()`;
7. synchronously install the prepared `PhraseBank`;
8. call frozen P1a2 `completePersistentTransfer()` as bookkeeping-only completion;
9. publish exactly one Scene revision through the canonical owner.

No KEEP UI is part of P1b.

## Undo

First Ctrl+Z uses the existing Phrase exchange:

```text
live PhraseBank (kept)
        <->
retained PhraseBank (before KEEP)
```

After the exchange:

- previous Phrase state is exact;
- KEEP references disappear if they did not exist before;
- physical accepted backing remains in Pattern storage;
- lifecycle metadata remains attached to the same canonical Undo slot;
- allocators treat retained backing as occupied.

P1b deliberately does not clear redo-required backing on first Undo.

## Redo

Second Ctrl+Z performs the same exchange in the opposite direction.

Because physical bytes were retained and allocator-protected, the kept Phrase returns with the same Pattern addresses and exact material. No physical snapshot reconstruction is required.

The one-slot exchange behavior and `nextIsRedo()` semantics remain canonical.

## Supersede / expiry

When a later canonical mutation publishes a new receipt, the old lifecycle is retired only after the new receipt has been admitted/published.

This ordering matters: if the new receipt also owns one of the same backing resources, cleanup sees the new canonical owner and does not destroy shared backing.

On revision mismatch / expiry, canonical `UndoOwner::clear()` retires the retained lifecycle and performs the same reference-safe cleanup.

## Reference-safe cleanup

`src/phrase/phrase_undo_backing.h` clears one retained track only when all of the following are false:

1. Song or Phrase persistent reference exists;
2. current canonical Undo/Redo lifecycle still retains the address/mask;
3. active `PatternLeaseOwner` leases the address/mask.

Cleanup is per track mask. Shared Synth A, Synth B or Drums material is never cleared merely because another track at the aligned global address became reclaimable.

This is bounded over at most 20 lifecycle records and the existing three editable tracks.

## Allocator interaction

A redo-only physical Pattern may have zero Song/Phrase refs and still be live ownership.

Therefore allocator safety uses `globalPatternReferenceCount()`, which adds canonical retained backing to the persistent Song/Phrase count. Both Song materialization and P1a2 lease acquisition consequently skip retained redo backing.

No second allocator or reservation arena is introduced.

## PatternLease interaction

P1a2 remains the allocation/temporary-ownership dependency:

- canonical track-mask bits unchanged;
- 1/2/4 counts unchanged;
- masked collision semantics unchanged;
- masked discard unchanged;
- `preparePersistentTransfer()` ordering unchanged;
- `completePersistentTransfer()` remains Scene-independent;
- `PatternLease` remains 14 bytes;
- `PatternLeaseOwner` remains 28 bytes.

A failed KEEP before canonical commit leaves the lease active and creates no Undo revision. The caller can retry or discard it.

The P1a2 **PAGE PIN** requirement remains. P1b does not add page-switch/project-save UI ownership for an active audition lease. An active lease is runtime-only and must be resolved before a physical page replacement; P1c must enforce that boundary. P1b does not serialize lease metadata.

## Persistence

### PhraseBank

Accepted Phrase slots continue through the existing Phrase persistence schema. P1b adds no Phrase persistence fields. A kept Phrase therefore saves/loads as its normal metadata + Pattern references.

### Raw `.gpp` page

When canonical Undo owns redo-only backing, `PatternPagingService::savePage()` copies physical banks into the existing transaction scratch Scene and calls `UndoOwner::sanitizeForPersistence()` on that detached copy before writing it.

- accepted backing with a live Song/Phrase ref remains in the persisted copy;
- redo-only unreferenced backing is cleared only in the detached copy;
- live runtime backing is untouched and Redo remains possible.

A successful page load validates the file first, then retires canonical Undo lifecycle before replacing physical banks. A failed load leaves the runtime receipt intact.

### Scene JSON / project save

The project-level Scene writer was separately audited. It previously wrote live pattern banks directly, which would have serialized redo-only backing even though `.gpp` was sanitized.

P1b fixes that concrete defect without a schema change:

- `SceneManager::writeSceneJson()` creates a detached `sceneTransactionScratch()` view when canonical lifecycle exists;
- it sanitizes only that view;
- only physical Drum/Synth bank serialization reads the detached view;
- PhraseBank and all normal Scene semantics continue to serialize normally.

Both validated Scene JSON load paths retire canonical Undo immediately before assigning the validated replacement Scene. Parse/validation failure occurs before that point and does not consume the retained pair.

### Reboot/runtime semantics

`UndoOwner`, lifecycle metadata and `PatternLeaseOwner` are runtime services and are absent from Scene/Phrase persistence schemas. A reload/reboot does not restore an Undo/Redo pair.

The saved persistent side survives: accepted Phrase refs and their referenced backing load normally; a redo-only orphan is removed from the serialized physical view.

## Memory budget

Canonical resident budget remains one Undo owner. No `4 × 1416 B` physical copy is introduced.

Host ABI measurements are enforced by `tests/measure_0_9_9_p1b_memory.cpp`:

| Item | Size / capacity |
| --- | ---: |
| `BoundedUndoSlot<1536>` | 1548 B |
| `UndoOwner` | 1552 B |
| ordinary payload capacity | 1536 B |
| lifecycle-aware payload capacity | 1424 B |
| reserved lifecycle tail | 112 B |
| `UndoLifecycleMetadata`, 64-bit host | 112 B |
| `UndoLifecycleMetadata`, 32-bit ESP32 ABI | 96 B |
| `PatternLease` | 14 B |
| `PatternLeaseOwner` | 28 B |

`PhraseUndoPayload` remains a bounded PhraseBank receipt; the measurement test records its exact compiler size. It does not contain physical Pattern arrays.

The lifecycle record count is fixed at 20, sufficient for four Phrase slots × up to four generated bars plus accepted backing within the one-slot owner, without heap allocation.

Cardputer fixed-DRAM acceptance is a mandatory final CI gate.

## Failure behavior

### KEEP preparation failure

If lease validation, transfer preparation, Phrase preparation, backing-capacity admission, or canonical Undo admission fails before commit:

- PhraseBank is unchanged;
- no revision is published;
- no new Undo receipt is published;
- lease remains active;
- candidate can be retried/discarded.

### Transfer completion

P1b relies on the frozen P1a2 guarantee: after successful `preparePersistentTransfer()` and successful synchronous canonical persistent mutation, `completePersistentTransfer()` has no Scene/persistence-dependent failure path. It only validates the prepared token and releases temporary lease bookkeeping.

### Undo context unavailable

Existing canonical context/revision validation is preserved. Wrong page/context does not silently consume a valid exchange pair.

### Persistence failure

Sanitation operates on detached data. A failed save does not mutate live redo backing. A failed Scene JSON parse does not clear the current Undo pair.

## Non-goals

P1b does not implement:

- Phrase Lab screen;
- A/B audition controls;
- TAKE;
- REROLL UI;
- EVOLVE NEXT;
- new DERIVE behavior;
- Pattern Picker changes;
- P1a2 lease redesign;
- F08.1;
- a new scheduler;
- a second Undo/history owner.

## Validation

Focused/cumulative runner:

```bash
bash tests/run_0_9_9_p1b_tests.sh
```

Final checkpoint also requires:

```bash
bash tests/run_host_tests.sh
cd platform_sdl && make clean all CXX=g++
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

## Acceptance checklist

- [ ] KEEP length 1 passes.
- [ ] KEEP length 2 passes.
- [ ] KEEP length 4 passes.
- [ ] KEEP completes P1a2 transfer exactly once.
- [ ] Failed KEEP leaves lease active and publishes no revision.
- [ ] Discard-before-KEEP creates no Undo.
- [ ] First Ctrl+Z restores exact previous PhraseBank.
- [ ] Redo returns exact kept Phrase refs and exact physical material.
- [ ] Retained redo backing cannot be reused by allocators.
- [ ] Superseded/expired receipt reclaims only unreferenced backing.
- [ ] Shared Song/Phrase/lease/Undo ownership protects backing.
- [ ] New canonical mutation replaces retained pair safely.
- [ ] Accepted Phrase survives Scene persistence roundtrip.
- [ ] Redo-only backing is absent from `.gpp` and Scene JSON persisted views.
- [ ] Runtime Undo/Redo lifecycle is not serialized.
- [ ] P1a cumulative tests pass.
- [ ] P1a2 cumulative tests pass.
- [ ] Phrase Core and canonical Undo regressions pass.
- [ ] Full host regressions pass.
- [ ] SDL build passes.
- [ ] Cardputer ADV build passes.
- [ ] fixed DRAM gate passes.
- [ ] SEQTRAK MIDI-only build passes.
- [ ] no heap introduced in KEEP/backing ownership.
- [ ] no physical Pattern snapshot added to resident Undo.
- [ ] no UI / TAKE / REROLL / EVOLVE / F08.1 work included.
