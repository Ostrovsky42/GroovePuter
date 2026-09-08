# PATTERN / PHRASE PRODUCT CLOSURE — LIVE CENSUS

Date: 2026-09-08

Checkpoint: PPC-0 — characterization only

Production delta in this commit: NONE

## Authoritative start

The live remote state was inspected before opening this checkpoint.

- `dev_0.9.10` is still the P2 integration line at `af6d76a1c9dfca0ef721adb0d2eca6f11eee8198`.
- P3 lifetime line is `feature/20260904-02-0.9.10-pattern-phrase-p3-phrase-lifetime @ aded0e183a934f78623030226b67b5d0b598648b`.
- UI Constitution V1 is an ancestor of the current material line.
- The current integrated product/material line is `feature/20260907-01-u4b6-make-phrase-entry @ 2e7e7d0e78f75fc6be1d7d67e1794bc75b0518ca`.
- Draft PR #443 is a separate recovery-only stack on that M4 head. It is not pulled into this checkpoint because its stated scope is inherited test liveness, not product semantics.
- Memory R0/R1 remains a separate workstream.

This checkpoint branch is cut from exact M4 head `2e7e7d0e78f75fc6be1d7d67e1794bc75b0518ca`.

Evidence level: CHARACTERIZED from live GitHub refs and source at the exact base. No physical hardware claim is made.

## Critical finding before production changes

The live line no longer contains only the historical P3 product model.

There are now two overlapping semantic vocabularies:

1. P3 / UI Constitution vocabulary:

```text
Synth A/B
  active sequenced source = PATTERN xor PHRASE
  Phrase = per-voice editable RuntimeSynthEventBuffer
```

2. M1-M4 material vocabulary:

```text
MaterialSlot
  kind = PATTERN | MELODY

higher hierarchy
  MATERIAL -> PHRASE -> SONG
```

`MiniAcid::SequencedSource::Phrase` currently survives as a compatibility API, but its storage authority is `activeMaterial_[voice].kind == MaterialKind::Melody`.

This is not merely a label difference. `src/state/material_slot.h` explicitly defines Pattern/Melody as two representations of one material slot and describes Phrase as the higher musical object. The canonical `PLAN.md`, independently, defines Phrase as bounded multi-bar musical material in the `step -> bar -> phrase -> section -> song` hierarchy.

Therefore the requested frozen model "PHRASE is the editable per-synth source" and the live M1-M4 model cannot both be promoted to final product truth without an explicit ownership/naming decision.

No production rewrite is authorized by this census.

## Ownership census

| Area | Live owner/path | State | Evidence / conflict |
|---|---|---|---|
| PATTERN source owner | `Scene` / active `SynthPattern`; playback projection in `RuntimePatternEventBank` | Implemented | Pattern playback uses `activePatternRuntimeEvents(voice)` when active material kind is Pattern. |
| Editable historical PHRASE source owner | `MiniAcid::currentPhrase_[2]` (`RuntimeSynthEventBuffer`) | Implemented but semantically transitional | M1-M4 now names the same active representation `MaterialKind::Melody`; the old `SequencedSource::Phrase` maps to it. |
| Active source selector | `MiniAcid::activeMaterial_[voice]` | Implemented | One published per-voice authority; audio path branches on `MaterialKind::Pattern` vs `MaterialKind::Melody`. |
| Legacy source API | `SequencedSource::{Pattern,Phrase}` over `activeMaterial_` | Partial / transitional | API terminology disagrees with M1-M4 storage terminology. |
| Pattern runtime projection | `RuntimePatternEventBank` + `refreshPatternRuntimeEvents` / `rebuildPatternRuntimeEventBank` | Implemented | Bounded runtime projection remains separate from Pattern owner. |
| Editable event representation | `RuntimeSynthEventBuffer currentPhrase_[2]` | Implemented | Fixed capacity; one buffer per supported voice. |
| Pending next material | `PendingMaterial pendingMaterial_[2]`, one fixed heap allocation per voice at construction | Implemented on M4 candidate | Preparation is control-side; boundary activation copies prepared value and publishes active material. Device largest-block acceptance remains separate. |
| MAKE PHRASE engine primitive | `MiniAcid::makePhrase(voice)` | Implemented | Failure-atomic projection of current Pattern into `currentPhrase_`; rejects re-conversion if already Melody. |
| MAKE PHRASE product action | No explicit Notes-surface command found | Contradictory / missing | `PhraseSourceToggle::toggle()` calls `makePhrase()` implicitly when no editable buffer exists. UI Constitution explicitly says source change must not implicitly convert and MAKE PHRASE remains explicit. |
| Source switching UI | Notes `ALT+R`; MORE `SRC` row is rendered | Partial / contradictory | `ALT+R` calls `PhraseSourceToggle`. The MORE `SRC` row is focusable/rendered, but `adjustFocusedElement()` has no branch for it on the exact base, so the advertised row has no Left/Right source action. |
| User-visible source wording | Pattern side says PATTERN; event editor says MELODY / SOURCE: MELODY | Contradictory | UI Constitution and requested checkpoint use PATTERN/PHRASE; M1-M4 material domain uses PATTERN/MELODY. |
| LENGTH owner | `RuntimeSynthEventBuffer::lengthTicks`; helpers in `RuntimePhraseEdit`; engine `setPhraseLength()` | Partial | 1/2/4/8 validation exists. No accepted direct Notes UI LENGTH control was found in the exact base. |
| GRID owner | `PhraseNotesCursor::State::grid` | Implemented as editor-local state | 1/8, 1/16, 1/32 map to 48/24/12 ticks. Insert/resize use the grid; changing grid changes cursor snap, not `lengthTicks` or transport. |
| GRID product visibility | `G` cycles grid and shows transient `STEP 1/8|1/16|1/32` toast | Partial | No persistent GRID state is visible in the Phrase header; Pattern-side `G` is Generate, so the same key changes meaning by source. |
| Phrase edit mutation | `prepare complete buffer -> commitRuntimePhraseEditWithUndo -> audio_guard_ -> global undoOwner -> complete-buffer commit` | Implemented | Insert/delete/pitch/length-of-note/join use the bounded prepare/commit path. |
| Pattern generation/edit mutation | Pattern Undo payload + `audio_guard_` + `refreshPatternRuntimeEvents()` | Implemented for inspected generation path | Generated Pattern is committed and runtime projection refreshed under existing guard. |
| Playback read path | `processSequencerEvents`: `activeMaterial_[voice].kind` selects Pattern runtime bank vs event buffer | Implemented | UI source selector and playback authority ultimately meet at `activeMaterial_`. |
| Cross-bar lifetime owner | `RuntimeSynthPlaybackState::releaseAtSubtick_` | Implemented structurally | Release deadline is absolute subtick time, not bar-local. `releaseDue()` has no bar-boundary special case. P3 behavioral regression still must be re-run on the closure head. |
| Undo owner | global `GroovePuterUndo::undoOwner()` | Implemented for inspected Phrase edits and source toggle | No second Phrase undo universe found. |
| Persistence of M1-M4 Melody material | M2/M2c disk-backed Melody store + material kind in page format | Implemented on current material line | This persistence model is outside the historical session-only P3 model and is part of the semantic conflict that must be resolved rather than silently discarded. |

## Time model census

Live constants and behavior inspected on exact base preserve:

```text
96 PPQN
384 ticks / bar
16 Pattern source steps
24 ticks / Pattern step
Phrase edit grids:
  1/8  = 48 ticks
  1/16 = 24 ticks
  1/32 = 12 ticks
```

`RuntimePhraseEdit::lengthTicksForBars()` admits only 1/2/4/8 bars.

`RuntimePhraseEdit::changeGrid` equivalent lives in the cursor layer: it snaps only cursor position to the new quantum. It does not rewrite existing event positions, Phrase length, or transport duration.

Evidence: CHARACTERIZED.

## Edit -> playback causality census

Inspected Phrase edit path:

```text
user edit
  -> prepare full RuntimeSynthEventBuffer copy
  -> validate
  -> commitRuntimePhraseEditWithUndo
  -> existing audio_guard_
  -> global Undo owner transaction
  -> RuntimePhraseEdit::commit(full value)
  -> currentPhrase_[voice]
  -> processSequencerEvents reads same buffer when activeMaterial.kind == Melody
```

This path is materially stronger than the current product affordance around it.

The current code does not require a second publish mechanism for ordinary in-RAM edits. Whether the supplied `AudioGuard` is exactly the project `AudioMutationGate` at every construction site remains an integration assertion to keep in the PPC suite; this census does not infer it from the type alias alone.

Evidence: CHARACTERIZED; full integration gate NOT YET PROVEN.

## Cross-bar characterization

`RuntimeSynthPlaybackState::acceptOnset()` computes:

```text
releaseAtSubtick = absoluteStartSubtick + durationSubticks
```

`releaseDue(absoluteSubtick)` releases only when that absolute deadline is reached. No `barTick == 0` release exists inside this lifetime owner.

Therefore a note beginning at tick 360 with a duration reaching tick 456 is representable without a forced release at tick 384.

This is architecture evidence, not a fresh exact-head behavioral run. P/P-5 must still execute the real engine path on the closure branch before claiming GREEN.

Evidence: CHARACTERIZED, not yet TESTED on PPC head.

## First real semantic gap

The earliest product-level contradiction that can be tested without choosing a new architecture is:

```text
UI Constitution:
  source change does not implicitly convert material
  MAKE PHRASE remains explicit

live PhraseSourceToggle:
  PATTERN + no editable event buffer
    -> toggle source
    -> engine.makePhrase()
    -> creates material and switches source
```

That behavior must receive a real RED before any production change.

Desired RED contract for this checkpoint:

```text
PATTERN source
Phrase/Melody working buffer empty
Pattern material A

SOURCE TOGGLE

must NOT create independent material implicitly
must NOT mutate Pattern
must NOT silently become editable Phrase/Melody

MAKE PHRASE / promotion is a separate explicit action
```

This RED is independent of the unresolved final product noun (PHRASE vs MELODY): both the requested checkpoint and UI Constitution prohibit hidden conversion.

## Stop condition triggered

A production implementation that chooses whether the user-visible editable object is:

A. historical P3 `PHRASE` (per-voice source), or
B. M1-M4 `MELODY` (material representation) while `PHRASE` is a higher arrangement object

would select an ownership/domain model, not merely wire UI.

That decision is explicitly outside an automatic product-closure patch.

Allowed before the decision:

- preserve current runtime architecture;
- add characterization and behavioral REDs for contradictions that are invalid under both models;
- prove existing source/lifetime/grid/edit causality behavior;
- build Lo-Fi verification harness only where it does not depend on the disputed noun/ownership boundary.

Not allowed before the decision:

- rename MaterialKind or persisted payloads;
- collapse M1-M4 Melody back into historical Phrase;
- create a second Phrase object next to Melody;
- route Song/Section/Phrase references differently;
- replace `activeMaterial_` with `sequencedSource_` again;
- add bidirectional synchronization.

## Lo-Fi status at PPC-0

No Lo-Fi production changes have been made.

Existing GF2 evidence already proves that phrase-law execution can create multi-bar differences in some generated material, but that is not sufficient for the requested Lo-Fi 30-second adversarial report.

The deterministic Lo-Fi harness remains planned after the Pattern/Phrase/Material ownership decision or, if kept domain-neutral, may be added as a measurement-only tool that reports generated event structure without changing generator semantics.

Evidence: NOT PROVEN for the requested 30-second PPC scenario.

## Memory / realtime status

M4 commit provenance reports Cardputer target fixed DRAM at 190808 bytes and two 1284-byte pending buffers allocated once at construction. This census has not independently rebuilt the ELF and does not promote those numbers to a new measurement.

This checkpoint must not modify memory thresholds or the Memory R1 workstream.

Evidence: inherited CHARACTERIZATION; fresh PPC ELF measurement NOT YET PROVEN.

## PPC-0 conclusion

The runtime is already close to the requested musical instrument behavior, but the live branch has crossed a semantic migration boundary since P3.

The next safe action is not a runtime rewrite. It is:

1. install a behavioral RED for the currently illegal implicit conversion in `PhraseSourceToggle`;
2. keep production unchanged;
3. resolve the product/domain naming and ownership boundary `PATTERN/PHRASE` vs `PATTERN/MELODY + higher PHRASE`;
4. after that decision, continue minimal GREEN wiring for explicit conversion, LENGTH visibility/control, persistent GRID visibility, exact edit/playback causality and full P/P acceptance.
