# PML-D — user scenarios and mockup (no production code)

Status: **scenario analysis + screen mockup + host proof.** Exact SHA of the proof run: `e7f8d66f11e3b91b2ed3e666e2856caf12fd6c36` (tool files `tools/pml/pml_d_proof.cpp`, `tools/pml/run_pml_d_proof.sh`
committed with this report). Prototype steps in the tool (forgetting the cycle flag, clearing the origin sidecar, clearing the Undo receipt, moving CURRENT) are stand-ins showing what a feature would need; they are **not production code**.
Decision source: owner's UI decision (action lives on MATERIAL; explicit selection; preview distinguishes "permission set" from "generation possible now"). Tags: **[OBSERVED]**, **[INFERENCE]**, **[OPEN]**.

## 1. The question to settle first: "GROW again"

Owner's proposal: an explicit action **"End the Undo of this cycle"**, removing only the receipt's protection, with CURRENT and the other holders still protecting.

**[OBSERVED] (S1)** The scenario: A kept in the Song, the cycle rows deleted, GROW pressed again.

| Step | Result |
|---|---|
| GROW with the cycle rows gone | `CycleAlreadyPublished`: the recipe still records a published cycle, independent of slots |
| (prototype) forget the cycle flag, GROW | **`EditedSinceGeneration`** although nothing was edited: see finding F1 |
| (prototype) clear the origin sidecar, GROW | `NoSafeSlots`: the slots of the deleted cycle are held by the live Undo receipt (and CURRENT) |
| a mark on those slots | refused (protection beats the mark) |
| (prototype) end the receipt, move CURRENT off the cycle, mark slots 4-11 | an 8-slot run appears; GROW succeeds |
| the regrown cycle vs the deleted one | **bit-identical (8 of 8 slots)** |

**Conclusion [OBSERVED]:** ending the receipt's protection *does* make the slots available, but for the same A the cycle is a pure function of the recipe, so the musician gets **the same music back**. The action would
remove a safeguard (the Undo) in order to reproduce what was just deleted. **Recommendation: do not build "End the Undo of this cycle".** What the musician wants in S1 ("try another cycle") needs a variation salt
(`P1_KEEP_AND_VARIANTS_SPEC.md`, slice V1). A variant operation replaces its **own** cycle: the old receipt is superseded by the variant's receipt, so no separate user-facing "end Undo" is needed, and because
cycle content is a pure function of `(recipe, salt)`, Undo of a variant could be implemented by **re-deriving the previous salt's cycle** instead of storing replaced content. **[OPEN]** not proven; it is a design hypothesis for V1.

### Findings that belong to the next production slice
* **F1 (latent bug; FIXED after this report in a separate commit with a regression test, see PML_E).** `generateCycle` cross-checks the origin sidecar with `origin.barCount != recipe.bars`. After a cycle the sidecar describes 8 bars, so any future repeated GROW would be refused as "edited".
  Today it is hidden by `CycleAlreadyPublished`, and Undo clears the sidecar (which is why "Undo, then D" works). Fix: compare the identity only.
* **F2.** After GROW the CURRENT selector sits on the first cycle slot and protects it in addition to the receipt. Any "replace the cycle" flow must also move CURRENT, or the preview must name it.
* **F3.** The cycle flag (`cycleSongStart`) outlives the cycle's rows. Regrowing after deleting the rows needs a rule for when the flag is reset.
* **F4.** GROW needs 4 or 8 slots depending on the take (a BREAK-only cycle is 4 bars), which the preview cannot know before GROW; it should say "4B, or 8B for a full cycle".

## 2. Scenarios

| # | Scenario | Result [OBSERVED] | Verdict |
|---|---|---|---|
| S2 | Old A + cycle exist; the musician deletes every row, takes a **new** A, presses GROW | TAKE lands on the only free run (12-15); GROW: `NoSafeSlots`; all twelve orphan slots (0-11) can be marked; preview: GROW possible only **after** the permission (longest run now 0, after 12); GROW then succeeds; Undo of that GROW restores and keeps the new A in the Song | **Solved by PML-C alone. No receipt action needed.** |
| S3 | House: four 4B TAKEs fill the page, rows deleted, a fifth TAKE | fifth TAKE refused (no consecutive empty slots); after marking twelve orphans the TAKE succeeds | **Solved by PML-C.** |
| S3 | House GROW | `NotAdmitted` whatever the slots | Slots are not the question for this style; the message must not say "no slots". |
| S3b | Techno, kept phrase edited by hand, GROW | `EditedSinceGeneration` whatever the slots | Stays a boundary of P0; needs its own design (edited phrases). |
| S1 | Regrow the same A | see section 1 | Not a slot problem: needs variation (V1). |

The owner's original path (House -> rave -> manual edit -> delete the bad one -> new generation) therefore needs, in the product: the **allow-replacement** flow (S3) for the generation, while GROW is not available for House and is refused for an edited phrase.

## 3. Screen mockup (MATERIAL, 240x135, about 26-30 characters per line; layout to be verified on the device)

Entry: the existing refusal toast becomes actionable.

```
MATERIAL                 P3 REWORK
LENGTH 4B   STYLE REWORK    TO 1
...
 NO SLOTS: PRESS R TO FREE UP          <- toast after G/D fails for lack of slots
G:TAKE D:GROW P:STYLE R:REPLACE
```

Screen "ALLOW REPLACEMENT" (key R on MATERIAL, leave with ESC/TAB):

```
ALLOW REPLACEMENT        PAGE 1
 1 2 3 4 5 6 7 8  9 10 11 12 13 14 15 16
[S][S][S][S][.][.][.][.] [~][~][~][~][#][#][#][#]
 S SONG  # LIVE UNDO  ~ UNUSED  . FREE
 SLOT 9-12  UNUSED, NOT IN SONG
 [L/R]MOVE [ENTER]ALLOW [X]UNDO MARK
 4B: NOW NO   AFTER YES
 8B: NOW NO   AFTER NO (13-16 UNDO)
```

* Grid = the 16 slots of the page; letters show the holder that protects a slot (Song, Phrase Bank `P`, CURRENT `C`, NEXT `N`, Melody `M`, live Undo `#`); `~` = unused, may be allowed; `*` = allowed (marked); `.` = free.
* The result lines are the two runs the contract requires: **now** and **after the allowed replacement**, per length the musician needs (4B for TAKE, 4B/8B for GROW), with the holder that blocks.
* "Allowed" is shown separately from "possible now": the status line reads `ALLOWED 4 OF 4: 4B NOW POSSIBLE` only when the run exists; otherwise `ALLOWED, STILL BLOCKED BY 13-16`.

Confirmation (before the first mark of a session):

```
ALLOW REPLACEMENT?
Slots 1-4 may be replaced by the
next TAKE/GROW. Old content will
NOT come back with Undo.
[ENTER]ALLOW  [ESC]CANCEL
```

Nothing is erased when the mark is set; marks vanish on scene load, new scene and page change; an edit of an allowed slot cancels its permission (the grid shows it as `~` again or as held).

**[OPEN] key choice:** `R` is proposed because the MATERIAL page handles only G, D, P and Enter; it must be checked against the global key handlers (Alt+R is already make-melody) before implementation.

## 4. What this means for the roadmap

1. **Build next (UI slice):** the allow-replacement screen above. It solves S2 and S3 and removes the "NO SLOTS" dead end; no new resident structure (PML-C already holds the marks).
2. **Do not build:** "End the Undo of this cycle".
3. **Fix with the UI slice:** F1 (identity-only origin check), with a regression test; F4 wording.
4. **Separate design, after listening:** variants by salt (V1), which is also the only honest answer to "try another cycle".
5. **Device acceptance (owner):** House -> rave -> manual edit -> delete the bad one -> new generation, then Undo, save and cold start, while stack and heap high-water are recorded. Not done.

## Raw tool output (`tools/pml/run_pml_d_proof.sh`)

```
exact SHA: e7f8d66f11e3b91b2ed3e666e2856caf12fd6c36  (dirty files: 0)
PML-D proof (isolated temporary projects; prototype steps are marked and are not production code)

S1: A kept in the Song, the cycle rows 4-11 deleted, user presses GROW again
  GROW with the cycle rows gone: CycleAlreadyPublished
  [PASS] blocker 1: the recipe still says a cycle was published (independent of slots)
  GROW after forgetting the cycle flag (prototype): EditedSinceGeneration
  [PASS] finding: a repeated GROW would be refused as 'edited' because the origin sidecar still describes the 8-bar cycle
  GROW with the origin cleared (prototype): NoSafeSlots
  [PASS] blocker 2: slots 4-11 still hold the old cycle and its Undo receipt
  preview: longest run now=4, after marks=4; holders of slot 4: 0x24 (receipt=0x20)
  [PASS] the slots of the deleted cycle are held only by the live Undo receipt
  [PASS] a mark alone is refused for them (protection beats the mark)
  [PASS] finding: the first cycle slot is also CURRENT, so ending the receipt alone is not enough
  [PASS] after the (prototype) end of that Undo, slots 4-11 can be marked
  preview after marks: longest run after=12  GROW 8B possible after=yes
  [PASS] an 8-slot run appears once the receipt protection is ended and the slots are marked
  GROW into the replaced slots: CommittedNow bars=8
  [PASS] GROW succeeds
  the regrown cycle is bit-identical to the one that was deleted: YES
  [PASS] regrowing the same A yields the SAME cycle (pure function of the recipe): slots alone do not give the musician another one

S2: old A + cycle exist, user deletes every row, takes a NEW A, then presses GROW
  [PASS] TAKE 4B (a new A) is committed on the only free run (slots 12-15)
  new A occupies slots 12-15
  GROW now: NoSafeSlots
  [PASS] GROW is refused for lack of slots
  preview now: TAKE4 no / GROW8 no; after marks (none yet): TAKE4 no / GROW8 no
  the musician allows replacement of the unused material: 12 of slots 0-11 marked
  [PASS] all twelve orphan slots (no Song reference, not CURRENT, not in the receipt) can be marked
  preview after marks: longest now=0 after=12  GROW 8B now=no after=yes
  [PASS] the preview separates 'permission set' (after) from 'generation possible now'
  GROW: CommittedNow bars=4
  [PASS] PML-C alone makes GROW possible for a new A (no receipt action needed)
  [PASS] Undo of that GROW reports Restored
  [PASS] the new A is still in the Song after that Undo

S3: House (auto style), four 4B TAKEs fill the page, rows deleted, a fifth TAKE, then GROW
  [PASS] four House TAKEs committed
  [PASS] a fifth TAKE is refused with an empty Song (no consecutive empty slots)
  [PASS] twelve orphan slots can be marked
  [PASS] after allowing replacement the fifth TAKE is committed
  GROW on a House TAKE: NotAdmitted
  [PASS] House cannot GROW at all: slots are not the question for this style

S3b: Techno (admitted), manual edit of the kept phrase, then GROW
  [PASS] a fresh TAKE
  GROW after a manual edit: EditedSinceGeneration
  [PASS] an edited kept phrase cannot GROW (R0 boundary), whatever the slots

RESULT: ALL PROOFS HOLD (0 failing checks)
```
