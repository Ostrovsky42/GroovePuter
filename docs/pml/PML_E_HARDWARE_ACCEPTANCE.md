# PML-E — hardware acceptance candidate and procedure

Status: **candidate `be34837a` prepared; NOT accepted.** No device run has happened for this candidate. Superseded: `839f4563` (flashed first; ALLOW REPLACEMENT layout clipped by the shell HUD) and `b4346c8f` (layout fix; but the grid-first flow was too hard to use on the device: six steps and internal vocabulary). `be34837a` replaces it with a one-question MAKE ROOM flow. Memory remains a **conditional pass**, not GREEN.
Scope: PML-C (session-only allow-replacement marks) with the ALLOW REPLACEMENT UI. **Not closed by this checkpoint:** repeated GROW, development of a manually edited A,
variants by salt, an exact preview of the selected GROW, musical expressiveness.

## 1. Candidate record

| Item | Value |
|---|---|
| Full SHA | `be34837ad7b5d8d9edcfa4a765e74331360eb681` (dirty tracked files: 0) |
| Branch | `feature/20260929-m0-musical-play-baseline` (not merged to `dev`) |
| Commits in this candidate line | F1 `679a28fa`, UI `fa3947b5`, label `8f518689`, probe `839f4563`, docs `a96e4408`/`a3fe5040`, layout fix `b4346c8f`, session ledger + MAKE ROOM engine, one-question UI, docs (up to `be34837a`) |
| Last **full** serial run | 18/18 green at `fa3947b5` (D1-A/B/B1/C/C1, cycle, mix, PML-C, PML-E, P0-B, M0-A, PMB-P1, host, unified slots, SDL, ADV, SEQTRAK, FS1B) |
| Gates run **at this SHA** | PML-E UI, cycle, host, unified slots, SDL, ADV, SEQTRAK, FS1B, probe build: all exit 0 (`logs/candidate_be34837a_gates.txt`). The other ten gates were **not** re-run at this SHA: every commit after `fa3947b5` changes `phrase_page.cpp` (label, flag-only probe lines, layout) and its test, or documents. |
| FS1B | `=== FS1B dynamic-FatFs Cardputer build PASS ===` for both images below |
| Static DRAM | **190128 B** of a 191488 B budget, headroom **1360 B** (+144 B against `b4346c8f`: the 136 B session ledger); `.dram0.data` 37080 B, `.dram0.bss` 153048 B; identical for both images (the probe adds no static RAM) |
| Memory status | **Conditional pass.** The check passes under a *provisional exception*: its ceiling is a rollback to an older repository ceiling and "not a universal hardware safety boundary"; threshold-rule items 1-4 are met and **items 5-7 (hardware minima, declared reserves, the deriving calculation) are pending**. |

### Images (same SHA, separate builds)

| Image | Purpose | ELF sha256 | BIN sha256 | Files |
|---|---|---|---|---|
| **Default** | the product image; `[PML-PROBE]` strings absent (verified: 0 in the ELF) | `1b6324b22c4e531a3bde1657e13b2ec093edd7d744bb928a8cc260b4462b2969` | `fa1b1c2456d16585ff5b93ef6fdba9ef800072745ba66bd81f55b1f4ef71776a` | `build/candidates/default-be34837a/` |
| **Probe** | the device-acceptance image: same code plus `-DGROOVEPUTER_P0_CYCLE_PROBE` (1 `[PML-PROBE]` string in the ELF) | `42a97990bed18cf53e1a0500194167e12708d5164cd65ae0931d15342e292ced` | `34b4b2d349b502dfb1e9af306943c7601ef1068148d1cf42daa4e452179d2d8e` | `build/candidates/probe-be34837a/` |

Logs: `docs/pml/logs/candidate_be34837a_gates.txt`, `..._default_image.txt`, `..._probe_image.txt` (DRAM check verbatim); the superseded candidates' logs stay beside them. Older evidence: `fs1b_pmlc_dram_check.txt` (PML-C build).
Flash only the image the owner names; the probe image is the one for this acceptance.

## 2. Device procedure (owner)

Serial monitor on the USB port shows `[PML-PROBE] <action> stackMinFreeBytes=… internalFree=… largestBlock=… minEverFree=…` after each action below
(`R-open`, `R-room`, `R-allow`, `R-cancel`, `G`, `UNDO`) and `[P0-CYCLE-PROBE] …` after each D. `stackMinFreeBytes` is the minimum free stack **since the task started**, so a later line
also covers every earlier draw and handler. Copy the lines into the report.

**A. The original path (House).**
1. House / rave as usual; MATERIAL, LENGTH 4B. Press G until a TAKE is refused with `NO ROOM: R MAKES ROOM`; the product line reads `NO SLOTS: R REUSE n` (or `NO SLOTS: R`).
2. Edit one generated pattern by hand; in Song delete the rows of the unwanted TAKEs (`Backspace` on the cells or `Ctrl+M`).
3. **R**: the page asks `REUSE n UNUSED TAKES?` (takes generated in this session, not in Song, not edited by you). The hand-edited one must **not** be counted.
4. **ESC** answers no: nothing changes. **R** again, **ENTER** answers yes: `ROOM FOR 4B: PRESS G`; the product line reads `REPLACES ALLOWED`.
5. **G**: a new TAKE is committed into the reused slots and plays with all its notes.
6. **Undo** (Ctrl+Z): `UNDO: MATERIAL`; the replaced slots are empty, the old content does **not** return, the Song rows are back as before the TAKE.
7. **R**, then **S**: the slot grid (`~` unused, `*` allowed, letters = held). Move with Left/Right; ENTER on `~` asks the first time, ENTER on `*` cancels; ENTER on a held slot names the holder. R/ESC back.
8. Edited slot: allow a slot in the grid, edit it by hand in the pattern editor, reopen the grid: it reads `EDITED, ALLOW AGAIN?` and G does not replace it.
9. Save the project, power off, cold start, load: the new content is present, the permissions **and the list of generated takes are gone** (R now says `NOTHING TO REUSE` until a new TAKE is made; the grid still reaches older slots), the old content is absent.

**B. A successful GROW (separate scenario, admitted material).** Techno, STYLE REWORK, G, D until a GROW is published (House cannot GROW: `TRY ANOTHER TAKE: G`); Undo once; D again.

**C. Memory.** Record the `[PML-PROBE]` and `[P0-CYCLE-PROBE]` lines from every step above, and note any audio dropout or unresponsive UI.

## 3. What the acceptance does and does not decide

* Decides: the PML-C/PML-E behaviour on the device, Undo correctness, save/cold-start, and whether the recorded stack/heap minima leave room.
* Does **not** by itself turn memory GREEN: the provisional exception and pending items 5-7 stay until the owner's threshold derivation exists.
* **Layout was verified in the SDL emulator** (window 480x270 = 2x of 240x135): the shell paints its HUD strip over content line 7, so the ALLOW REPLACEMENT views use lines 0-6 only. Host tests cannot see this (text width is approximated); the device screen is still the final judge.
* An exact preview of the selected GROW (will it run, and with 4 or 8 bars) is still an open requirement; the two `SLOT SPACE` lengths are a capacity preview only.
