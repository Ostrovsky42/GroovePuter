# 0.9.9 UI FINAL PRODUCT PASS

Status: production UI candidate. Draft/checkpoint only; do not merge until exact-head CI and hardware acceptance are complete.

## Purpose

Turn the frozen I1 phrase/Song integration into a practical Cardputer workstation workflow without creating new musical or transport owners:

```text
generate multi-bar phrase
  -> Song
  -> PLAY
  -> Synth NOTES follows the physical pattern that actually sounds
  -> STOP
  -> last sounding physical pattern remains selected
  -> edit it directly
  -> PLAY again
```

This checkpoint also separates Generated Phrase request length from legacy PhraseCore capture length and exposes a small read-only product snapshot of the most recently accepted Generated Phrase.

## Exact ancestry

```text
I1 FINAL
fb30bbb93f739d8b3e4514ae931cbc73fc8eeb09
  -> UI-P0 ownership audit
b64a26278d225d9842d6f19c130fcc9015216a1e
  -> this product UI branch
agent/20260828-09-0.9.9-ui-final-product-pass
```

UI-P0 was intentionally used as the immediate parent so its ownership conclusions and source guards remain part of the branch history. Frozen H1/W1R/H2R/P1R/I1 musical semantics are not redefined here.

## Product ownership

### Generated Phrase Length

New session/request owner:

```text
src/state/phrase_generation_request_state.h
```

Domain presented by the UI:

```text
1 / 2 / 4 / 8 bars
```

It owns only the user's requested extent for the next `GeneratedPhraseSong::generate(...)` command. It does not own admissibility. Frozen P1R phrase-length policy may return a typed rejection.

It is distinct from:

```text
PhrasePage::capture_length_   = legacy PhraseCore capture extent
Scene::feel.patternBars       = FEEL CYCLE / local transport-repeat window
```

No rejected request is silently rewritten to another accepted musical request.

### Generated Phrase product snapshot

`src/state/generated_phrase_product_state.h` holds a small fixed-capacity session read model. It stores only evidence already produced by I1 plus the exact publication coordinates needed to navigate back to physical material:

```text
last outcome
last requested bars
accepted bars
Song slot / Song start row
page + first local slot
phraseGenerationIdentity
progression id
harmonic event count
```

It is not Phrase IR and is not a new musical policy owner. No dynamic allocation or phrase-sized buffer is introduced.

The three product outcomes remain distinct:

```text
ACCEPTED
TYPED REJECTION
EXECUTION FAILURE
```

A rejected request leaves the previous accepted snapshot inspectable; it does not fabricate replacement material.

### Semantic bar -> physical editor

For the accepted Generated Phrase, UI navigation uses the exact publication mapping captured from the successful I1 result:

```text
semantic phraseBarOrdinal
  -> accepted.firstLocalSlot + phraseBarOrdinal
  -> page / bank / slot physical pattern

semantic phraseBarOrdinal
  -> accepted.songStart + phraseBarOrdinal
  -> authoritative Song row
```

There is no `% phraseBars`, `% patternAddress`, or inferred identity.

ENTER on a selected semantic bar is accepted only while stopped. It reuses existing MiniAcid Song-selection APIs; those APIs call the existing I1 `applySongPositionSelection()` owner. The UI does not write a second synth pattern mapping.

## Phrase page

The default `PHRASE` surface is now the Generated Phrase product view.

It shows:

```text
LENGTH       requested 1B / 2B / 4B / 8B
DEPTH        existing P1 / P2 / P3 generation-request owner
FEEL         current FEEL CYCLE, explicitly separate
BARS         semantic bar selector/current playback bar
LAST         ACCEPTED / REJECTED / EXEC FAILURE
ID           phraseGenerationIdentity for P1R material
PROG         existing progression id evidence
HARM         existing harmonic-event count evidence
BAR n/N      selected semantic bar -> physical page/bank/slot + Song row
```

`V` toggles to the retained `PHRASE CORE` workspace for the older capture/derive/write workflow. Its `capture_length_` remains a separate owner.

Product controls:

```text
UP / DOWN       requested Phrase Length
LEFT / RIGHT    semantic bar selection
P               canonical P1/P2/P3 DEPTH
G               Generated Phrase command through GeneratedPhraseSong
ENTER           while stopped: focus selected semantic bar/physical target
Ctrl+arrows     destination Song row
V               PRODUCT / PHRASE CORE
```

## Genre / Apply

GENRE remains the first page of the existing `GENRE -> FEEL` workflow.

Product labels are aligned with existing owners:

```text
GENRE
RECIPE
RHYTHM
DEPTH
APPLY
```

The `DEPTH` row and plain `P` shortcut call the same `generation_request_state` owner. `G` remains the existing canonical generation/materialization command. APPLY still selects the existing profile/materialization publication behavior; no fourth generation path was added.

## Follow playback and STOP

No new transport cursor or mapper is introduced.

Frozen I1 path remains:

```text
transport position
  -> Song row
  -> applySongPositionSelection()
  -> SceneManager current Synth bank/pattern
  -> PatternEditPage::syncSongPatternContext()
  -> Synth NOTES physical editor
```

While the user remains on Synth A or Synth B NOTES in Song mode, the editor mirrors the physical bank/pattern selected by playback. Navigating to another application page does not force the UI back to the synth page and does not change transport ownership.

STOP remains engine-owned:

```text
STOP
  -> current Song playhead is retained
  -> current physical Synth selection is not restored to pre-PLAY selection
  -> NOTES therefore remains on the last sounding physical pattern
```

The user can immediately edit that physical pattern. No shadow Song-preview pattern is created.

## Physical pages

The existing storage contract remains authoritative:

```text
kMaxPages = 16
pattern identity = page + bank + slot
```

This product pass does **not** shrink storage to eight pages. Hardware acceptance exercises pages 1..8 as a minimum practical set; page 16 should also be spot-checked to prove the full storage range remains reachable.

## Redraw/performance policy

No 96-PPQN full-screen animation is added. Existing NOTES follow reads the authoritative physical selection from MiniAcid and existing dirty-region/display code remains responsible for rendering.

The new Phrase product page uses only a small fixed snapshot and compact bar cells. It does not allocate a phrase-sized UI buffer and does not scan physical storage each transport tick.

## Cross-bar lifetime UI

Not implemented in this branch.

Corrected C2/R1/hardware lifetime semantics are not part of this branch ancestry. This checkpoint therefore deliberately does not infer held cross-bar notes from step 15, pitch equality, slide, gate length, or visual continuity.

When R1 is frozen on the final ancestry, lifetime visualization may be replayed as a separate narrow consumer of the authoritative semantic/runtime lifetime state.

## Known product coupling

I1 generated-phrase publication uses `forceSingleBarRows` and may normalize:

```text
scene.feel.patternBars = 1
```

That remains a known FEEL CYCLE/publication coupling. This UI pass does not change frozen I1 publication semantics. The Phrase page explicitly distinguishes `FEEL CYCLE` from `PHRASE LENGTH` so the behavior is visible rather than silently reinterpreted.

---

# Cardputer ADV hardware test

## Purpose

Validate the finished workstation loop on real Cardputer ADV hardware: Generated Phrase navigation, Song playback follow, STOP-in-place editing, Synth A/B symmetry, full physical page identity, and persistence of physical patterns/Song references.

## Hardware list

- M5Stack Cardputer ADV, ESP32-S3
- USB-C data cable
- Optional headphones / powered speaker through the normal GroovePuter audio path
- Optional Yamaha SEQTRAK only for the existing MIDI-only build/route check

No external I2C hardware is required for this UI test.

## Wiring

For the normal internal-display test:

```text
Cardputer ADV <-> USB-C host
```

No PORT.A changes are required.

If unrelated expansion hardware is connected, preserve project invariants:

```text
PORT.A SDA = GPIO2
PORT.A SCL = GPIO1
M5Unit-Scroll = 0x40
CardKeyboard = 0x5F
PAHub = 0x70
Scroll uses Wire, never Wire1
```

If an external ILI9488 test configuration is used, initialize it before `M5Cardputer.begin()` as required by the existing hardware contract. This UI branch does not modify that initialization.

## Build / Flash

From the repository root on the UI product branch:

```bash
bash tests/run_0_9_9_ui_final_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

If the serial device differs, replace `/dev/ttyACM0` with the actual Cardputer ADV CDC device.

## Expected behavior

### 1. Phrase request ownership

Open SONG -> PHRASE. PRODUCT view is the default.

Use UP/DOWN and verify:

```text
LENGTH 1B -> 2B -> 4B -> 8B
```

`FEEL`/`FEEL CYCLE` is shown separately and is not used as the requested Phrase Length.

Press `P`; DEPTH changes through the existing P1/P2/P3 owner. GENRE's DEPTH row must show the same value.

### 2. Generate and inspect

Choose 4B or 8B and press `G`.

On success, Phrase shows:

```text
LAST ACCEPTED
BARS [1] [2] ...
ID <stable generation identity>
PROG <id>
HARM <event count>
BAR n/N -> <page+bank+slot> / Song row
```

A typed length rejection must display as `REJECTED`, not `EXEC FAILURE`. An operational failure must display separately.

### 3. Semantic bar navigation

With transport stopped, use LEFT/RIGHT to choose a bar and ENTER.

Expected:

```text
selected semantic bar
  -> exact generated Song row
  -> existing Song selection owner
  -> exact physical pattern/page
```

The mapping must not wrap by pattern-address modulo.

### 4. Synth A playback follow

Arrange/generate a Song where consecutive rows reference visibly different Synth A patterns/pages.

Open Synth A NOTES and press PLAY.

At each Song-row boundary, the displayed notes must switch to the physical pattern that actually sounds on Synth A. A moving step/playhead alone is not sufficient: note content must change with the sounding physical pattern.

Navigate to another top-level page while playback continues. Transport must keep running and the UI must not force navigation back to Synth A.

### 5. STOP -> edit -> PLAY

Return to Synth A NOTES, let playback enter a distinctive bar, then press STOP.

Expected immediately after STOP:

```text
Song row = last sounding row
Synth A bank/pattern/page = last sounding physical pattern
NOTES content = that physical pattern
```

Change one note on the stopped page, then PLAY again. The edit must be audible from the same physical Song reference.

There must be no return to the pattern selected before PLAY.

### 6. Synth B symmetry

Repeat the previous playback-follow and STOP/edit/PLAY test on Synth B. Synth A must continue playing normally while viewing/editing Synth B and vice versa.

### 7. Physical pages

Verify at least pages 1..8 with distinct note content and Song references. Each page must remain independently editable and audible.

Spot-check page 16 as well. The branch must preserve the existing 16-page capacity.

### 8. Save / reboot / load

Save the project using the existing project workflow, reboot, and load it.

Verify:

- edited physical pattern contents are restored;
- Song references still point to the expected page/bank/slot;
- Synth A/B engine/patch persistence behaves as before;
- playback again follows the restored physical patterns.

The session-only Phrase product snapshot may start empty after reboot; physical pattern/Song persistence remains authoritative.

## Serial / diagnostics

Use the existing 115200 baud USB CDC serial output if debugging is required. No new per-tick serial logging is added by this UI pass.

Unexpected regeneration or Song writes while merely navigating UI are failures.

## Troubleshooting

- If NOTES shows the correct playhead but wrong notes, verify `applySongPositionSelection()` updates the expected Synth bank/index and `syncSongPatternContext()` mirrors them.
- If STOP jumps backward, verify STOP does not restore `patternModeSynthPatternIndex_` / `patternModeSynthBankIndex_` while remaining in Song mode.
- If ENTER on Phrase changes playback while transport is running, treat it as a bug; product bar focus is intentionally stopped-only.
- If a generated request reports `REJECTED`, do not treat it as a firmware crash. Verify the requested genre/length against the frozen phrase-length policy.
- If generated Phrase changes FEEL CYCLE to 1B, this is the documented I1 `forceSingleBarRows` coupling, not evidence that FEEL CYCLE owns Phrase Length.
- If a page above 8 appears unavailable, verify the existing `kMaxPages=16` storage/navigation path; do not reduce storage capacity as a workaround.

## Acceptance checklist

- [ ] UI branch has exact UI-P0 ancestry.
- [ ] Generated Phrase LENGTH cycles exactly 1/2/4/8.
- [ ] PhraseCore capture length is independent from Generated Phrase length.
- [ ] FEEL CYCLE is visibly distinct from Phrase Length.
- [ ] GENRE DEPTH row and plain P use the same state owner.
- [ ] ACCEPTED / REJECTED / EXEC FAILURE are visibly distinct outcomes.
- [ ] 4B Generated Phrase exposes four semantic bars; 8B exposes eight.
- [ ] Selecting bar n and ENTER while stopped resolves its exact Song row/physical pattern.
- [ ] Synth A NOTES follows actual sounding pattern content during Song playback.
- [ ] Synth B NOTES follows actual sounding pattern content during Song playback.
- [ ] Leaving a synth page does not change transport or force navigation back.
- [ ] STOP leaves the last sounding row/page/pattern selected.
- [ ] Immediate edit after STOP changes the exact physical material that was heard.
- [ ] PLAY after edit audibly uses the edit.
- [ ] Pages 1..8 are independently editable/playable and retain distinct content.
- [ ] Page 16 remains reachable; 16-page storage is preserved.
- [ ] Save/reboot/load restores physical patterns and Song references.
- [ ] No new cross-bar lifetime visualization appears before corrected R1/hardware acceptance.
- [ ] No full-screen redraw occurs on every transport tick.
- [ ] Cardputer ADV build and fixed-DRAM gate pass.
- [ ] SEQTRAK MIDI-only build remains green.
