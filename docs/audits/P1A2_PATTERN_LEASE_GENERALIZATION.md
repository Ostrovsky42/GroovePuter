# 0.9.9-P1a2 — Pattern Lease Generalization

## Purpose

P1a2 generalizes the accepted P1a storage owner so the same `PatternLeaseOwner` can serve both future Phrase Lab audition and future Song Pattern Picker audition.

Exact stacked base:

- branch: `agent/20260823-02-0.9.9-p1a-phrase-audition-leases`
- SHA: `b82c8d08b4cb8494f5b54c530a5613d2fc24bdc7`

This checkpoint changes only temporary physical Pattern ownership semantics. It does not add Phrase KEEP, Phrase Undo changes, Phrase Lab UI, Pattern Picker UI, EVOLVE, DERIVE, RELATED, F08.1, or musical generation.

## Current contract audit

P1a used one aligned address as implicit ownership of all three editable physical tracks:

- Synth A
- Synth B
- Drums

That was correct for Phrase audition, but too strong for future Song Pattern Picker, where one selected Song track may need a temporary candidate without touching the other aligned tracks.

The repository already has the canonical mask representation:

- `SongPatternMaterializer::kSynthAMask`
- `SongPatternMaterializer::kSynthBMask`
- `SongPatternMaterializer::kDrumsMask`
- `SongPatternMaterializer::kEditableTrackMask`

`PhraseCore::kTrackSynthA`, `kTrackSynthB`, `kTrackDrums`, and `kAllTracks` use the same bit positions. P1a2 compile-time asserts that the two existing contracts remain identical. No new track enum or duplicate bit assignment is introduced.

The repository also already owns the required per-track safety predicates:

- `SongPatternMaterializer::slotContentIsEmpty()`
- `SongPatternMaterializer::globalPatternIsReferenced()`

The latter includes both Song references and Phrase references for the requested track. P1a2 reuses these predicates inside the single lease owner; it does not create `SongPatternLeaseOwner` or another allocator.

## Ownership

`PatternLease` remains a fixed address-only value and now records `trackMask` explicitly.

A lease owns the Cartesian set:

`{ leased global addresses } x { requested track bits }`

It does **not** own unrequested aligned tracks at the same global address.

Examples:

- Phrase audition: `trackMask = SynthA | SynthB | Drums`
- Song SynthA preview: `trackMask = SynthA`
- Song SynthB preview: `trackMask = SynthB`
- Song Drums preview: `trackMask = Drums`

There is still exactly one `PatternLeaseOwner`, with two fixed active records and no heap storage.

## Acquire

Generic acquire:

```cpp
acquire(scene, pageIndex, count, trackMask, lease, preferredLocalSlot)
```

The P1a all-track overload remains for cumulative compatibility and forwards to the generic call with `SongPatternMaterializer::kEditableTrackMask`.

Supported counts remain only 1 / 2 / 4. Eight-bar audition remains unsupported.

For every candidate global address and every requested track bit, acquire requires:

1. the physical track slot is empty;
2. the same track/global address has no Song reference;
3. the same track/global address has no Phrase reference;
4. no active lease already owns an overlapping track bit at that global address.

Unrequested tracks are ignored by the safety test. Therefore an occupied or referenced Synth B at `5B3` does not prevent a SynthA-only lease at `5B3`, provided Synth A itself is safe.

Allocation remains transactional: failure to find all requested addresses leaves `Scene`, the output lease, and owner records unchanged.

## Masked collision rule

Lease collision is defined by **address plus overlapping mask**, not address alone.

Allowed:

- lease 1: SynthA @ address X
- lease 2: SynthB @ address X

Rejected/skipped:

- lease 1: SynthA @ address X
- lease 2: SynthA|SynthB @ address X

This is required because unrequested tracks are not claimed. The owner capacity remains two records; P1a2 does not widen the bounded concurrency budget.

## Reuse

Reroll/replacement of an active lease requires the same:

- page
- count
- track mask
- lease token

The exact global addresses are returned again even after candidate bytes have been written. `preferredLocalSlot` is irrelevant on reuse.

Only references on owned/requested tracks invalidate reuse. References on unrequested aligned tracks do not steal the lease.

## Discard

`discard(scene, currentPage, lease)` validates the active token, original page, address mapping, and absence of persistent references on the owned mask.

It then clears **only the tracks in `lease.trackMask`**.

Examples:

- all-track Phrase lease -> clears Synth A + Synth B + Drums;
- SynthA-only lease -> clears Synth A only;
- SynthB-only lease -> clears Synth B only;
- Drums-only lease -> clears Drums only.

Unrequested physical bytes and references at the aligned address are preserved exactly.

A requested track that unexpectedly became persistently referenced causes `PersistentReference`; the lease remains active and referenced material is not deleted.

## Two-phase persistent transfer

P1a's single post-commit `transferCommittedOwnership()` was fallible because it inspected persistent references after the canonical commit. That is the wrong transaction boundary: once the canonical owner commits successfully, temporary bookkeeping must not introduce a second failure that leaves ownership ambiguous.

P1a2 replaces it with:

```text
preparePersistentTransfer(...)
        |
        v
canonical Song/Phrase persistent mutation
        |
        v
completePersistentTransfer(...)
```

### preparePersistentTransfer

Prepare validates all lease-side preconditions before persistent mutation:

- active/current lease token;
- leased page still current;
- every global address still maps to that page;
- track mask remains valid;
- no requested track/global address is already persistently referenced.

Prepare writes only a fixed `PreparedPersistentTransfer` token. It does not mutate Song, PhraseBank, Scene revision, Pattern bytes, or lease ownership.

The no-reference check intentionally proves ordering: prepare must happen **before** the canonical persistent mutation.

### Canonical persistent mutation

The existing Song or Phrase owner performs its normal mutation and owns its normal success/failure semantics.

P1a2 does not provide a shadow commit path and does not mutate persistent references itself.

If canonical commit fails, the caller does not call complete. The lease remains active and can be retried or discarded.

### completePersistentTransfer

Complete accepts only the active lease plus the exact prepared token. It performs temporary owner bookkeeping only:

- verify token identity against the still-active bounded record;
- deactivate the temporary record;
- clear the caller's lease handle;
- do **not** clear physical Pattern bytes.

Complete intentionally receives no `Scene` and performs no Song/Phrase reference check, persistence check, or physical-content check. Therefore after a successful prepare and successful canonical commit there is no post-commit persistent-state failure point.

Misuse with an invalid/stale token may still return `InvalidTransfer`/`InvalidLease`; the promised no-failure property applies to the valid synchronous sequence produced by successful prepare followed by successful canonical commit.

## No Phrase KEEP

No Phrase KEEP is wired in P1a2.

The P1a Undo blocker remains unchanged: canonical Phrase Undo currently restores the fixed `PhraseBank` but does not own cleanup of transferred physical backing. A future KEEP checkpoint must first extend/prove canonical rollback ownership for that backing.

P1a2 supplies only the generic lease and transaction primitive needed by that later work.

## Persistence and PAGE PIN

Runtime lease records and prepared-transfer tokens are not stored in `Scene`, Song, `PhraseBank`, Phrase persistence, or generation provenance.

Pattern page persistence still writes raw physical Pattern banks. Therefore an active candidate must not survive an operation that persists or replaces the leased raw page without explicit lifecycle handling.

**PAGE PIN requirement for future UI:** while a lease is active, future Phrase Lab / Song Pattern Picker integration must keep the leased page pinned, or synchronously discard/complete the lease before any page switch/save path that can write or replace raw Pattern page storage.

P1a2 does not implement page-switch UX, serializer exclusions, or a new paging scheduler.

## Memory budget

No material arena is added.

Fixed state:

- `PatternLease`: 14 B, including `trackMask`;
- `PreparedPersistentTransfer`: 14 B caller-side token;
- `PatternLeaseOwner`: 28 B for two active records.

The owner still stores only four `int16_t` addresses per record and fixed metadata. There is no `vector`, smart pointer, `new`, `malloc`, or variable-capacity storage.

The known physical material remains in existing Pattern banks; it is not duplicated:

- Synth A: 112 B/bar
- Synth B: 112 B/bar
- Drums: 1192 B/bar
- aligned total: 1416 B/bar

## Failure behavior

- count other than 1/2/4 -> `UnsupportedLength`
- invalid page -> `InvalidPage`
- zero/unknown track bits -> `InvalidTrackMask`
- both owner records occupied -> `OwnerFull`
- insufficient safe masked addresses -> `Exhausted`, no mutation
- stale handle -> `InvalidLease`
- active reuse with changed page/count/mask -> `ShapeMismatch`
- discard/prepare on another page -> `PageMismatch`
- requested track becomes persistent before discard/reuse/prepare -> `PersistentReference`
- stale/mismatched prepared token -> `InvalidTransfer`

No failure path performs musical generation.

## Tests

Focused P1a2 coverage includes:

- all-track P1a compatibility;
- SynthA-only, SynthB-only, and Drums-only acquire;
- mixed requested/unrequested occupancy;
- requested referenced track rejection;
- unrequested referenced track preservation;
- mask-scoped discard;
- exact address+mask reroll reuse;
- disjoint-mask same-address coexistence and overlapping-mask collision;
- prepare validation;
- failed canonical commit leaving lease active;
- successful prepare + canonical commit + complete;
- complete preserving accepted bytes;
- invalid masks;
- no heap allocation;
- fixed-size assertions.

The cumulative P1a suite remains part of the P1a2 runner.

## Build / validation

Focused cumulative lease suite:

```bash
bash tests/run_0_9_9_p1a2_tests.sh
```

Repository gates:

```bash
bash tests/run_host_tests.sh
cd platform_sdl && make clean all CXX=g++
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

No hardware wiring changes are introduced.

## Acceptance checklist

- [ ] branch merge-base is exact P1a SHA `b82c8d08b4cb8494f5b54c530a5613d2fc24bdc7`
- [ ] exactly one `PatternLeaseOwner`; no `SongPatternLeaseOwner`
- [ ] canonical existing track-mask bits reused
- [ ] all-track P1a acquire compatibility PASS
- [ ] SynthA-only acquire PASS
- [ ] SynthB-only acquire PASS
- [ ] Drums-only acquire PASS
- [ ] unrequested occupied track does not block requested safe track
- [ ] requested Song/Phrase-referenced track is never leased
- [ ] unrequested referenced track is preserved
- [ ] discard clears only owned mask
- [ ] reroll reuses exact addresses and mask
- [ ] disjoint masked leases may share a global address
- [ ] overlapping masked leases cannot share owned track/address
- [ ] prepare changes no persistent owner and releases nothing
- [ ] failed canonical commit leaves lease active
- [ ] valid complete after successful canonical commit PASS
- [ ] complete clears no accepted Pattern bytes
- [ ] active lease PAGE PIN requirement documented
- [ ] no Phrase KEEP / Undo extension / UI / EVOLVE / DERIVE / generation delta
- [ ] no heap allocation
- [ ] `PatternLease == 14 B`
- [ ] `PreparedPersistentTransfer == 14 B`
- [ ] `PatternLeaseOwner == 28 B`
- [ ] focused P1a2 suite PASS
- [ ] cumulative P1a suite PASS
- [ ] `run_host_tests.sh` PASS
- [ ] SDL PASS
- [ ] Cardputer ADV PASS
- [ ] fixed DRAM PASS
- [ ] SEQTRAK MIDI-only PASS
- [ ] stacked PR remains Draft and unmerged
