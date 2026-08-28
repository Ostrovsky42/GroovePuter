# 0.9.9-PHRASE-I1 — END-TO-END SYNTH PHRASE INTEGRATION

## Purpose

Connect frozen P1R semantic phrase execution to the already accepted D2 Phrase -> Song publication/transport path, then project the currently sounding Song pattern into the Synth NOTES editor.

Exact frozen base:

`a413561136b274a1b16b079f95f8d3ce3353fac5`

Branch:

`agent/20260828-05-0.9.9-phrase-i1-end-to-end-synth-phrase`

## Ownership

```text
Phrase page G
  -> P1R preparePhraseExecution(...)
  -> P1R materializePreparedPhraseBar(...)
  -> existing GeneratedPhraseSong transaction
  -> existing D2 pending/BAR_START activation
  -> existing Song transport
  -> existing Song physical pattern selection
  -> Synth NOTES display projection
```

No second Song transport, pending queue, phrase cache or Undo owner is introduced.

For a P1R-capable strong-rhythm route, a typed P1R reject/failure is fail-closed. It does not silently fall back to the old D2 musical preparer. A true `StrongRhythmRoute::Legacy` route retains the old D2 preparer.

Musical phrase identity is allocated through the existing session generation-attempt owner on reserved logical channel `0xFFFF`. `pageIndex`, Song row and physical pattern address are not identity inputs. Physical pattern address first appears at final per-bar materialization.

D2 continues to set `scene.feel.patternBars = 1` only as Song-row transport cadence. Semantic phrase length remains exact `1/2/4/8`.

C2/R1 cross-bar lifetime production is not imported; P1R lifetime remains inert/all-false.

## Hardware list

- M5Stack Cardputer ADV
- USB-C cable for flash/Serial
- built-in speaker, or existing audio output
- optional SEQTRAK for the existing MIDI-only regression path

## Wiring

No new wiring. Use the normal Cardputer ADV hardware configuration. Internal audio remains the existing ES8311/I2S path.

## Build / flash

```bash
git checkout agent/20260828-05-0.9.9-phrase-i1-end-to-end-synth-phrase
bash tests/run_0_9_9_phrase_i1_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/upload.sh /dev/ttyACM0
```

Serial: `115200`.

## Hardware acceptance

1. Open `PHRASE`.
2. Use Up/Down to select `1B`, `2B`, `4B`, or `8B`.
3. Use `Ctrl+Left/Right` or `Ctrl+Up/Down` if the Song destination row must be moved to empty rows.
4. Press plain `G`.
5. If stopped, the generated arrangement enters Song Mode immediately. If already playing, the existing D2 owner activates it only at the next BAR_START.
6. Play the Song and open Synth A or Synth B `NOTES`.
7. At every Song row transition, the displayed bank/pattern and note grid must follow the actually sounding physical pattern.
8. Press STOP during a later phrase bar. The NOTES page must remain on that last played bank/pattern; editing a step must edit that exact stopped pattern.

Repeat for accepted phrase lengths and at least one P1R-capable genre with moving harmony. An inadmissible exact length must show a rejection/failure and must not publish a legacy substitute.

## Expected behavior

- one P1R phrase selection per logical phrase;
- exact semantic length 1/2/4/8 when admitted;
- bar-local HarmonicRhythm WHEN with phrase-global harmonic ordinals;
- one ChordProgressionSource WHAT across all bars;
- one physical Song row per semantic phrase bar;
- existing D2 quantized activation and Undo semantics unchanged;
- Synth NOTES follows Song playback selection;
- STOP preserves the last displayed/selected physical pattern for editing;
- relocating the same prepared semantic bar to another physical slot does not change its musical material.

## Troubleshooting

- `PHRASE LENGTH REJECTED`: the selected exact length is not admitted by the frozen phrase law for that route. This is expected fail-closed behavior.
- `PHRASE EXEC FAILED`: P1R preparation/materialization failed; do not treat the old D2 output as a substitute.
- `NO CONTIGUOUS PATTERNS` / occupied Song rows: move `TO` to free Song rows or free pattern storage.
- UI does not follow playback: confirm Song Mode is active and the visible tab is Synth `NOTES`; record the current Song row and displayed pattern address.
- Audio works but a phrase does not activate while PLAY is running: verify the next BAR_START occurs and no other quantized generation is pending.

## Acceptance checklist

- [ ] focused I1 source/GCC/repeat/Clang/ASan/UBSan gate GREEN
- [ ] frozen P1R focused suite GREEN
- [ ] frozen D2 focused suite GREEN
- [ ] Core host / SDL / Cardputer ADV / fixed DRAM / SEQTRAK MIDI-only GREEN
- [ ] Stage15 / tonal inherited matrix GREEN
- [ ] 1/2/4/8 exact-length behavior checked for admitted cases
- [ ] typed inadmissible length fails closed without legacy fallback
- [ ] Song row cadence is one row per semantic bar
- [ ] Synth NOTES follows the sounding Song pattern
- [ ] STOP leaves the last played pattern visible and immediately editable
- [ ] no C2/R1 lifetime behavior claimed
