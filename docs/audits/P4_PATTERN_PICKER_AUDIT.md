# 0.9.9-P4R — Pattern Picker / Audition Integration Audit

## Status

Research / contract checkpoint only.

- Production base: `dev_0.9.9 @ 78bc8394ede5e6d81464cff5878c29bbf754c555`
- Research branch: `agent/20260823-05-0.9.9-p4-pattern-picker-audit`
- P1a reference branch inspected: `agent/20260823-02-0.9.9-p1a-phrase-audition-leases`
- Production semantics delta: **0**
- Pattern Picker implementation: **not in this checkpoint**
- New allocator / generator / persistence: **forbidden and not introduced**

The question for P4R is narrow:

> How should a future Song Pattern Picker reuse the P1a PatternLease owner without creating a second preview/generation allocator?

The answer is:

> **EXISTING preview requires no lease. GENERATE and RELATED must acquire temporary backing only through the P1a owner. P4 needs a small track-mask / two-phase-transfer extension of that same owner, because current P1a is aligned A+B+Drums and cannot transfer a lease after accepting one Song cell. Accept remains one canonical Song transaction; discard never mutates Song.**

This document audits the current base and defines the integration contract. It does not implement that contract.

---

## Scope and hard boundary

Audited on the exact production base:

- `scenes.h`
- `src/pattern/pattern_address.h`
- `src/ui/key_normalize.h`
- `src/ui/pattern_matrix_navigation.h`
- `src/ui/components/pattern_selection_bar.h`
- `src/ui/components/bank_selection_bar.h`
- `src/ui/pages/song_page.h`
- `src/ui/pages/song_page.cpp`
- `src/ui/pages/song_page_r4_owner.inc`
- `src/ui/pages/pattern_edit_page.h`
- `src/ui/pages/pattern_edit_page.cpp`
- `src/ui/pages/pattern_edit_page_legacy.h`
- `src/ui/help_dialog_frames.h`
- `docs/keys_music.md`
- `src/state/song_edit.h`
- `src/dsp/song_pattern_materializer.h`
- `src/dsp/phrase_generator.h`
- `src/phrase/phrase_types.h`
- `src/audio/pattern_paging.h`
- `scenes.cpp`

P1a reference inspected separately, without stacking it into P4R ancestry:

- `src/phrase/pattern_lease_owner.h`
- `docs/audits/P1A_PATTERN_LEASE_OWNER.md`

P4R does **not** create or change:

- `PatternLease`
- free-slot allocation
- musical generation
- Phrase Lab
- EVOLVE
- DERIVE
- F08.1
- persistence format/schema
- Song, Pattern, Phrase, Undo, paging, or playback production semantics

---

## Current flow

### 1. Song cell and global Pattern coordinates

`SongPosition::patterns[]` stores `int16_t` global Pattern IDs. The editable Song tracks are Synth A, Synth B, and Drums; Voice is a fourth Song track but is not a Pattern-materializer target.

The global Pattern matrix is fixed:

- `kMaxPages = 16`
- `kBankCount = 2`
- 8 slots per bank
- 16 Pattern addresses per page
- `kMaxGlobalPatterns = 256`

The canonical conversion is already centralized in `src/pattern/pattern_address.h`:

```text
PAGE 0..15
  x 2 BANKS A/B
  x 8 SLOTS 1..8
  = 256 global Pattern IDs
```

`PatternAddress { page, bank, slot }`, `patternAddressFromGlobal()`, `patternAddressToGlobal()`, and `formatPatternAddress()` are the correct address/navigation vocabulary for a future picker. P4 must not invent another address type.

A global address identifies aligned coordinates across three distinct physical stores on the resident page:

- `scene.synthABanks[bank].patterns[slot]`
- `scene.synthBBanks[bank].patterns[slot]`
- `scene.drumBanks[bank].patterns[slot]`

The same `PAGE/BANK/SLOT` number therefore does not imply one shared physical Pattern object. Track remains part of physical ownership.

### 2. Resident page and page navigation

Only one Pattern page is resident in the Scene pattern banks at a time. `SceneManager::setPage()`:

1. saves the previous resident page with `PatternPagingService::savePage()`;
2. loads the requested page if it exists, otherwise initializes and saves an empty page;
3. changes `currentPageIndex_` only after the page operation succeeds.

Song references themselves remain global IDs and can point to any of the 16 pages.

The existing pure shortcut decoder in `src/ui/pattern_matrix_navigation.h` maps:

- `Ctrl+1..8` -> pages 1..8
- `Ctrl+Fn+1..8` -> pages 9..16

Song Page calls `requestPageSwitch()` for those shortcuts.

**P4 consequence:** any temporary lease is resident-page-bound. A temporary candidate must not survive a page switch because leaving the page first saves raw physical Pattern bytes.

### 3. Song cursor and selection

Song Page already owns:

- cursor row;
- cursor track;
- rectangular selection;
- edit Song slot A/B;
- split A|B view;
- playback Song slot;
- track lane filtering.

Left/right moves among visible tracks and, at the edge, switches the editable Song slot. Up/down moves rows. Shift/Ctrl+arrows extend selection.

The first Pattern Picker implementation should anchor to **one current editable Pattern cell** `(songSlot, row, track)`. Multi-cell picker semantics are not required by P4R and would unnecessarily couple picker acceptance to area-fill behavior.

### 4. Q..I assignment

`qwertyToPatternIndex()` maps exactly:

```text
Q W E R T Y U I
0 1 2 3 4 5 6 7
```

In Song Page, plain Q..I computes:

```cpp
songPatternFromPageBankIndex(
    mini_acid_.currentPageIndex(),
    assignment_bank_index_,
    patternIdx)
```

and commits the resulting global ID through the canonical Song mutation owner.

For a selection, the same global ID is filled into every selected editable cell inside one Song transaction.

This existing direct assignment remains valid and must not be redirected through a future picker unless a separate UX decision explicitly changes it.

### 5. B / Alt+B / Ctrl+B

The current Song key ownership is already dense:

- plain `B`: toggle `assignment_bank_index_` A/B; runtime UI state only, no Song mutation;
- `Alt+B`: flip the **stored Song reference bank** for the current cell/selection; persistent Song mutation;
- `Ctrl+B`: switch/toggle playback Song slot A/B according to current live playback rules.

A future picker must not add another Song-level meaning to B. Inside a modal picker, B may safely mean candidate bank selection only if the event is consumed by the modal and never falls through to Song Page.

### 6. Enter navigation

Plain Enter on a populated Song Pattern cell is already **Quick Jump to Editor**:

- resolves the selected global Pattern reference;
- changes to its Pattern page;
- routes Synth A / Synth B / Drums to the corresponding editor page.

This is navigation to edit the referenced persistent Pattern. It is not preview-before-commit and should remain distinct from Pattern Picker.

### 7. G current-cell generation

Current Song `G` uses `SongPage::materializeSongTracks()` and `SongPatternMaterializer::generate()`.

The path combines three concerns in one successful action:

1. choose/reuse physical backing;
2. generate candidate material;
3. write the physical Pattern and immediately assign the Song reference.

`generateCurrentCellPattern()` captures the old Song reference, old Song length, and Scene revision before the action, then commits immediately.

This is **not** a preview lifecycle and is not the allocator P4 should call for temporary candidates.

### 8. Double-G row generation

Double-G is intentionally implemented as immediate first-tap commit plus gesture rollback:

1. first `G` generates the current cell immediately;
2. if a second `G` arrives within 300 ms, `rollbackPendingCellGeneration()` removes that first generated cell and restores the captured Song/revision state;
3. `generateEntireRow()` then generates Synth A + Synth B + Drums for the row.

The rollback receipt is specific to the double-tap gesture. It is **not** a general audition lease and must not be copied into Pattern Picker.

`Alt+G` similarly iterates generation over selected cells. R4 deliberately delegates generation gestures to the retained generation owner instead of converting them into normal Song Undo transactions.

### 9. Current Song generation copy-on-write / free-slot search

`SongPatternMaterializer` is the current production Song-generation allocator.

For each requested track it first tries to reroll the current generated destination in place only when:

- it belongs to the current page;
- it carries `kSongGeneratedOwnershipBit`;
- its global reference count for that track is exactly one.

Otherwise it scans the 16 local slots and accepts only:

- an unreferenced strictly empty slot; or
- an unreferenced orphan previously marked Song-generated.

It explicitly does not reclaim non-empty manual/imported material.

This logic is correct for **commit-time Song generation**, but the successful path always ends in a physical write + Song assignment. P4 must not fork `findReusableLocalSlot()`, `findSafeFreeLocalSlot()`, or `reusableSlotCount()` into a preview allocator.

### 10. Song and Phrase references

`SongPatternMaterializer::globalPatternReferenceCount()` protects:

- both Song slots;
- all Song rows;
- Phrase `ReferenceView` pattern references for valid Pattern-backed Phrase sources.

`PhraseCore::PhraseSlot::patternRefs[8][3]` stores persistent global Pattern IDs by bar and editable track.

Therefore a temporary-storage owner must treat both Song and Phrase references as persistent borrowers. P4 must not implement a smaller Song-only reference scan.

### 11. Undo / revision ownership today

Normal Song mutations in `song_page_r4_owner.inc` use `commitSongMutation()`:

1. capture `SongUndoPayload`;
2. prepare the complete `Song after` value;
3. reject no-op/unavailable target;
4. publish one `UndoKind::Song` receipt through `UndoOwner`;
5. commit the Song value under the existing audio/activation boundary.

`SongEdit::setPattern()` already owns clamping and Song length extension.

This is the correct future owner for Pattern Picker **accept**.

Current `G` is deliberately excluded from that path because generation has its own PREPARE/commit semantics. P4 must not change existing G while introducing picker audition later.

### 12. Pattern selector/navigation components available for reuse

The existing editor already has two reusable bounded UI components:

- `PatternSelectionBarComponent`
  - 8 Pattern slots;
  - selected index;
  - cursor index;
  - optional cursor;
  - callback on select.
- `BankSelectionBarComponent`
  - bounded bank count;
  - selected/cursor index;
  - callback on select;
  - already used with A/B labels.

They are reusable as **presentation/input widgets**, not as ownership components.

Pattern Editor callbacks currently mutate runtime engine pattern/bank selection directly. A Picker must supply picker-local callbacks instead; it must not reuse those editor mutation callbacks.

`PatternAddress` and `songPatternPageShortcut()` are also directly reusable.

There is no existing complete Pattern Picker state machine or accept/discard owner to reuse.

### 13. Current help/docs drift

Current help/source are not perfectly aligned:

- `docs/keys_music.md` says `Q..U` assign pattern; source is Q..I.
- Song help correctly says Q..I but describes `G` as `Generate new song`; current code generates cell / double-G row / Alt+G selection.
- some Song footers imply left/right changes Track/Bank, while the actual edge behavior switches editable Song slot rather than PAT assignment bank.

P4 implementation should update these surfaces only when the picker exists. P4R records the conflict; it does not edit user-facing key semantics.

---

## Ownership table

| Object / state | Current canonical owner | Future Picker rule |
|---|---|---|
| Global `PAGE/BANK/SLOT` conversion | `PatternAddress` helpers | Reuse unchanged |
| Resident Pattern page | `SceneManager` + `PatternPagingService` | Picker observes; lease pins one resident page logically |
| Song cell reference | `Song` / `SongEdit` | Change only on accept |
| Phrase Pattern reference | `PhraseCore::PhraseBank` | Read as persistent borrower; never changed by P4 |
| Direct Q..I assignment | Song Page + `commitSongMutation()` | Preserve existing behavior |
| PAT assignment bank | `SongPage::assignment_bank_index_` | Picker may have separate local bank cursor; do not overload owner |
| Existing Pattern bytes | current persistent Pattern backing | Read-only audition; no lease |
| Current Song `G` free-slot policy | `SongPatternMaterializer` | Preserve legacy G; do not reuse as preview allocator |
| Temporary audition addresses | P1a `PhrasePatternLease::PatternLeaseOwner` | **Single allocator for GENERATE/RELATED** |
| Candidate musical generation | current Atlas / Groovebox generation path | Reuse producer semantics; lease owns storage only |
| RELATED candidate creation | no qualifying production producer established by this audit | Keep unimplemented until an existing producer contract exists |
| Song accept transaction | R4 `commitSongMutation()` + `UndoOwner` | Reuse exactly once per accept |
| Temporary discard | P1a lease owner | Must leave Song/Phrase untouched |
| Song Undo/Redo | `UndoKind::Song` | Restore/reapply Song reference only |
| Accepted generated orphan reclaim class | existing `kSongGeneratedOwnershipBit` policy | Must be set through centralized promotion semantics, not raw P4 bit writes |
| Scene revision | canonical persistent commit owners | Preview/discard must not publish persistent revision |
| `.gpp` Pattern page persistence | `PatternPagingService` | Never serialize an active temporary candidate |
| Picker UI state | future Song Picker modal | Runtime-only, fixed/bounded, never persisted |

---

## P1a compatibility finding

The current P1a implementation is deliberately **Phrase-aligned**:

```cpp
AcquireResult acquire(
    const Scene& scene,
    int pageIndex,
    uint8_t count,
    PatternLease& lease,
    int preferredLocalSlot = 0);

LeaseStatus discard(
    Scene& scene,
    int currentPageIndex,
    PatternLease& lease);

LeaseStatus transferCommittedOwnership(
    const Scene& scene,
    int currentPageIndex,
    PatternLease& lease);
```

Its acquisition predicate calls `PhraseGenerator::localSlotIsSafeForPhrase()`, which requires the same local address to be physically empty for **Synth A + Synth B + Drums** and unreferenced across all editable tracks.

Its transfer precondition is even stronger: `hasCompletePersistentOwnership()` requires each leased global address to have a persistent reference for **all three editable tracks** before the lease is released.

That is correct for the P1a Phrase-aligned contract.

It is not sufficient for Song Pattern Picker:

```text
selected Song cell = one (row, track)
accept             = one persistent track reference
```

A Synth A cell accept must not manufacture Synth B and Drum references merely to satisfy P1a transfer.

The correct integration is therefore **not** a P4 allocator. The correct integration is a small track-mask generalization of the existing P1a owner.

---

## Future picker state machine

The recommended future state machine is bounded and single-cell-scoped.

```text
CLOSED
  |
  | Alt+Enter on editable Song Pattern cell
  v
OPEN / EXISTING
  |  browse page/bank/slot
  |  preview persistent backing (NO LEASE)
  |------------------------------------ Enter --------------------> ACCEPT SONG REF -> CLOSED
  |------------------------------------ Esc ----------------------> CLOSED
  |
  +--> GENERATE
  |      |
  |      | acquire P1a lease(count=1, trackMask=current track)
  |      v
  |   LEASED CANDIDATE
  |      |  generate prepared material using existing producer
  |      |  write only leased track backing
  |      |  repeated candidate -> SAME lease/address
  |      |----------------------------- Enter --------------------> PREPARE TRANSFER
  |      |                                                           |
  |      |                                                           v
  |      |                                                   SONG ASSIGN COMMIT
  |      |                                                           |
  |      |                                                           v
  |      |                                                   COMPLETE TRANSFER
  |      |                                                           |
  |      |                                                           v
  |      |                                                         CLOSED
  |      |
  |      +----------------------------- Esc/tab-away -------------> DISCARD LEASE -> OPEN/CLOSED
  |
  +--> RELATED
         |
         | same P1a lease lifecycle as GENERATE
         | no implementation until an existing related producer exists
         v
      LEASED CANDIDATE
```

### State invariants

#### CLOSED

- no picker lease;
- Song Page behavior unchanged.

#### EXISTING

- **no lease**;
- highlighted candidate is an existing persistent global Pattern ID;
- preview is read-only;
- page changes are allowed because no temporary bytes exist;
- accept changes only the Song reference.

#### GENERATE / RELATED with active candidate

- exactly one active P1a lease for the picker;
- `count = 1`;
- lease track mask is exactly the focused editable Song track;
- candidate page equals the resident page;
- candidate replacement reuses the same lease rather than consuming another slot;
- page switching is disabled until accept/discard;
- Song cell retains its old reference until accept.

#### ACCEPT

- Existing candidate: one Song transaction, no lease handoff.
- Leased candidate: prepare lease transfer, execute one Song transaction, complete lease transfer inside the successful synchronous commit path.
- no second persistent Song write.

#### DISCARD

- Existing candidate: close/leave mode only.
- Leased candidate: P1a discard clears only leased-track temporary backing and releases the lease.
- Song/Phrase refs and Song Undo history are untouched.

---

## Required P1a API

### What P4 can reuse as-is

P4 should reuse:

- `PhrasePatternLease::PatternLeaseOwner` singleton ownership model;
- fixed owner capacity;
- lease generation/token validation;
- page binding;
- fixed address list;
- repeated-acquire reuse behavior;
- `discard()` lifecycle concept;
- no lease persistence;
- canonical Song/Phrase reference awareness.

P4 must **not** create a sibling `SongPatternLeaseOwner`, a second free-slot scan, or a hidden preview arena.

### Required extension: track-scoped lease shape

The same P1a owner needs an equivalent target API with `trackMask` carried by the lease:

```cpp
namespace PhrasePatternLease {

struct PatternLease {
  int16_t globalPattern[kMaxLeasePatterns];
  uint8_t count;
  uint8_t pageIndex;
  uint8_t trackMask;   // PhraseCore kTrackSynthA/kTrackSynthB/kTrackDrums bits
  uint8_t ownerSlot;
  uint8_t generation;
  uint8_t active;
};

AcquireResult acquire(
    const Scene& scene,
    int pageIndex,
    uint8_t count,
    uint8_t trackMask,
    PatternLease& lease,
    int preferredLocalSlot = 0);

}
```

Compatibility requirement:

- current Phrase callers can use `trackMask = PhraseCore::kAllTracks` and retain aligned P1a behavior;
- Song Picker uses exactly one bit for the selected editable track;
- no new allocator class is introduced.

The owner-internal safe predicate must become mask-aware:

- requested physical track(s) must be strictly empty;
- requested track/global IDs must have zero persistent Song/Phrase references;
- any conflicting active lease remains excluded by the same owner;
- non-requested physical tracks are not cleared or claimed by a one-track lease.

P4 itself must not implement that scan.

### Required extension: track-scoped discard

The existing signature can stay:

```cpp
LeaseStatus discard(
    Scene& scene,
    int currentPageIndex,
    PatternLease& lease);
```

but semantics must follow `lease.trackMask`:

- validate page/token first;
- refuse discard if a requested track/address gained a persistent reference;
- clear only requested physical track backing;
- never clear non-requested Synth/Drum material at the same global coordinate;
- release exactly that lease.

### Required extension: two-phase persistent transfer

Current P1a `transferCommittedOwnership()` validates after persistent ownership exists. For Song Picker, a safer consumer contract is two-phase so P4 never has a successful Song assignment followed by a fallible lease-release step.

Required equivalent surface:

```cpp
namespace PhrasePatternLease {

enum class PersistentClass : uint8_t {
  ReferenceOnly = 0,
  SongGenerated,
};

struct PreparedTransfer {
  uint8_t ownerSlot = kInvalidOwnerSlot;
  uint8_t generation = 0;
};

LeaseStatus preparePersistentTransfer(
    Scene& scene,
    int currentPageIndex,
    PatternLease& lease,
    PersistentClass persistentClass,
    PreparedTransfer& prepared);

void completePersistentTransfer(
    PatternLease& lease,
    const PreparedTransfer& prepared);

}
```

Required semantics:

`preparePersistentTransfer()`:

- validates active lease token, page, address shape, and track mask;
- does **not** write Song or Phrase references;
- does **not** release the lease;
- for `SongGenerated`, prepares the existing reclaim classification for the leased track using the existing Song-generated ownership policy while the bytes are still lease-owned and discardable;
- performs no new persistence/schema work;
- remains rollback-safe through normal `discard()` if the later Song transaction does not commit.

`completePersistentTransfer()`:

- is bookkeeping-only and non-failing after a valid prepare;
- is called only inside/at the guaranteed-success edge of the synchronous persistent commit;
- releases temporary exclusivity without clearing accepted bytes;
- invalidates the lease handle;
- does not publish a second Scene revision or Undo receipt.

P4 must not set `SynthStep::unused`, `DrumStep::unused`, or `kSongGeneratedOwnershipBit` directly. The existing reclaim classification is ownership policy and belongs behind the centralized transfer contract.

If P1a chooses different C++ names, the behavioral surface above is the P4 requirement.

### P4 use for one Song cell

For Synth A, Synth B, or Drums:

```text
trackMask = one PhraseCore track bit
count     = 1
page      = current resident page
preferred = current track bank * 8   (hint only)
```

The candidate producer writes only the leased track material at `lease.globalPattern[0]`.

No P4 free-slot search exists.

---

## Does EXISTING preview need a lease?

**No.**

An Existing candidate already has persistent physical backing. Previewing it is a read-only runtime operation.

A lease would be actively harmful here because it would:

- consume scarce temporary owner capacity for bytes P4 does not own;
- confuse persistent backing with temporary backing;
- create unnecessary discard semantics;
- make shared Song/Phrase references look exclusive when they are intentionally shared.

EXISTING preview rules:

1. resolve candidate with canonical `PatternAddress`;
2. load its page using existing page navigation if needed;
3. audition without editing Pattern bytes or Song refs;
4. accept by assigning the existing global ID to the selected Song cell;
5. discard by doing nothing persistent.

If the user wants to modify the Existing Pattern, plain Enter / Quick Jump to Editor remains the correct route. Pattern Editor then owns Pattern Undo normally.

---

## How GENERATE / RELATED get temporary storage

### GENERATE

Storage sequence:

1. enter GENERATE while a Pattern page is resident;
2. request `count=1`, current-track `trackMask` from the P1a owner;
3. if acquisition fails, keep the picker open and report no temporary backing;
4. produce candidate material using existing production generation semantics;
5. write the prepared value into the leased track backing;
6. audition it without changing the Song cell;
7. reroll by replacing material in the **same lease/address**;
8. accept or discard.

P4 must not call `SongPatternMaterializer::generate()` for audition because that function ends by assigning the Song reference and marking Scene mutation.

A future implementation may extract/reuse the existing Atlas/Groovebox candidate-production portion of `SongPage::materializeSongTracks()` as a pure prepared-value producer, provided legacy `G` behavior remains unchanged. That is reuse/refactoring, not a new generator.

### RELATED

RELATED has the same storage lifecycle as GENERATE:

```text
producer -> prepared Pattern value -> same P1a lease -> audition -> accept/discard
```

This audit did not establish a production Related candidate producer that satisfies the requested boundary. Therefore P4 must **not** implement RELATED by calling EVOLVE, DERIVE, Phrase Lab, or inventing a generator.

The future picker may reserve the state/tab in design, but runtime enablement must wait for an explicit existing producer contract.

---

## Accept as a Song assignment transaction

### Existing candidate

Accept is exactly the existing R4 Song mutation shape:

```cpp
commitSongMutation([&](Song& song) {
  GroovePuterUndo::SongEdit::setPattern(
      song, row, track, candidateGlobalPattern);
});
```

Consequences:

- one Song Undo receipt;
- Song length extension remains canonical;
- current playback activation rules remain canonical;
- candidate Pattern bytes are not copied;
- no Pattern Undo receipt is created.

### Leased GENERATE / RELATED candidate

Required order:

```text
1. P1a preparePersistentTransfer(... SongGenerated ...)
2. commitSongMutation(... set candidate global Pattern ...)
   - inside the successful synchronous commit edge:
     P1a completePersistentTransfer(...)
3. close picker
```

If the Song commit does not execute/succeed, P4 does not complete transfer; the lease remains discardable.

This is ownership transfer, not copy-on-accept.

The Song transaction remains the only persistent user action visible to Undo/revision ownership.

---

## Discard leaves Song untouched

Discard is intentionally asymmetric with the current first-tap `G` behavior.

Pattern Picker must **not**:

- assign a temporary Song reference and later roll it back;
- use `rollbackPendingCellGeneration()`;
- restore Scene revision from a SongPage-local snapshot;
- create and then cancel a Song Undo receipt.

Instead:

```text
old Song reference remains in place for the entire audition
```

For EXISTING, discard is only modal close/state reset.

For GENERATE/RELATED, discard calls the P1a owner, which clears temporary leased-track backing and releases the lease. Song and Phrase values remain byte-identical to their pre-picker values.

---

## Undo boundary

### Preview

Opening Picker, moving candidate, auditioning Existing, acquiring a lease, replacing a leased candidate, and discarding must create:

- no `UndoKind::Song` receipt;
- no Pattern receipt;
- no Phrase receipt;
- no persistent Scene revision.

### Accept

Accept creates exactly one persistent Song action through the existing R4 owner.

Undo after accept restores the previous Song reference. Redo restores the accepted global Pattern reference.

### Accepted physical backing must survive Song Undo

For a leased generated/related candidate, Song Undo must **not** clear the accepted physical Pattern bytes.

Reason:

- redo needs the same global Pattern address;
- another Song/Phrase reference may have been created later;
- physical cleanup would create a second ownership/history system outside the Song receipt.

After Undo, the accepted generated Pattern can become an unreferenced orphan. It should remain classified under the existing Song-generated reclaim policy so the canonical Song generation allocator can reuse it later.

This is why promotion classification must be centralized during lease transfer rather than left as a raw P4 bit write.

### Playing Song

Picker accept must reuse current `commitSongMutation()` activation/Undo rules. P4 should not invent a different live-commit timing path.

Preview audio switching is runtime-only and must not masquerade as Song arrangement truth.

---

## Persistence boundary

### Existing candidate

No new persistence behavior. Page navigation and page files are already persistent backing. Accept changes only the normal Song reference.

### Active lease

An active lease is runtime-only, but its candidate bytes reside in the ordinary resident Scene pattern banks.

`SceneManager::setPage()` saves the current page before leaving it. Therefore allowing a page switch with an undiscarded candidate can serialize temporary preview bytes into `.gpp`.

Future Picker must enforce:

```text
ACTIVE LEASE => NO PAGE LEAVE / NO SAVE PATH THAT SERIALIZES TEMP BYTES
```

The simplest P4 policy is:

- EXISTING: page navigation allowed because there is no lease;
- entering GENERATE/RELATED: acquire on current page and pin picker page;
- changing page/category away: discard lease first, then navigate;
- accept: complete transfer first/within persistent commit, then page may change;
- close/cancel: discard before returning control.

P4 must not add a serializer exception or lease persistence field.

### Accepted candidate

After successful transfer, the physical Pattern becomes ordinary persistent page backing and the Song stores its existing global ID. No schema or new persistence object is needed.

### Runtime lease state

Never serialize:

- lease token;
- owner slot;
- picker category;
- highlighted candidate;
- audition state;
- transfer-preparation state.

---

## UI controls proposal

This is a proposal for the later implementation, not a P4R key-map change.

### Entry

**Proposed:** `Alt+Enter` on an editable Song Pattern cell.

Why:

- plain Enter is already Quick Jump to Editor;
- no `Alt+Enter` binding was found in the audited Song/global key map;
- Alt is a practical Cardputer modifier;
- the gesture reads naturally as “alternate action for this cell”.

It must still receive a final global-router source test when implemented.

### Inside Picker

Recommended modal ownership:

| Key | Picker action | Reason |
|---|---|---|
| Left/Right or Tab | switch `EXISTING / GENERATE / RELATED` | modal-local category navigation |
| Q..I | select slot 1..8 in EXISTING | reuses established Pattern muscle memory |
| B | bank A/B in EXISTING | safe only because modal consumes event |
| Ctrl+1..8 / Ctrl+Fn+1..8 | page 1..16 in EXISTING | reuse existing page shortcut decoder |
| Up/Down | move candidate/list focus where applicable | no Song cursor leak while modal open |
| Enter | accept highlighted/auditioned candidate | consistent commit gesture |
| Esc / backtick | discard and close | consistent cancel gesture |

Candidate preview should start automatically when the highlighted candidate changes, avoiding another overloaded Cardputer key.

### Keys that should not open Picker

Do not consume these at Song level for Picker entry:

- `G`: cell generation / double-G row / Alt+G selection / Ctrl+G mode;
- `B`: assignment bank; Alt+B stored-bank flip; Ctrl+B playback slot;
- `Q..I`: direct Pattern assignment;
- plain Enter: Quick Jump to Editor;
- `V`: lane focus;
- `X`: split view / Alt+X LiveMix;
- `L`: loop operations;
- `M/N`: Song mode / row operations;
- `Ctrl+1..8`: page navigation.

### Modal event isolation requirement

While Picker is open, every consumed key must stop before Song Page legacy/R4 handling. In particular:

- picker `G` must never trigger Song double-G state;
- picker `B` must never flip a stored Song bank;
- picker Q..I must never assign Song before Enter;
- picker page commands must be disabled while a lease is active.

---

## Partial-redraw / UI budget

Cardputer ADV display budget is 240x135. Song Page already has a dense header/grid/footer and multiple themes.

Future Picker should be a bounded overlay/panel, not another full-page data model.

Required redraw policy:

- full Song background redraw is acceptable on picker open/close and after a committed accept;
- candidate movement redraws only the picker panel / candidate preview region;
- automatic audition ticks do not redraw the full Song grid;
- category/bank/slot changes invalidate only the picker header, selector rows, and preview summary;
- no full-screen clear on every Q..I/B candidate change;
- one logical input should group its panel draw work rather than produce many independent screen transactions;
- use the existing fixed 8-slot Pattern bar and 2-bank bar instead of heap-backed scrolling lists;
- no Pattern waveform/history cache is required by this checkpoint.

A later implementation should prefer a compact panel that leaves the selected Song cell context visible. Exact pixels are intentionally not frozen by this research branch.

---

## Current help/docs implications

When P4 is eventually implemented, update all of these together:

- Song Page footer for every theme;
- Song multi-frame help;
- `docs/keys_music.md`;
- any quickstart/global key sheet that still claims Q..U or stale Song G semantics.

Do not document Pattern Picker before the implementation exists.

---

## Risks

### R1 — current P1a transfer is all-track aligned

**High / implementation blocker.**

Current P1a cannot release a one-cell Song candidate because it requires persistent A+B+Drums ownership. Fix by extending the same owner with track-mask semantics. Do not create a Song-specific lease owner.

### R2 — aligned-only P1a acquisition would cause false exhaustion

If P4 reused current P1a `localSlotIsSafeForPhrase()` unchanged, a Synth A preview would fail unless Synth A, Synth B, and Drums were all empty at the same coordinate. Current Song G can use track-local free backing, so this would be a user-visible regression in available capacity.

Track-mask acquisition is required, not merely track-mask transfer.

### R3 — temporary bytes can leak into `.gpp`

Page switch saves the current raw Pattern banks. An active lease must pin the page or be discarded before leaving/saving.

### R4 — Existing preview must stay read-only

Reusing Pattern Editor callbacks would mutate persistent runtime selection/edit context and risks editing a Pattern shared by other Song/Phrase references. Reuse selector components, not editor mutation callbacks.

### R5 — accepted generated orphan classification

Undo removes the Song reference but intentionally leaves accepted physical backing. Without the existing Song-generated reclaim classification, repeated picker use could accumulate non-empty unreferenced material that canonical Song generation will not reclaim.

Promotion policy belongs behind P1a transfer, not in P4 raw bytes.

### R6 — two allocators racing

Legacy Song G remains live and continues using `SongPatternMaterializer`. Picker preview uses P1a. While Picker is modal, Song generation gestures must not fall through. P1a remains the only **temporary audition** allocator; legacy G remains the existing immediate-commit allocator until a separate task changes it.

### R7 — Phrase references are persistent borrowers

Any mask-aware P1a extension must continue checking Phrase references for the requested track. A Song-only scan is insufficient.

### R8 — Undo must not own lease discard after accept

Binding Song Undo to physical cleanup would make redo and later shared references unsafe. Undo changes Song reference only; orphan reclaim remains allocator policy.

### R9 — RELATED producer is undefined in this boundary

Do not implement RELATED by quietly routing to EVOLVE/DERIVE or adding a generator. Storage integration can be ready while the category remains disabled pending a separate producer contract.

### R10 — key-map density and stale help

Song has several modifier collisions already. Modal event isolation and synchronized help updates are mandatory when implementation begins.

### R11 — page/track are both ownership coordinates

A `PatternAddress` alone is insufficient to name physical candidate bytes because Synth A, Synth B, and Drums have separate stores. Every lease used by P4 must carry a track mask.

### R12 — current double-G rollback is tempting but wrong

`pending_cell_generation_` restores one gesture's immediate commit. Reusing it for Picker would make preview dirty Song/revision state and would create a second lifecycle owner. Explicitly forbidden.

---

## Recommended implementation split

No implementation belongs in P4R. Recommended later checkpoints:

### P4.1 — Existing-only Picker shell

- modal anchored to one Song cell;
- reuse `PatternAddress`, page shortcut decoder, Pattern/Bank selection bars;
- EXISTING read-only preview;
- accept through existing `commitSongMutation()`;
- discard/close with no persistent mutation;
- no PatternLease dependency yet.

This proves UI/navigation and Undo boundaries without allocator risk.

### P1a follow-up — track-mask lease contract

Before generated Picker audition:

- generalize the **same** P1a owner with `trackMask`;
- make acquire/reuse/discard reference and clear only requested tracks;
- add rollback-safe two-phase persistent transfer;
- preserve current all-track Phrase behavior via `kAllTracks`;
- keep owner fixed-capacity and non-persistent.

This is an owner extension, not P4 implementation.

### P4.2 — GENERATE lease consumer

After the P1a extension lands:

- acquire one-track `count=1` lease;
- reuse existing generation producer semantics;
- repeated generate replaces candidate in same lease;
- accept = prepare transfer + one Song transaction + complete transfer;
- discard = P1a discard;
- legacy Song G unchanged.

### P4.3 — RELATED producer adapter

Only after a separate task establishes an allowed existing Related producer:

- feed its prepared Pattern value into the same P1a lease lifecycle;
- no second allocator;
- no EVOLVE/DERIVE/Phrase Lab hidden behind the tab.

### P4.4 — UI/performance/help closure

- partial redraw source tests / simulator review;
- Cardputer key conflict tests;
- all theme footers/help/docs synchronized;
- hardware audition acceptance.

---

## Hardware assumptions

Target UX hardware is M5Stack Cardputer ADV:

- ESP32-S3;
- 240x135 display;
- compact QWERTY keyboard;
- Alt/Ctrl are practical modifiers;
- practical Shift should not be assumed as the only way to access a command.

No GPIO, I2C, SPI wiring, MIDI electrical, audio routing, or voltage changes are introduced by this audit.

P4R requires no hardware connection or flash because it changes documentation only.

---

## Validation / build steps

Research-branch validation is intentionally source/diff oriented:

```bash
git status --short
git rev-parse HEAD
git merge-base HEAD 78bc8394ede5e6d81464cff5878c29bbf754c555

git diff --check 78bc8394ede5e6d81464cff5878c29bbf754c555..HEAD
git diff --name-only 78bc8394ede5e6d81464cff5878c29bbf754c555..HEAD
git diff --stat 78bc8394ede5e6d81464cff5878c29bbf754c555..HEAD
```

Expected changed path:

```text
docs/audits/P4_PATTERN_PICKER_AUDIT.md
```

There is no production behavior to flash or listen to in P4R.

---

## Expected behavior

P4R itself changes no screen, serial, audio, Song, Pattern, Phrase, Undo, generation, or persistence behavior.

A later conforming implementation should demonstrate:

- EXISTING preview without a lease;
- GENERATE/RELATED temporary material only through P1a;
- no Song change before Enter;
- discard byte-preserves Song/Phrase state;
- accept creates one Song Undo action;
- Undo restores the old Song ref while accepted backing remains valid for redo;
- no temporary candidate survives page leave/save;
- no second free-slot allocator exists in P4.

---

## Troubleshooting guidance for future implementation

- **Picker says no free slot while current G can generate on that track:** P1a is still using all-track aligned safety; implement the track-mask owner extension, not a P4 scan.
- **Accept fails transfer after assigning one cell:** current all-track P1a transfer is still being used.
- **Discard changes Song:** reject the implementation; preview lifecycle is wrong.
- **Existing preview allocates a lease:** remove it; Existing is persistent read-only backing.
- **Page switch persists a rejected candidate:** active lease was not discarded/pinned before `SceneManager::setPage()`.
- **Undo deletes accepted Pattern bytes:** physical lifetime was incorrectly attached to Song Undo.
- **Repeated GENERATE consumes new addresses:** caller is reacquiring instead of reusing the active P1a lease.
- **RELATED calls EVOLVE/DERIVE:** outside P4 contract; reject.
- **P4 contains a free-slot loop:** reject; ownership was duplicated.

---

## Acceptance checklist

- [ ] branch ancestry starts from exact `78bc8394ede5e6d81464cff5878c29bbf754c555`
- [ ] P4R does not stack P1a implementation into production ancestry
- [ ] P1a reference branch/API was inspected separately
- [ ] current Song cell/selection flow documented
- [ ] global PAGE/BANK/SLOT = 16 x 2 x 8 = 256 documented
- [ ] canonical `PatternAddress` reuse identified
- [ ] Q..I exact mapping documented
- [ ] plain B / Alt+B / Ctrl+B ownership documented
- [ ] current-cell G documented
- [ ] double-G immediate-commit + rollback behavior documented
- [ ] Alt+G selection generation noted
- [ ] current Song COW/reclaim rules documented
- [ ] both Song slots and Phrase references included in persistent borrower model
- [ ] current Song Undo/revision owner documented
- [ ] Pattern/Bank selection components identified for UI reuse only
- [ ] EXISTING preview explicitly requires **no lease**
- [ ] GENERATE uses P1a only; no P4 allocator
- [ ] RELATED storage uses P1a only and producer remains unimplemented in this boundary
- [ ] current P1a all-track transfer incompatibility with one-cell accept documented
- [ ] required P1a track-mask acquisition/discard extension specified
- [ ] required rollback-safe two-phase transfer specified
- [ ] accepted generated backing reclaim classification remains centralized
- [ ] accept is exactly one canonical Song assignment transaction
- [ ] discard leaves Song/Phrase untouched
- [ ] preview creates no persistent Undo/revision receipt
- [ ] Song Undo restores reference only and preserves accepted backing
- [ ] active lease cannot cross resident-page save/switch
- [ ] no lease state is persisted
- [ ] `Alt+Enter` proposed only as future Picker entry, with final router test required
- [ ] current key conflicts documented
- [ ] partial-redraw budget documented
- [ ] help/docs drift documented
- [ ] no `src/` production files changed by P4R
- [ ] no tests require production semantic changes
- [ ] no PatternLease implementation in P4R
- [ ] no new free-slot allocator
- [ ] no new generator
- [ ] no Phrase Lab / EVOLVE / DERIVE / F08.1
- [ ] no persistence/schema change
- [ ] research PR remains Draft and unmerged
