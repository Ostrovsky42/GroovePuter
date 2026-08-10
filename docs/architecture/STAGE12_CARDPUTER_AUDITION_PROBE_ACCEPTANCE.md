# Stage 12 — Cardputer Phrase Audition + Runtime Probe Acceptance

Status: `AUDITION-READY CANDIDATE / PHYSICAL VERDICT PENDING`

Branch: `agent/20260810-22-stage12-cardputer-audition-probe`

Base: `agent/20260810-21-stage12-shipped-phrase-evolution` @ `0b8caff7a7e71f599eeac2f7e473f9f41f45e482`

## Purpose

Make the complete current genre set listenable on Cardputer ADV while measuring
the physical ESP32-S3 cost that blocks normal production multi-bar wiring.

This is an explicit audition command. Normal DRUMS `G` remains the accepted
one-bar Stage 14 path. `Ctrl+Alt+G` creates a separate audition in current-page
Bank B and Song B, then uses the existing MiniAcid song transport to play it.
No second transport or background sequencer is introduced.

Audition behavior:

```text
selected archetype is Stage 12 phrase-enabled
  -> 2/4/8 bars use PhraseEvolution / BarEvolution
  -> toast reports EVOLVED

selected archetype is still one-bar-only
  -> requested 1/2/4/8 slots use deterministic strong variations
  -> same selected rhythm identity is locked across slots
  -> toast reports VARIATION
```

`VARIATION` is an intentional audition fallback, not a claim that the genre has
received structured multi-bar evolution.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3;
- USB-C data cable;
- development computer with the repository dependencies installed;
- headphones/speaker as normally used with GroovePuter.

No external I2C/SPI accessory is required.

## Wiring

No external wiring.

PORT.A is not used. Its existing I2C contract remains untouched:

```text
SDA GPIO2
SCL GPIO1
```

## Destructive audition reservation

`Ctrl+Alt+G` deliberately reserves:

```text
current pattern page
Bank B local patterns 1..8
Song B
```

The command may overwrite those Bank B patterns and Song B. Bank A and Song A
are not audition storage and must remain untouched.

For a clean hardware test, save the project first or use a disposable scene.

## Build / Flash steps

Checkout the audition branch:

```bash
git fetch origin
git checkout agent/20260810-22-stage12-cardputer-audition-probe
```

Run focused and full gates:

```bash
bash tests/run_phrase_stage12_tests.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh \
  build/cardputer-adv-current/GroovePuter.ino.elf
```

Flash the current build. Default port is `/dev/ttyACM0`; pass a different
`/dev/...` device when needed:

```bash
bash scripts/upload.sh /dev/ttyACM0
```

Capture Serial at 115200 baud:

```bash
python3 scripts/serial_monitor.py \
  --port /dev/ttyACM0 \
  --baud 115200 \
  --duration 0 \
  --log build/stage12-audition-probe.log
```

## Controls

Normal controls stay unchanged:

```text
G             normal one-bar Stage 14 DRUMS generation
Ctrl+G        randomize selected drum voice
Alt+G         drum chaos randomize
Ctrl+Alt+G    Stage 12 audition + physical runtime probe
```

Before `Ctrl+Alt+G`:

1. Select the genre on GENRE.
2. Select RHYTHM AUTO or MANUAL as desired.
3. On FEEL set REPEATS to `1`, `2`, `4`, or `8`.
4. Return to the main DRUMS grid.
5. Press `Ctrl+Alt+G` once.

The audition command switches to Song B, loops the generated rows, and leaves
normal transport controls responsible for playback.

## All-genre listening matrix

Run at least one audition for every visible genre:

```text
Acid
Minimal / Outrun
Techno / Darksynth
Electro
Rave
Reggae
TripHop
Broken
Chip
House
Techno
HipHop
FunkSoul
UK Garage
Drum & Bass
LoFi
```

Use the labels shown by the current GENRE page if a display label differs from
the internal enum name.

For multi-bar-focused listening, use FEEL REPEATS `4` first, then `8` on styles
where longer development matters most: LoFi, TripHop, HipHop, Broken,
UK Garage, Drum & Bass, and Electro.

## Expected behavior

### Screen

After `Ctrl+Alt+G`, expect one of:

```text
AUD 4B EVOLVED #<id>
AUD 4B VARIATION #<id>
```

The bar count follows FEEL REPEATS. `EVOLVED` means the selected archetype used
the Stage 12 phrase catalog. `VARIATION` means the selected identity remains
one-bar-only and the audition used deterministic per-bar strong variations.

Unexpected statuses:

```text
SELECT_FAIL
MATERIAL_FAIL
```

### Serial

Each audition prints one line beginning with:

```text
[PHRASE-PROBE]
```

The line includes:

```text
status
bars / profileBars
selected archetype
trajectory ids
whole audition command duration
worst measured 4-bar P2 Reduction duration + archetype id
worst measured 4-bar P3 Break duration + archetype id
stack high-water before/after
remaining stack bytes after probe
internal free heap before/after
largest internal heap block before/after
```

Keep the complete line from the first 4-bar run and the first 8-bar run.

### Musical behavior

- All supported genres must produce a listenable audition rather than silently
  falling back to an unrelated random drum pattern.
- Stage 12-enabled Broken / DnB / UKG / Electro identities should exhibit real
  inter-bar structure when selected.
- `416 halftime_switch` remains multi-bar but non-subtractive: its accepted P1
  identity is preserved instead of lowering structural minima to fake a break.
- For 8-bar evolved outputs, the second four-bar segment must not be a literal
  topology copy of the first.
- Plain `G` must still sound/behave like the accepted Stage 14 one-bar route.

## Physical gate decision

Do **not** enable normal production multi-bar `G` merely because the audition
works musically. Record the probe values first.

The physical gate must review together:

```text
minimum observed remaining stack after command
largest internal heap block after command
fixed-DRAM build result with candidate catalog linked
worst Reduction duration
worst Break duration
audio underrun count before/after repeated audition commands
30-minute playback soak
```

Any stack/heap instability, watchdog/reset, new audio underruns, or unacceptable
UI/audio stall keeps normal production wiring blocked.

## Troubleshooting

### `SELECT_FAIL`

The current GENRE/RHYTHM selection did not resolve through the Stage 14 strong
selection boundary. First retry with RHYTHM AUTO and morph disabled. Do not make
the audition bypass the production selector just to obtain sound.

### `MATERIAL_FAIL`

One of the reserved Bank B bars failed strong/evolved materialization. The
command restores the previous selection/transport state, but Bank B may already
contain partially written audition material. Do not treat the partial Bank B
contents as a valid result.

### `VARIATION` instead of `EVOLVED`

This is expected when the selected archetype is not admitted to the Stage 12
phrase catalog. It exists so every genre can be listened to before all genres
receive evidence-backed phrase trajectories.

### Large memory drop

Compare `[PHRASE-PROBE]` `free_internal` and `largest_internal` with the normal
`[PERF]` diagnostics. Do not raise the fixed DRAM budget to hide the candidate
catalog cost. If static/runtime cost is too high, reduce/isolate the audition
representation before production wiring.

### Controls do not fire

Use `Ctrl+Alt+G`, not Shift+G. The Cardputer ADV workflow does not rely on a
physical Shift key for this command. Plain `G`, `Ctrl+G`, and `Alt+G` have
separate existing meanings.

## Acceptance checklist

```text
[ ] Focused Stage 12 tests pass.
[ ] Full host suite exits 0.
[ ] SDL build passes.
[ ] Cardputer ADV normal compile passes.
[ ] Fixed DRAM gate passes with the candidate catalog now linked.
[ ] SEQTRAK MIDI-only compile passes.
[ ] Plain G remains one-bar Stage 14 and does not enter audition.
[ ] Ctrl+G remains selected-voice randomize.
[ ] Alt+G remains chaos randomize.
[ ] Ctrl+Alt+G is recognized on physical Cardputer ADV.
[ ] FEEL REPEATS 1 produces one audition row.
[ ] FEEL REPEATS 2 produces two audition rows.
[ ] FEEL REPEATS 4 produces four audition rows.
[ ] FEEL REPEATS 8 produces eight audition rows.
[ ] Bank A remains untouched by audition.
[ ] Song A remains untouched by audition.
[ ] Current-page Bank B is the only pattern-bank reservation.
[ ] Song B is the only song reservation.
[ ] Every visible genre produces EVOLVED or VARIATION, not SELECT_FAIL/MATERIAL_FAIL.
[ ] Stage 12-enabled selections audibly differ across bars.
[ ] 416 halftime_switch does not fake Reduction/Break.
[ ] A [PHRASE-PROBE] line is captured for a 4-bar audition.
[ ] A [PHRASE-PROBE] line is captured for an 8-bar audition.
[ ] Reduction/Break worst-case timings are recorded.
[ ] Remaining stack and largest internal heap block are recorded.
[ ] No watchdog/reset occurs during repeated audition generation.
[ ] No new audio underruns appear during the listening matrix.
[ ] 30-minute Song B playback soak completes without regression.
```
