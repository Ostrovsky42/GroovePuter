# CALL / RESPONSE partner — phase 4 (BARS 4)

Status: implemented on `genre/cell-source` (on top of 6dec977b), host-tested, Cardputer build PASS,
**not hardware-tested, not listened to**. PARTNER `BARS` now offers 2 (default), 1 and 4. With BARS 2
or 1 the output is byte-identical to phase 3 (checked, see below).

## Estimate (before the code)

| Item | Where | Static DRAM | Loop stack |
|---|---|---|---|
| Capture ring 16 → 32 events | `PhraseCapture::events_` (singleton listener) | +256 B at 16 B/event; **+128 B** after shrinking the event to 12 B (NoteOff stored as a uint16 delta from the onset) | — |
| `Phrase::notes` 16 → 32 | `listener.loopPhrase_` (LOOP mode copy) | **+64 B** | +64 B per `Phrase` (72 → 136 B; 2–3 copies live in `serviceCallResponseLog` / `planPerformanceReply`) |
| `ResponsePlan` 16 → 32 notes | responder `slots_` (12 B each) | **+192 B** | `ResponsePlan` 68 → 132 B, `int notes[]` 64 → 128 B |
| 64-bit onset mask | `MotifDescriptor` | 0 | +8 B |
| 16 strong-beat chords, 16-bit masks | `ResponseHarmony` | 0 | 18 → 36 B, plus +24 B lookahead scratch |
| Log buffers | `[CR]` 160 → 224, `[CR-A]` 320 → 576 | 0 | +320 B |
| **Total** | | **+384 B** (target ≤ ~400) | ≈ +0.7 KB on a 32 KB loop stack |

Note on the previous estimate ("+192 B per 16 events"): the capture event was 16 B, so 16 more events
were +256 B, and the responder slots (+192 B) and the LOOP phrase copy (+64 B) were not counted. Without
the event shrink the total would have been 512 B.

Measured (`Cardputer DRAM globals`, `scripts/build_cardputer_dynbuffers.sh`):
**185412 → 185796 B (+384 B)**, budget 191488, headroom **5692 B** (≥ 4 KB). All of it lands in
`.dram0.data` (the listener singleton is initialised data because of the responder's non-zero
default target), `.dram0.bss` unchanged at 142456. Note: the HEAD baseline is 185412, not the 185316 of
the phase-3 document — the partner modes commit (6dec977b) added 96 B.

## Decisions

| Question | Decision | Why |
|---|---|---|
| Default | **2 BARS** unchanged | owner rule; phase 0–3 behaviour |
| Cycle order | `</>`/ENTER forward 2 → 1 → 4 → 2, backward 2 → 4 → 1 → 2 | 2 → 1 stays the phase-3 forward step (tests and muscle memory) |
| Capacity | 16 notes for 1 and 2 bars (unchanged, also the overflow/`drop=` behaviour), **32 for 4 bars** | same density cap (8 notes per bar); a 2-bar call with 17+ notes still drops exactly as before |
| Event shrink | `offStep` (int32) → `offDelta` (uint16, from the onset) | saves 128 B; any NoteOff past the window is clamped to the window end anyway (≤ 64), so semantics are identical |
| Mask | `MotifDescriptor::onsetMask` uint64, `ResponsePlan::onsetMask()` uint64 | 64 sixteenths; `[CR]` log prints 32 bits for 1/2 bars (identical) and 64 bits `b1_b2_b3_b4` for 4 bars |
| Harmony | `ResponseHarmony::kBeats` 8 → 16, masks uint16 | one chord per beat over 4 answer bars; for 1/2 bars only the first 8 beats are filled (as before, so the `chord=A…C…` log is unchanged) |
| Lookahead span | 4 bars: offset ≤ 80 steps, up to **4 Song rows**; 1/2 bars keep ≤ 48 / 2 rows | new optional parameters `maxRowsAhead` (`shiftHarmonyPosition`, `legacyHarmonyAhead`, `graphHarmonyAhead`) and `maxOffsetSteps` (`patternHarmonyAhead`), defaults = phase 2 |
| Genre seed | `responseSeed` keeps `onset & 31` | bars 3–4 fold onto bars 1–2 in the seed; 1/2-bar seeds (and genre endings) stay identical |
| MINIMAL ending | thins every answer bar after the first (bars 2–4) | the rule is "step ≥ 16, off the strong beats" — unchanged code |
| One-note fix | a one-note CALL on step 63 is answered on step 62 (delay 1) | the phase-3 rule, generalised by the window length |

## Risks

- **Song boundaries of the lookahead.** Bars 3–4 of the answer are up to 4 Song rows ahead. Every row is
  still checked: inside the generated recipe (`recipe.bars ≤ 8`), ≤ `lastSequentialRow` (loop end, Song
  end, reverse), and confirmed by the in-memory Song participants (same material id and version). A beat
  that fails falls back to the current PERFORM harmony, then to none — never a wrong chord, at worst a
  less specific one. A Song loop or end inside the 4 answer bars means those bars use the fallback.
- **Lookahead CPU.** 16 instead of 8 snapshot builds once per CALL (every 8 s at 120 BPM), main loop
  only, no file system (Graph state comes from the route cache). Not measured on hardware.
- **Wait time.** A 4-bar CALL is answered 4 bars + 1/16 after it starts (8 s at 120 BPM). A player who
  keeps playing cancels the answer as before (turn rule unchanged).
- **Free clock idle stop** is still 2 empty windows, i.e. 8 bars with BARS 4 (16 s at 120 BPM) before the
  free clock stops.
- **Log length.** A dense 4-bar `[CR-A]` line can reach ~500 chars; buffer is 576 B on the 32 KB loop
  stack.

### Stuck note / no sound

Not touched: no change to `UsbMidiOutput`, the answer lane, `ResponsePlayer` logic or the NoteOff path.
What changes for the responder is only its size: `slots_` holds 32 notes instead of 16, so a 4-bar answer
can keep **up to 4 bars of pending NoteOns/NoteOffs** in the queue (8 s at 120 BPM) instead of 2. Every
exit path is the same code as before (person NoteOn → cancel, BARS change / epoch / OFF → `stopClock` →
`cancel`, which sends the NoteOff of a sounding note at once; tested for 4 → 2, 4 → 1 and 2 → 4). If the
known symptom (no sound after a stuck note) is related to the answer staying active, a longer answer
makes that state last longer — it does not create it.

## What is checked

- `tests/test_cr_four_bars.cpp` (`cr_four_bars`, added to `tests/run_smart_play_tests.sh`):
  - capture: 64-step window, no close at step 32, notes in all 4 bars, a note at 31.8 stays inside;
    capacity 32 with 40 notes (8 dropped, `drop=8`), back to 16 for 2 and 1 bars; pickup on the 4-bar
    boundary (63.6 → step 63, 63.8 → next window step 0, held note clamped to 64);
  - 64-bit mask: bits 0/20/33/47/63, `[CR]` log prints 64 bits with 3 separators; a 2-bar phrase still
    prints the phase-0 32-bit format;
  - planner: 3000 random 4-bar CALLs × delay 0/1 × ECHO, harmonic echo (16 random beat chords, 16 genres
    and none) and LOOP/DUET with EVOLVE 0..3 — every note in the key, in 24..71, sorted, monophonic,
    `step + length ≤ 64 − delay`; a CALL ending in bars 3–4 is answered there; one note on step 63 →
    step 62; strong beats in bars 3 and 4 take the chords of beats 8 and 12;
  - setting: default 2, cycle 2 → 1 → 4 → 2 and back, 3 → 2; BARS 4 answer starts at step 65 and stays
    inside steps 65..128; one-note CALL on step 63 of a later window answered at window + 1 + 62;
    BARS change while a 4-bar answer sounds (→ 2, → 1) and 2 → 4 while a 2-bar answer sounds: one NoteOff
    at once, clock reset, NoteOn/NoteOff balanced afterwards; playing over a 4-bar answer cancels it;
  - lookahead: 3 rows ahead refused by default, allowed with 4 rows and equal to the snapshot built at that
    row; past the recipe, past `lastSequentialRow` and an edited row are refused; pattern offset 72
    refused by default, allowed with 80, 81 refused.
- **BARS 1/2 unchanged:** a golden harness (host only, not committed) ran 144 random sessions (BARS 1 and 2
  × ANSWER/DUET/LOOP × delay 0/1, incl. overflow windows) through listener + `planPerformanceReply` +
  `planEchoVariant` + the responder, printing `[CR]`, `[CR-A]` + harmony log and every MIDI event with its
  time: 11253 lines, **byte-identical** before and after the change.
- Full `bash tests/run_smart_play_tests.sh`: all targets pass (phases 0–3, PERFORM, `cr_four_bars`).
- `bash scripts/build_cardputer_dynbuffers.sh`: PASS, DRAM above.

## Not verified

- Not hardware-tested, not listened to: the musical value of trading 4-bar phrases, the 8 s wait, the
  lookahead across 3–4 Song rows on a real Song/Graph project, CPU cost of 16 snapshot builds.
- SDL: the BARS row renders `4 BARS` (string only), nothing else wired, as in phases 0–3.

## Code

- `src/input/call_response_phrase_capture.h` — `kMaxWindowSteps`, `kMaxCapacity`, `capacity()`,
  `setWindowBars(4)`, 12-byte event.
- `src/input/call_response_motif.h` — uint64 mask, `windowSteps`, 64-bit log.
- `src/input/call_response_planner.h` — plan capacity 32, uint64 `onsetMask()`, window ≤ 64.
- `src/input/call_response_development.h` — limits to the 4-bar maxima.
- `src/input/call_response_harmony.h` — 16 beats, `kDefaultBeats`, uint16 masks.
- `src/input/performance_harmony_lookahead.h` — `maxRowsAhead` / `maxOffsetSteps` (defaults = phase 2).
- `src/input/call_response_listener.h` — BARS 1/2/4, cycle order.
- `GroovePuter.ino` — `collectCallResponseHarmony(start, windowSteps, …)`, log buffers.
- `src/ui/pages/perform_page.cpp` — hint `</> CALL AND REPLY LENGTH: 1/2/4 BARS` (37 chars).
