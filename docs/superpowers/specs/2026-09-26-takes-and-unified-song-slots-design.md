# GroovePuter 0.9.13 — Takes and Unified Song Slots

## Status

Design, 2026-09-26. Unified Song slots: implemented on
`feature/20260926-unified-song-slots`, host test
`tests/run_unified_song_slots_tests.sh` (USS-1..5). Takes: design only.

Base: `dev_0.9.13` @ `6a6f97c5`.

Product decisions taken for this design (user, 2026-09-26):

1. A Melody on a Song row plays **from the row start**; shorter loops, longer
   is cut at the row end. Row length stays `feel.patternBars`.
2. When Song reaches a row while the voice holds an **unsaved** Working Melody,
   that voice **holds** its Working material and slot. No silent loss, no
   autosave.
3. A Song cell references a **slot** (whatever is accepted there), never a
   pinned Take.

---

## 1. The owners that must stay single

```text
ACCEPTED (canonical)   durable, per slot: scene/page file (Pattern) or
                       /projects/<p>/melody/v<V>_g<SSS>_{A,B}.gpml (Melody)
CURRENT  (Working)     workingMaterial_[voice] + activeMaterial_[voice]
NEXT     (pending)     pendingMaterial_[voice]  (one fixed buffer per voice)
```

Neither Takes nor Song adds a fourth musical payload owner. Both are expressed
through these three:

- **Song is a NEXT producer.** It fills the existing NEXT buffer and activates it
  at the row boundary. It never holds a Melody of its own.
- **A Take is an archive entry.** It is never played directly; the only path
  from a Take back to sound is `Take -> NEXT -> CURRENT`, and the only path back
  to canonical is the ordinary `ACCEPT`.

---

## 2. Unified Song slots

### 2.1 The typed reference

A Song cell stays `int16 globalSlot` (`-1` empty, `-2` rehearsal pause). The
type of the reference is **resolved, not stored**: it is the kind of the slot
descriptor (`Scene::materialSlots[voice][residentSlot].kind`).

Storing `{kind, slot, id}` in the cell was rejected:

- it would make the cell a second owner of `kind`, which goes stale the moment
  a slot is promoted (MAKE MELODY + ACCEPT);
- `MaterialId` per cell costs 128 rows x 4 tracks x 2 songs x 4 B = 4 KB DRAM
  against ~2.2 KB free.

Consequence (accepted): a cell follows its slot. If a slot is re-promoted, every
Song row using it plays the new material. This is the same rule as decision 3.

### 2.2 Row activation (audio thread, bar boundary)

`applySongPositionSelection()` already runs at the row boundary inside
`processSequencerEvents()`. Per synth voice it now calls
`applySongSynthRow_(voice, row, slot)`:

| state of the voice / slot                     | result                                              |
|-----------------------------------------------|-----------------------------------------------------|
| Working Melody unsaved                        | **held**: bank/pattern and Working untouched        |
| slot `< 0`                                    | pattern-mode indices restored, saved Melody released |
| slot not on the resident page                 | indices set, saved Melody released, **awaiting**    |
| slot is Pattern                               | indices set, saved Melody released, CURRENT=Pattern |
| slot is Melody, voice already on that Melody  | continue (no reload, no phase reset)                |
| slot is Melody, NEXT prepared for (row, slot) | NEXT activated, recorded as saved, phase reset      |
| slot is Melody, nothing prepared              | saved Melody released, **awaiting**                 |

**Awaiting** voices are silent (fail closed): they neither play the Pattern steps
that lie under a Melody slot nor the previous row's Melody.

A held voice keeps its slot on purpose: moving it would make its Working
Melody "sit" on another slot, and an ACCEPT would then write it there.

Two details the implementation had to get right:

- In Song mode a voice's slot is *derived from the Song cell*
  (`display303PatternIndex`), so by the time the boundary is applied the slot
  has already moved. "Unsaved" therefore cannot mean "not saved into the slot
  the voice is on now" (the Pattern-mode rule). In Song it means what can
  actually be lost: Working Melody not byte-equal to the version last
  loaded/accepted (`workingMelodyUnsaved_`).
- The voice's own slot is tracked in `songVoiceSlot_`. While held,
  `display303PatternIndex` reports that slot, so ACCEPT (after STOP) and
  DISCARD address the Melody's own slot, never the row's.

### 2.3 Preparation (control thread)

`MiniAcid::serviceSongMaterial()` runs from the UI loop under the audio guard
(like `handlePaging_`). SD reads go through the control I/O window, exactly
like `loadSlotMelody_()`.

1. **Catch-up.** Any voice that is not held and not in sync with the current
   row (awaiting, just released from hold by DISCARD, stopped on a Melody row)
   is resolved now: load the slot Melody and activate it immediately.
2. **Prefetch.** While playing, the next row is computed with the same rules as
   `advanceSongPlayhead()` (`peekNextSongRow_()`), and a Melody slot there is
   loaded into `pendingMaterial_[voice]` tagged with `songRow`.

NEXT ownership during Song playback: the NEXT buffer belongs to the Song only
where the Song needs it, i.e. the next row brings the voice a **different
Melody** slot. There `prepareNextMelody()` refuses (`UnsupportedCurrentState`),
and a user NEXT still queued when the Song needs the buffer is cancelled with
its GO. Everywhere else the development workflow (NEXT/GO) keeps working in
Song mode. This matters: the default scene starts with Song mode ON
(`scenes.cpp`, loop rows 0..7), so a blanket "no NEXT while Song plays" rule
would have disabled development on a fresh device (caught by the H5 restart
suite). A user NEXT activated by GO makes CURRENT dirty, so at the next row the
voice is held until that result is saved or discarded. A Song-prepared NEXT is tagged (`PendingMaterial::songRow`), is
never lifecycle-bound (user GO cannot fire it), is hidden from
`hasPendingMaterial()` (it is not a user candidate), and is dropped on
STOP/PAUSE because canonical may change (ACCEPT) while stopped.

The UI loop calls the service only when `songMaterialServiceDue()` (an
unguarded, read-only hint) says there is work, so the audio guard is not taken
every frame. A Melody that fails to load is not retried every frame; START or a
Song position change retries it.

### 2.4 Phase

A Melody's phase is `(absoluteTick - melodyPhaseOriginTick_[voice]) %
lengthTicks`. The origin is reset at the first bar boundary after the voice's
Song material changed (and at transport start). Consequences:

- `X | Y | Z`: Y starts from its first event at the row start.
- `Y | Y` with 1-bar rows and a 2-bar Y: plays Y's bars 1 and 2 (continuation,
  because the material did not change). Choose different slots to restart.
- Pattern mode keeps origin 0 (unchanged behaviour).

Observed, not changed: in Pattern mode START sets `currentTick_ = 383`, so the
first processed tick is 384 and a 2-bar Melody starts at its **second** bar
(`384 % 768`). Song mode is not affected (row-relative origin). Worth a
separate fix.

### 2.5 Reload

Boot hydration used `bank*8+pattern` as the Melody address, which is only the
global slot on page 0. It now uses the page-aware global slot. After boot,
`serviceSongMaterial()` catches up the current row, so a Song saved on
`X | Y | Z` plays all three rows after reload with no manual slot switch.

### 2.6 Leaving Song mode

A voice returns to its Pattern-mode slot and plays that slot's accepted
material (Melody reloaded from SD if the slot is a Melody slot), exactly as
after a Q..I move. A held voice stays on its own slot with its unsaved Melody.

### 2.7 Known limits (not in this slice)

- Cross-page Song rows: the Melody activates one service pass after the page
  load completes; until then the voice is awaiting (silent), not wrong.
- ACCEPT is still refused while Song plays (`UnsupportedCurrentState`). A held
  voice is saved by stopping and pressing Alt+Enter, or dropped with DISCARD.
- UI: no Song-page marker for Melody cells / held / awaiting voices yet.

---

## 3. Takes (design only)

### 3.1 What a Take is

An immutable, addressable copy of one **accepted** state of one slot:

```text
TakeAddress = { voice, globalSlot, generation }
```

`generation` already exists: every Melody publication writes
`storageGeneration` into the A/B file pair. A Take is that generation, kept.

### 3.2 Storage

```text
/projects/<p>/takes/v<V>_g<SSS>_<GGGGG>.gptk
header: magic 'GPTK', version, kind (Pattern|Melody), MaterialId,
        MaterialVersionToken, generation
body:   GPML payload (Melody) or packed SynthPattern (Pattern)
```

- Written by ACCEPT **after** canonical publication succeeds; a failed Take
  write never fails ACCEPT (the canonical state is already durable).
- Bounded ring per slot (proposal: 16). Oldest file is removed first.
- Zero DRAM: listing reads the directory on demand.

### 3.3 Why this is not a second owner

- **Canonical stays where it is.** Resolution (`resolveMaterial`,
  `loadMaterial`) does not read `takes/`. Deleting the whole directory changes
  nothing that plays.
- **There is no "current take" pointer.** "Head" is simply canonical. A pointer
  would be a second answer to "what is accepted".
- **Restore = prepare NEXT.** `restoreTake(addr)` loads the payload and calls
  `prepareNextMelody()` with the ordinary CURRENT basis. GO makes it CURRENT
  (dirty relative to canonical). ACCEPT makes it canonical and, in doing so,
  writes a *new* Take. History is append-only; restoring never rewrites it.
- **Identity.** A Take carries the `MaterialId` it was accepted under. After a
  slot is replaced (new id), older Takes are shown as belonging to a previous
  material and restoring one is refused unless the user confirms a
  replace (which then goes through the normal new-id path).

### 3.4 Song interaction

None by construction: Song references slots (decision 3). Restoring a Take and
accepting it changes every Song row that uses the slot, which is exactly what
"the slot's accepted material" means.

### 3.5 Open questions before implementing Takes

1. Pattern Takes in the first slice, or Melody only? The format is kind-tagged
   so both fit; Pattern needs its own encode/decode.
2. Ring size (16?) and whether DISCARD should also be able to target a Take
   ("discard to take N" = restore + accept, two keys, not one).
3. UI: where the take list lives (MELODY page footer vs. a modal).
