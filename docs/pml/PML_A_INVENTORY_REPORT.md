# PML-A — project material: reproduction and inventory (read-only)

Status: **report, no production change, no user data touched.** Exact SHA of the run: `e2ea65b8f4c8d3c82d30f54cd2cf780400dbdfe7`
(tool files `tools/pml/pml_a_inventory.cpp`, `tools/pml/run_pml_a_inventory.sh` were untracked at the time and are committed with this report).
The tool runs in an isolated temporary project and a private working directory; it computes the free-slot preview from masks and frees nothing.
Tags: **[OBSERVED]** measured or read from code; **[INFERENCE]**; **[OPEN]**.

## 1. Reproduction (host, real engine)

Techno / broken_techno, P3. `G` = `GeneratedPhraseSong::generate`, `D` = `generateCycle`.

| Step | Result |
|---|---|
| [1] TAKE 4B | CommittedNow |
| [2] GROW | CommittedNow, 8 bars (A uses slots 0-3, cycle 4-11) |
| [3] delete every Song row (emulated: rows cleared, length = 1) | content and descriptors of slots 0-11 **stay** |
| [4] TAKE 4B again | CommittedNow (uses the only free run, slots 12-15) |
| [5] TAKE 4B a third time | **Failed, `NoContiguousPatternSlots`** ("Need consecutive empty slots") with an empty Song |
| [6] GROW after [4] | **NoSafeSlots** |

**[OBSERVED]** "Deleted the rows, still no slots" is reproduced and is by design of the predicate: a slot counts as free for phrase generation only if
(a) all three physical patterns (Synth A, Synth B, drum set) are empty **and** (b) both material descriptors are canonical-free **and** (c) nothing in either Song or the Phrase Bank references it
(`PhraseGenerator::localSlotIsSafeForPhrase`). Deleting rows removes only (c). The page has 16 slots; A (4) + cycle (8) = 12; every further TAKE consumes 4 more.
Real result for generation: after [3] a **4-slot run exists (12-15) but never an 8-slot run**, so a second GROW is impossible even with an empty Song.

## 2. Slot table (what holds each slot)

Columns: physical content per lane (A/B/D), descriptor, content class, Song/Phrase-Bank references, protecting holders, what blocks phrase generation, free for phrase.
Holders checked: Song rows (both songs) and Phrase Bank reference views; CURRENT (selectors and drum selector); working material bound to the slot; queued NEXT; Melody descriptors;
the Undo receipt; the recipe and the origin sidecar. Full tables (after GROW, after deleting rows, after repeated TAKEs) are in the raw output below.

**Findings from the tables [OBSERVED]:**

1. After deleting rows, slots 0-11 have **no Song or Phrase-Bank reference** but are still blocked by content + descriptor; slot 4 also shows CURRENT (selector) and slots 4-11 show the Undo receipt.
2. **A loses its provenance once the cycle is published.** The origin sidecar describes only the latest generated phrase (the 8 cycle bars), so the kept phrase A (slots 0-3) is classified `unknown/manual`.
   After the cycle, generated-unedited and hand-made patterns cannot be told apart for A. A freeing contract must not rely on the current origin sidecar.
3. Descriptors: every generated Synth A slot carries a canonical MaterialId (`A.`); Synth B carries none. A slot with content cleared but a descriptor left (orphan descriptor) still blocks generation.
4. The Song-generation path already has a safe-reclaim precedent: `SongPatternMaterializer` marks patterns it created (`kSongGeneratedOwnershipBit`), reuses a uniquely referenced one in place
   and reclaims orphans that carry the mark; **non-empty manual/imported material is never reclaimed**. The phrase path (`generate`) does not write that mark (slots report `songGeneratedBit` false).

## 3. Preview: what could become a candidate

Computed from masks (nothing freed). Candidate = occupies a slot only by content/descriptor and is protected by nothing (no Song/Phrase-Bank reference, not CURRENT, not working, not NEXT, not Melody, not in the Undo receipt).

| State | Free for phrase now | Candidates | 4B run | 8B run |
|---|---|---|---|---|
| after TAKE + GROW | 4 | 0 | yes (12) | no |
| after deleting the Song rows | 4 | 4 (slots 0-3, unprotected) | yes (12) | no, **even if all candidates were freed** |
| after the repeated TAKEs | 0 | 12 (slots 0-11) | no | only after freeing candidates (start 0) |

**[OBSERVED]** Protection matters: right after deleting the rows, slots 4-11 are still held by the Undo receipt and slot 4 by CURRENT, so freeing "everything unreferenced" would
either break Undo of the cycle or drop the current selection. **[INFERENCE]** All candidates here are classified `unknown/manual` or `generated-unedited`; none can be proven disposable, which is why
**deleting rows must not be treated as permission to delete music** (owner's rule).

## 4. Undo and save boundary

* **[OBSERVED]** One Undo receipt, 1536 B (`kUndoPayloadBytes`). A generation receipt carries the whole Song (1032 B) plus page/slot/bars; it restores rows and clears the slots it created.
* **[OBSERVED]** Content of one slot is 1416 B (2 x 112 B synth + 1192 B drum set). **One** freed slot fits into an otherwise empty receipt (1416 <= 1536), but **not** next to a Song copy (1536 - 1032 = 504 B).
  Freeing several slots cannot be made undoable with the existing receipt.
* **[OPEN]** Options for a reversible free: (a) keep content and only mark slots reclaimable until overwritten (nothing is lost, the slot is reused first-fit); (b) snapshot the slots to a file
  under the project before clearing; (c) a larger bounded Undo region. Each needs an owner decision; none is implemented.
* **[OPEN, not measured here]** What the existing page save / autosave preserves for a slot that was cleared in RAM: `PatternPagingService::savePage` is called on page switch (`miniacid_display.cpp`) and from the page-save helpers in `scenes.cpp` (around lines 1909-1925);
  I did not trace which user actions (Save, autosave) reach those helpers. A freed slot that has been saved is gone from disk. Needs its own check before any freeing action.

## 5. What the next contract must answer (input for PML-B)

1. Does "free" mean erase, or **reclaimable (overwritable, still recoverable)**? The data above favours reclaimable: no Undo mechanism fits erase, and nothing proves the content disposable.
2. Provenance for A after a cycle: a per-slot durable mark for phrase-generated material (like the Song path's ownership bit), or an explicit user confirmation for anything not marked.
3. Protected set (never freed by the action): Song/Phrase-Bank referenced, CURRENT, working, NEXT, Melody, anything held by the live Undo receipt.
4. Preview before confirmation: slots to be freed, slots protected and why, and the resulting **longest contiguous run** (not the count), because 8 bars are needed to GROW.

## Raw tool output (`tools/pml/run_pml_a_inventory.sh`)

```
exact SHA: e2ea65b8f4c8d3c82d30f54cd2cf780400dbdfe7  (dirty files: 0)
PML-A inventory. Isolated temporary project 'pml-a-inventory'; nothing is freed or deleted.
page=0 slots=16 sizeof(SynthPattern)=112 sizeof(DrumPatternSet)=1192 sizeof(Song)=1032 undoPayloadBytes=1536

[1] TAKE 4B at row 1: CommittedNow
[2] GROW (D): CommittedNow bars=8

== after TAKE + GROW (A 4 + cycle up to 8) ==
slot A B D  desc  content-class        refs(song/pb)  holds(protected)           blocks-phrase-by  free-for-phrase
 0   x x x  A.    unknown/manual       3/0            SONG                       content+descriptor+song no
 1   x x x  A.    unknown/manual       3/0            SONG                       content+descriptor+song no
 2   x x x  A.    unknown/manual       3/0            SONG                       content+descriptor+song no
 3   x x x  A.    unknown/manual       3/0            SONG                       content+descriptor+song no
 4   x x x  A.    generated-unedited   3/0            SONG,CURRENT,UNDO-RECEIPT  content+descriptor+song no
 5   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
 6   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
 7   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
 8   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
 9   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
10   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
11   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
12   . . .  ..    empty                0/0                                       -                 yes
13   . . .  ..    empty                0/0                                       -                 yes
14   . . .  ..    empty                0/0                                       -                 yes
15   . . .  ..    empty                0/0                                       -                 yes
free for phrase now: 4 of 16 | candidates to free (unprotected, not Song/Phrase-Bank referenced): 0
  contiguous 1B: now=start 12  after-freeing-candidates=start 12
  contiguous 2B: now=start 12  after-freeing-candidates=start 12
  contiguous 4B: now=start 12  after-freeing-candidates=start 12
  contiguous 8B: now=NONE  after-freeing-candidates=NONE

[3] user deletes the Song rows (emulated: every row of Song 1 cleared, length=1)

== after deleting the Song rows (content and descriptors remain) ==
slot A B D  desc  content-class        refs(song/pb)  holds(protected)           blocks-phrase-by  free-for-phrase
 0   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 1   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 2   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 3   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 4   x x x  A.    generated-unedited   0/0            CURRENT,UNDO-RECEIPT       content+descriptor no
 5   x x x  A.    generated-unedited   0/0            UNDO-RECEIPT               content+descriptor no
 6   x x x  A.    generated-unedited   0/0            UNDO-RECEIPT               content+descriptor no
 7   x x x  A.    generated-unedited   0/0            UNDO-RECEIPT               content+descriptor no
 8   x x x  A.    generated-unedited   0/0            UNDO-RECEIPT               content+descriptor no
 9   x x x  A.    generated-unedited   0/0            UNDO-RECEIPT               content+descriptor no
10   x x x  A.    generated-unedited   0/0            UNDO-RECEIPT               content+descriptor no
11   x x x  A.    generated-unedited   0/0            UNDO-RECEIPT               content+descriptor no
12   . . .  ..    empty                0/0                                       -                 yes
13   . . .  ..    empty                0/0                                       -                 yes
14   . . .  ..    empty                0/0                                       -                 yes
15   . . .  ..    empty                0/0                                       -                 yes
free for phrase now: 4 of 16 | candidates to free (unprotected, not Song/Phrase-Bank referenced): 4
  contiguous 1B: now=start 12  after-freeing-candidates=start 0
  contiguous 2B: now=start 12  after-freeing-candidates=start 0
  contiguous 4B: now=start 12  after-freeing-candidates=start 0
  contiguous 8B: now=NONE  after-freeing-candidates=NONE

[4] TAKE 4B again: CommittedNow error=0
[5] TAKE 4B a third time: Failed error=5 (PhraseError 5? see phrase_generator.h NoContiguousPatternSlots)
[6] GROW after the second TAKE: NoSafeSlots

== after the repeated TAKEs ==
slot A B D  desc  content-class        refs(song/pb)  holds(protected)           blocks-phrase-by  free-for-phrase
 0   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 1   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 2   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 3   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 4   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 5   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 6   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 7   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 8   x x x  A.    unknown/manual       0/0                                       content+descriptor no
 9   x x x  A.    unknown/manual       0/0                                       content+descriptor no
10   x x x  A.    unknown/manual       0/0                                       content+descriptor no
11   x x x  A.    unknown/manual       0/0                                       content+descriptor no
12   x x x  A.    generated-unedited   3/0            SONG,CURRENT,UNDO-RECEIPT  content+descriptor+song no
13   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
14   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
15   x x x  A.    generated-unedited   3/0            SONG,UNDO-RECEIPT          content+descriptor+song no
free for phrase now: 0 of 16 | candidates to free (unprotected, not Song/Phrase-Bank referenced): 12
  contiguous 1B: now=NONE  after-freeing-candidates=start 0
  contiguous 2B: now=NONE  after-freeing-candidates=start 0
  contiguous 4B: now=NONE  after-freeing-candidates=start 0
  contiguous 8B: now=NONE  after-freeing-candidates=start 0

Undo/save boundary facts (from code, not from this run):
  * one Undo receipt (1536 B). A Generation receipt stores the whole Song plus (pageIndex, firstLocalSlot, bars); it restores rows and clears the slots it created.
  * restoring ONE freed slot needs 1416 B of pattern content (2 synth + 1 drum set); 0 slots would fit next to a Song copy.
```
