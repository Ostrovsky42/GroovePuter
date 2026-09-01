# 0.9.9-P1a — Phrase Audition Pattern Lease Owner

## Purpose

P1a introduces one bounded runtime owner for temporary physical Pattern addresses used by a future audition-before-commit Phrase workflow.

This checkpoint owns **storage lifecycle only**. It does not add Phrase Lab UI, playback switching, musical generation, EVOLVE, DERIVE, trajectory logic, or 8-bar audition.

Exact base: `dev_0.9.9 @ 78bc8394ede5e6d81464cff5878c29bbf754c555`.

Supported lease lengths: **1 / 2 / 4** bars only.

## Current allocator audit

### Global address model

`scenes.h` defines:

- 16 logical Pattern slots per page (`2 banks * 8 slots`),
- 16 pages,
- 256 global Pattern addresses,
- one global address mapping to the aligned physical slot in Synth A, Synth B, and Drums.

`src/pattern/pattern_address.h` is the canonical page/bank/slot conversion layer.

### Current Song `G` path

`src/dsp/song_pattern_materializer.h` already owns the reference-aware Song generation allocation rules:

- `globalPatternReferenceCount()` scans both Song slots and Phrase references;
- `findReusableLocalSlot()` rerolls a uniquely-owned Song-generated destination in place;
- non-empty manual/imported material is not reclaimed;
- `findSafeFreeLocalSlot()` exposes the physically-empty + unreferenced safe-free rule.

P1a does not create a parallel notion of safe storage.

### Existing aligned Phrase placement

`src/dsp/phrase_generator.h` already defines the stronger aligned predicate needed here:

- `localSlotIsEmpty()` requires Synth A + Synth B + Drums to be empty at the same local slot;
- `globalPatternIsReferenced()` checks all three editable tracks through the Song materializer reference counter;
- `localSlotIsSafeForPhrase()` combines aligned physical emptiness with absence of Song/Phrase references;
- `findSafeContiguousEmptySlots()` is used by the existing generated-Phrase-to-Song path.

P1a reuses `localSlotIsSafeForPhrase()` directly. It does **not** require contiguity because the lease contract is a fixed list of up to four global addresses, not a new bank layout.

`SceneManager::findFirstFreePattern()` remains an older generic helper. P1a does not use it because the Song/Phrase generation helpers already provide the exact reference-aware aligned predicate.

### Current generated Phrase placement

`src/dsp/generated_phrase_song.h` currently prepares up to eight complete `PhraseBar` values, finds a safe contiguous physical range, then commits each bar into the aligned Synth A / Synth B / Drums slot and writes the same global address into all three Song tracks.

That path uses heap-backed PREPARE staging. P1a intentionally does not reuse that material arena: an audition lease stores addresses only.

## Ownership

`src/phrase/pattern_lease_owner.h` defines:

- `PatternLease`: up to four global addresses plus page/count/token state;
- `PatternLeaseOwner`: two fixed active lease records;
- `patternLeaseOwner()`: the single process-local owner entry point for future integration.

The capacity of two is deliberate: one normal audition can coexist with one independently prepared lease without address collision, while storage remains bounded. A third concurrent lease fails with `OwnerFull`.

The owner does not own musical generation. It only owns temporary exclusivity over safe physical addresses.

No address is permanently reserved or hidden from the user. Once discarded, an address immediately returns to the normal safe-free pool.

## Acquire

`acquire(scene, page, N, lease, preferredLocalSlot)`:

1. accepts only `N = 1, 2, 4`;
2. validates the current page;
3. scans at most the current page's 16 local Pattern slots;
4. rejects any slot already held by another active lease;
5. calls the existing `PhraseGenerator::localSlotIsSafeForPhrase()` predicate;
6. records the selected global addresses only after the complete request can be satisfied.

Failure is transactional: exhaustion, invalid shape, or owner-capacity failure does not partially activate a lease and does not mutate Scene material.

## Reuse

Calling `acquire()` again with the same active lease, page, and count returns the exact same addresses and sets `reusedExistingLease()`.

The reuse path intentionally does **not** require the physical slots to remain empty: after the first candidate is written, those bytes are expected to be non-empty. It instead proves that the lease token is still current and that no Song/Phrase persistent reference has appeared.

Therefore repeated future `G` replacement consumes no additional Pattern slots.

## Commit

P1a exposes `transferCommittedOwnership()` but does not wire a KEEP action into UI.

Transfer has a strict precondition: every leased global address must already have persistent references for aligned Synth A, Synth B, and Drums. Only then does the runtime lease release exclusivity **without clearing physical material**.

The persistent Song/Phrase mutation remains the canonical commit owner. The lease owner does not write persistent metadata and does not increment Scene revision.

This proves the no-copy ownership-transfer primitive required by future KEEP while preventing a transferred address from immediately becoming leasable again.

## Discard

`discard(scene, currentPage, lease)` is the only normal release operation.

Before clearing anything it proves:

- the caller holds the current lease token;
- the original page is still active;
- every address still maps to that page;
- no Song/Phrase persistent reference exists for any leased address.

It then resets aligned Synth A, Synth B, and Drum material to canonical default values and releases the owner record.

A stale or double release returns `InvalidLease` without mutation. If a persistent reference appeared unexpectedly, discard returns `PersistentReference` and keeps the lease active rather than deleting referenced material.

## Persistence

There is no lease field in `Scene`, `PhraseBank`, Song, generation provenance, or the Phrase persistence schema.

`src/phrase/phrase_persistence.h` serializes only the persistent PhraseBank and its `patternRefs`; an uncommitted audition never writes those references.

The physical Pattern bytes require stronger treatment:

- Scene JSON serializes the current physical Synth/Drum pattern content;
- `PatternPagingService::savePage()` writes raw `synthABanks`, `synthBBanks`, and `drumBanks` payloads into `.gpp` page files.

Therefore P1a does **not** add a serializer exception. DISCARD clears candidate bytes before releasing ownership, so a normally completed audition leaves canonical empty backing and no meaningful persistent reference.

Runtime lease state itself is never serialized.

A page switch while a lease is active is rejected by discard/transfer with `PageMismatch`. Future Phrase Lab integration must discard before changing the leased page or otherwise explicitly own page pinning; P1a does not add a scheduler or paging policy.

## Undo interaction

Current Phrase mutations use the canonical `UndoOwner` with `UndoKind::Phrase` and a fixed `PhraseUndoPayload` containing the 244-byte `PhraseBank`.

That receipt restores Phrase references, but it does **not** own physical Pattern cleanup.

The existing generated-Phrase-to-Song path is different: its `GeneratedPhraseUndoPayload` records the generated physical range, and Undo explicitly clears those physical bars before restoring Song.

This creates an exact future KEEP blocker:

1. audition lease owns candidate physical backing;
2. KEEP could commit new Phrase references through canonical `UndoOwner`;
3. `transferCommittedOwnership()` could then release the lease without a copy;
4. later Phrase Undo would remove the references but leave non-empty physical backing orphaned;
5. that backing would remain persistable and would no longer satisfy the safe-empty allocator predicate.

P1a therefore does **not** implement Phrase KEEP integration. Future P1b/KEEP must extend the canonical Undo contract so rollback also relinquishes transferred physical backing, or prove another canonical ownership path. A hidden second history/allocator is not introduced here.

## Memory budget

Known physical material per aligned bar:

- Synth A: 112 B
- Synth B: 112 B
- Drums: 1192 B
- total: 1416 B

A four-bar duplicate arena would cost 5664 B and is forbidden for P1a.

P1a fixed state:

- `PatternLease`: **14 B**
- `PatternLeaseOwner`: **28 B** for two records
- maximum address payload: four `int16_t` global addresses per lease

Both sizes are compile-time asserted. The owner contains no `vector`, smart pointer, `new`, `malloc`, or variable-capacity storage.

## Failure behavior

- 8 bars -> `UnsupportedLength`
- invalid page -> `InvalidPage`
- two active owners already in use -> `OwnerFull`
- fewer than N safe addresses -> `Exhausted`, no mutation
- stale/double lease handle -> `InvalidLease`
- different N/page while reusing -> `ShapeMismatch`
- page changed before discard/transfer -> `PageMismatch`
- persistent reference appears before discard/reuse -> `PersistentReference`
- ownership transfer without aligned persistent A/B/Drum references -> `IncompletePersistentOwnership`

No failure path performs musical generation.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3 for firmware build validation.
- Yamaha SEQTRAK is needed only for the existing MIDI-only compile target; P1a adds no MIDI behavior.
- Host compiler for permanent lifecycle tests.

## Wiring

No wiring changes. P1a adds no GPIO, I2C, SPI, MIDI electrical, audio, or voltage dependency.

## Build / Flash steps

Focused host suite:

```bash
bash tests/run_0_9_9_p1a_tests.sh
```

Full regression matrix remains the repository's existing PR matrix:

```bash
bash tests/run_host_tests.sh
cd platform_sdl && make clean all CXX=g++
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

No hardware flash is required to validate this storage-only checkpoint.

## Expected behavior

- acquiring 1/2/4 returns unique, safe global addresses;
- Song/Phrase referenced addresses are never leased;
- active leases cannot collide;
- repeated candidate replacement reuses the original addresses;
- discard clears physical candidate material and returns addresses to the pool;
- transfer only succeeds after persistent aligned ownership exists;
- no lease state appears in persistent Scene/Phrase metadata;
- no musical generation behavior changes.

## Troubleshooting

- **Address with Song/Phrase reference is leased:** reject the build; the owner bypassed the canonical safe predicate.
- **Second `G` consumes more slots:** caller created a new lease instead of reusing the active handle.
- **Discard leaves non-empty backing:** reject the build; raw page persistence can preserve that orphan material.
- **Discard deletes a committed Pattern:** reject the build; `PersistentReference` validation failed.
- **Transfer succeeds with only one referenced track:** reject the build; aligned backing ownership is incomplete.
- **Phrase Undo leaves transferred backing after future KEEP integration:** this is the documented P1a blocker; do not bypass it with hidden ownership.

## Acceptance checklist

- [ ] branch starts exactly from `78bc8394ede5e6d81464cff5878c29bbf754c555`
- [ ] no F08 ancestry
- [ ] focused P1a host suite PASS
- [ ] acquire 1 PASS
- [ ] acquire 2 PASS
- [ ] acquire 4 PASS
- [ ] 8-bar acquire rejected
- [ ] addresses unique
- [ ] Song referenced address never leased
- [ ] Phrase referenced address never leased
- [ ] simultaneous active leases never collide
- [ ] exhaustion produces no partial owner/Scene mutation
- [ ] discard clears candidate physical material
- [ ] released address returns to safe pool
- [ ] repeated acquire on active lease returns identical addresses
- [ ] transfer prevents committed backing from being re-leased
- [ ] stale/double release is safe
- [ ] owner performs zero heap allocations
- [ ] `PatternLease == 14 B`
- [ ] `PatternLeaseOwner == 28 B`
- [ ] no Scene/Phrase persistence schema change
- [ ] full host regressions PASS
- [ ] SDL build PASS
- [ ] Cardputer ADV build PASS
- [ ] fixed DRAM check PASS
- [ ] SEQTRAK MIDI-only build PASS
- [ ] no Phrase Lab UI / EVOLVE / DERIVE / musical generation delta
- [ ] PR remains Draft and unmerged
