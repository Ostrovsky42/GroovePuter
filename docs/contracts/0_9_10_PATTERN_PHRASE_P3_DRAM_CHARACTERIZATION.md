# 0.9.10 PATTERN/PHRASE P3 — Cardputer ADV DRAM characterization

Measurement record. Neither the DRAM ceiling nor phrase capacity is changed
here, and no reserve policy is proposed: this document only fixes what was
measured, so the ceiling decision can be made from numbers instead of estimates.

## Identity

| | |
|---|---|
| Product source | `48762854954813604508b639798cd8bd472d0aaf` |
| Diagnostic source | `020582e19933e0c83bc62ad7a05bced2ceab2b23` |
| Pre-P3 reference | `4d51a12d` |
| Toolchain | arduino-cli 1.4.0, m5stack:esp32 3.2.2, esp32:esp32 3.3.5 |
| Device | Cardputer ADV, PSRAM disabled, huge_app |

## Static DRAM

| image | FQBN CDCOnBoot | data | bss | fixed | vs 191488 |
|---|---|---|---|---|---|
| product normal | `cdc` | 35304 | 157600 | **192904** | **−1416** |
| product midi-only | `default` | 35304 | 157544 | **192848** | **−1360** |
| pre-P3 normal (`4d51a12d`) | `cdc` | 35304 | 155040 | 190344 | +1144 |

ELF SHA-256:

```
product normal      0346bd89ea1b7fcbb87d95ee9126037a31fbe5ce5ea78252814a0340f37c2354
product midi-only   46a204c523c3309ccf8c7a299c66280cd71e3448be9e73cb2bd249af5da81416
diagnostic normal   3709dd732bee298b272f6f0fad00a40112aabba4aed0cc8c442f7d8dfa56edb5
```

`scripts/check_cardputer_dram_budget.sh` exits 1 for both product images.

### Attribution

Comparing `.dram0` symbols between `4d51a12d` and the product image, exactly one
symbol changed size and no symbol appeared or disappeared:

```
g_miniAcidInstance   14320 -> 16888   (+2568)
```

`.data` is byte-identical at 35304 in both. The whole delta is zero-initialised
storage inside the engine instance: two per-voice `RuntimeSynthEventBuffer`
objects at 1284 bytes each.

So the ceiling was not inherited broken. Before P3 the budget held with 1144
bytes to spare — 0.6% of the ceiling — and P3 consumed rather more than twice
that in one feature.

## Why hardware measurement needed an injection

Nothing in production calls `setSequencedSource()`, `setPhraseLength()` or
`currentPhraseBuffer()`; only host tests do. In the shipped firmware
`sequencedSource_[]` therefore stays `Pattern` and `currentPhrase_[]` stays
empty. Measuring the product image would have shown that the two buffers are
resident, never that phrase-relative playback executes.

`src/diag/p3_dram_characterization.h` drives that path for the diagnostic image
only. Its body sits behind `P3_DRAM_CHARACTERIZATION`, which only the
runtime-instrumented source copy defines. Firewall verified two ways: the
product image measures 192904 bytes fixed both before and after the header
existed, and the product ELF contains none of the scenario's symbols or phase
strings while the diagnostic ELF contains all of them.

Scenario: Synth A, 8 bars, buffer filled to `kMaxSynthEvents` — worst case for
the linear phrase scan. A note at tick 360 running 96 ticks crosses the bar
boundary at 384 and releases at 456. Onsets at 1200 and 1208 preempt each other
in the monophonic playback owner. Every event carries probability 100 and no
ghost flag, keeping `triggerSynthStep_` on its zero-RNG-draw path.

## Runtime, three runs of the same ELF

Bytes are `MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT`.

| phase | run 2 | run 3 | largest block |
|---|---|---|---|
| pre-P3 periodic | 27292 | 27292 | 16372 |
| p3-playback-start | 27292 | 27292 | 16372 |
| p3-cross-bar | 26144 | 26144 | 16372 |
| p3-stop-1 | 26144 | 26144 | 16372 |
| p3-stop-2 | 26144 | 26144 | 16372 |
| p3-stop-3 | 26008 | 26144 | 16372 |

`integrity` reported 1 at every sample. `largestInternal8` was 16372 at all 31
samples across the three runs and its watermark never moved.

Three costs separate cleanly:

- **first-playback warm-up, 1148 bytes.** Between `playback-start` and
  `cross-bar`, once, not returned on stop. Consistent with lazy initialisation
  warmed by the first transport start rather than a per-cycle cost.
- **playback-active, ~456 bytes.** The dip between a stop and the following
  peak. Peak samples vary (25688, 25920, 25952, 26008, 26144) because the sample
  lands at different points of the audio/UI cycle; the stop value is the stable
  one.
- **cleanup, full.** Eight of nine stop samples across the three runs read
  exactly 26144.

The ninth read 26008, 136 bytes lower, in run 2's `stop-3`. It is not
positionally reproducible — run 1 and run 3 both read 26144 at that same stop —
and a per-cycle leak would have produced a monotonic 26144 / 26008 / 25872
instead of two identical stops followed by one outlier. Read as a transient
allocation caught by the sample, not accumulation.

## Verdict

```
fragmentation            NO      largest block unmoved, 31/31
heap integrity           PASS    31/31
per-cycle accumulation   NOT OBSERVED
playback-active cost     ~456 B
first-playback warm-up   1148 B, one-off
static ceiling           FAIL    -1416 / -1360
```

P3 phrase runtime is not a source of memory instability. The static excess and
the runtime behaviour are separate problems, and only the first is open.

## Not established here

- **Reserve policy: MISSING.** How many bytes must remain below the ceiling is
  undecided, so no new ceiling is derivable yet. `191488` is a provisional
  policy exception by the checker's own comment, not a proven hardware maximum.
- **MIDI-only runtime telemetry:** not attempted. That profile builds with
  `CDCOnBoot=default`, so a serial console may not enumerate. Its static figure
  above stands regardless.
- **Ten-minute mixed soak:** not run. These captures are ~180 s each and cover
  three play/stop cycles.
- **`p3-phrase-loaded`:** never captured; the monitor attaches just after it.
  Phases from `playback-start` onward are direct observations.
