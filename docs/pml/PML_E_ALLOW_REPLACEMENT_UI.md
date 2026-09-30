# PML-E — MATERIAL -> ALLOW REPLACEMENT (UI slice)

Status: **implemented and host-tested (real `PhrasePage`, real engine, real events); not yet run on the device.** Contract and proofs: `PML_B_REUSE_CONTRACT.md`, `PML_C_ALLOW_REPLACEMENT.md`,
`PML_D_SCENARIOS_AND_MOCKUP.md`. Tags: **[OBSERVED]**, **[OPEN]**.

## What the musician gets

* On MATERIAL, **R** opens *ALLOW REPLACEMENT*. R was verified before it was used: global Alt/Meta/Ctrl shortcuts (including Alt+R) are handled separately and the page handles **plain R only**
  (same rule as G, D, P); the performance keyboard is active on PERFORM only, so MATERIAL does not compete for the key. There is no text entry on this page.
* Grid of the 16 slots of the page, one character each: `.` free, `~` unused (may be allowed), `*` allowed, letters = the holder that protects it (`S` Song, `P` Phrase Bank, `C` CURRENT, `W` working,
  `N` NEXT, `M` Melody, `U` live Undo). Protection wins over the permission.
* Cursor: L/R one slot, U/D one bank (8). ENTER on `~` asks first, ENTER on `*` cancels the permission, ENTER on a held slot names the holder. R or ESC leaves; entering the page starts on the product view.
* Confirmation (once per visit): *"ALLOW REPLACEMENT OF SLOT n? The next TAKE or GROW may replace its content. After replacement Undo will not restore the old content. Nothing is erased now."*
* Two results per operation length, separately: `TAKE 4B: NOW NO  AFTER YES`, `GROW 4B: ...`, `GROW 8B: ...` (the TAKE line follows the selected LENGTH). "Allowed" is not "possible now". A line names what blocks
  (`BLOCKED BY CURRENT 1  LIVE UNDO 3`); with no blocker it says that the lines are slots only and that D also checks style and edits.
* Product view: when a run exists only through allowed slots the TO line reads `REPLACES ALLOWED` (not `FREE`); with none it reads `NO SLOTS: R`. A G refused for lack of a consecutive run says `NO ROOM: R=ALLOW REPLACE`.
* Marking starts no generation and erases nothing.

## Refusal messages (each cause is different)

| Cause | Toast |
|---|---|
| style cannot grow (`NotAdmitted`, e.g. House) | `TRY ANOTHER TAKE: G` |
| kept phrase edited (`EditedSinceGeneration`) | `EDITED TAKE: PRESS G` |
| cycle already published | `ALREADY GROWN` |
| no consecutive slots (`NoSafeSlots`, or a refused G) | `NO ROOM: R=ALLOW REPLACE` |

## F1 (separate commit `679a28fa`, before this slice)

The kept-phrase check no longer relies on the length of the latest published cycle: the origin sidecar is consulted only while it still describes A (same first slot). Tests: an unedited A gets no false
`EditedSinceGeneration`; a really edited A is still refused; repeated GROW stays blocked by `CycleAlreadyPublished`. The regression fails without the fix.

## Host results (`tests/run_0_9_14_pml_e_tests.sh`, in CI)

Plain R opens and leaves the view, Alt/Ctrl/Meta+R do not; the footer names R; the grid shows the real holders (CURRENT on slot 13, the rest of the receipt, unused orphans); the confirmation wording and cancel;
allow/cancel per slot and the held-slot refusal; results NO/NO -> AFTER YES after four allowed slots with GROW 8B still NO; G refusal points to R; allowed slots make the product view say so and G uses them and
consumes the marks; the four refusal messages differ; entering the page resets the view. Sensitivity: removing the confirmation, and letting R open with modifiers, each make the suite fail.

## Not done

* **Device acceptance** of the original path (House -> rave -> manual edit -> delete the bad one -> new generation), then Undo, save and cold start, with heap and stack high-water recorded.
* Preview does not know whether GROW will publish 4 or 8 bars for a given take (it shows both) and does not check style or edits (D reports those).
* The cycle-flag rule for a regrow (F3) and variants by salt (V1) are separate slices. Memory remains a conditional pass until the exact-SHA FS1B and the device measurement are recorded below.
