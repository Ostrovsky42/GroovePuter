# SD runtime residency — localization (2026-09-06)

Workstream `0.9.10 — SD RUNTIME RESIDENCY / PANIC LOCALIZATION AND FIX`,
characterization checkpoint (§4). **No fix is applied and none is proposed as
final in this document.**

## Identity

```
BRANCH            feature/20260906-05-0.9.10-sd-runtime-residency
                  (based on research/20260906-04-...-memory-r0-d-sd-panic-root-cause,
                   not on dev_0.9.10, to avoid a duplicate parallel line)
SOURCE SHA        95f5236a9af12da2d93e863bdd2efbe66f76517d
DIAGNOSTIC ELF    77d738060239ee517340fd4aabea874984d61afff164d804f88fe884b43502f1
PRODUCT ELF       893fb3806fb77f858b5b3338fbb726e593bdd8dd3374f252dff67de5ef6456c5
BUILD             scripts/build_cardputer_memory_phase_trace.sh
                  (adds only -DGROOVEPUTER_MEMORY_PHASE_TRACE)
TOOLCHAIN         xtensa-esp-elf-gcc (crosstool-NG esp-14.2.0_20241119) 14.2.0
FQBN              m5stack:esp32:m5stack_cardputer:PSRAM=disabled,
                  PartitionScheme=huge_app,USBMode=default,CDCOnBoot=cdc,UploadMode=cdc
HARDWARE          Cardputer ADV, PSRAM disabled, DRAM-only
SD                same physical card throughout; scenes/ emptied by the operator
LOGS              build/memory-matrix/caseA-sd-absent.log
                  build/memory-matrix/caseC-sd-present.log
```

### Product firewall (§13) — measured, not asserted

```
product      190776 B fixed DRAM   0 memPhase symbols   0 diagnostic strings
diagnostic   190840 B fixed DRAM   4 symbols            5 strings
```

190776 B is byte-identical to the recorded product baseline, so the
instrumentation costs the product image nothing. The 64 B diagnostic delta is
exactly `g_memPhaseStack` (8 frames x 8 B) — accounted for, not approximated.
`run_host_tests.sh` exits 0.

## Baseline matrix

| Case | SD | Result | Free after setup | Largest after setup |
|---|---|---|---|---|
| A | absent | stable, 305 s idle, no panic | 32080 B | 23540 B |
| C | present, scenes empty | **11 boots / 11 panics in 90 s** | **1824 B** | **1012 B** |

CASE B (SD present, empty filesystem) and D–G (subsystem-gated variants) were
not needed: A vs C already isolates a single discriminating owner, so building
diagnostic gates to subtract subsystems would add risk without adding
information.

## Phase deltas — the discriminating table

Both columns from the same ELF, same hardware, same card. Only card presence
differs.

| Owner | A: SD absent | C: SD present | difference |
|---|---|---|---|
| `SD_INIT` | −140 (largest 0) | **−30356 (largest −24064)** | **−30216** |
| `SMF_RUNTIME` | −8836 (largest −6144) | −9092 (largest −5376) | −256 |
| `USB_MIDI` | −216 | −84 | +132 |
| `ENCODER8` | −224 (largest 0) | −224 (largest −128) | 0 |
| `MINIACID_INIT` | 0 | 0 | 0 |
| `SAMPLE_SCAN` | −216 | 0 | +216 |
| `DISPLAY_ALLOC` | −588 (largest −1024) | −588 (largest −192) | 0 |
| `FIRST_DRAW` | 0 | 0 | 0 |

Every owner except `SD_INIT` costs the same in both branches within noise.

## Largest-block behaviour

`largest` matters more than `free`, and its collapse is sequential:

```
after M5 begin        73716
after display         31732     display: −65540 B free, −41984 largest
after SD_INIT (C)      7668     SD:      −30356 B free, −24064 largest
after SMF_RUNTIME      2292     SMF:      −9092 B free,  −5376 largest
after DISPLAY_ALLOC    1012
```

The display is the largest single consumer in absolute terms (65540 B) but it is
**identical in both branches**, so it does not discriminate a working device from
a failing one. It sets the ceiling; SD decides whether the system survives under
that ceiling.

## Retained owner

Inside `SD_INIT` the cost is attributed to a bracketed call, not to a size
coincidence:

```
[SD] mount begin   free=44732 largest=31732
[SD] mount result=1 type=2  free=14592 largest=7668
```

`SD.begin()` itself consumes ~30140 B and does not return it. This is the
Arduino/ESP-IDF FATFS/VFS layer. Attribution is at call-site granularity
(`ensureCardputerSdMounted()` → `SD.begin()`), which satisfies §8's requirement
that a size match alone is not evidence. It does **not** yet identify which
internal allocation inside that layer holds the memory.

## Panic attribution

Post-`setup() complete` the device survives ~17–23 s, then panics with
`Reset Reason: 4` and `Previous stage retained: 100`; 11 of 11 boots did this.
No `Guru Meditation` or backtrace is emitted — the USB-Serial/JTAG endpoint
disappears on reset, so this transport has never delivered one across ~30 panics
in this workstream.

The mechanism was independently established on a different build with the
retained-record instrument
([RUNTIME_PANIC_LOCALIZED_2026-09-06.md](RUNTIME_PANIC_LOCALIZED_2026-09-06.md)):
the `loop` task requests 1281 B in `Phase::Control` while the largest free block
is 1012 B, heap integrity OK, all stacks healthy. **This run reproduces the same
1012 B largest-block value from a different ELF and a different instrument**,
which is why the two are treated as the same event.

## Evidence status

```
PROVEN      SD mount is the discriminating owner: −30356 B free, −24064 B largest,
            while every other owner is unchanged between A and C.
PROVEN      The cost is retained, not transient: it is still absent from the free
            pool at setup completion, 30 s later.
PROVEN      Scene loading costs nothing at boot: MINIACID_INIT delta = 0 in BOTH
            branches. The autosave-OOM hypothesis is dead.
PROVEN      Sample indexing is not involved: SAMPLE_SCAN delta = 0 with SD present.
PROVEN      The display is the largest absolute consumer but does not discriminate.
PROVEN      Product firewall: instrumentation costs the product image 0 B.
OBSERVED    CASE A is stable for 305 s idle; CASE C panics 11/11 within ~17-23 s.
OBSERVED    Post-setup largest free block is 1012 B in the failing branch.
INFERRED    The 1281 B allocation that fails is the proximate trigger; that number
            comes from the other instrument, not from this run.
NOT PROVEN  Which allocation inside SD.begin()/FATFS holds the ~30 KB, and whether
            any of it is reclaimable without changing behaviour.
NOT PROVEN  That the panic is a null-dereference of the failed allocation. No
            backtrace has ever been captured on this transport.
NOT PROVEN  Any earlier claim that the mount needs one contiguous ~29.5 KB block.
            That model was already downgraded when a mount failed at largest=31732.
```

## Rejected hypotheses

- **Autosave / scene-load OOM** — `MINIACID_INIT` = 0 in both branches.
- **Sample index residency** — `SAMPLE_SCAN` = 0 with the card present.
- **P3 / Phrase memory** — untouched by this path; no owner shows it.
- **Repeated `SPIClass::begin()` clobbering pins** — disproven from source on
  2026-09-05, not revisited without new evidence.
- **Heap corruption / stack overflow** — integrity OK, all stacks healthy in the
  retained record.
- **The instrumentation itself** — a control run with the untouched product ELF
  behaved identically, exonerating the added brackets.

## Not yet decided: the fix

Per §10 the narrowest fix must follow the proven cause, and the proven cause is
*residency of ~30 KB claimed by `SD.begin()` on a device that has ~44 KB at that
point*. Before choosing between §10's classes A–E, one question must be answered
that this run does not answer: **what inside the FATFS/VFS layer holds that
memory, and is any of it configuration rather than necessity** (for example
mount-time cache or open-file table sizing).

If the answer turns out to require replacing the FATFS stack or introducing
mutually-exclusive resource modes, §16 applies: stop, and bring options rather
than an implementation.

## Remaining risks

- No backtrace has ever been captured; panic attribution rests on the retained
  record plus the reproduced 1012 B value, not on a decoded fault.
- The card used has empty `scenes/`. A card with real user content may add cost
  this matrix does not show.
- ~2404 B between `USB_MIDI end` and `ENCODER8 begin` is outside any bracket
  (only `screenLog` runs there). Small, but currently unattributed.
- `localMin` is not measured anywhere: these are before/after snapshots and
  cannot see a transient peak inside a phase.

---

# Addendum: the 0.9.9 comparison reframes the diagnosis

Question that prompted this: *why was there no such problem on 0.9.9?*

## SD is not the regression

Same physical card, same slot, both versions mount successfully:

```
v0.9.9    [SD] mount result=1 type=2   49108 -> 18968   (-30140, largest 31732 -> 7668)
0.9.10    [SD] mount result=1 type=2   44732 -> 14592   (-30140, largest 31732 -> 7668)
```

`SD.begin()` costs the same ~30 KB in both. It is a large permanent consumer, but
**its cost did not change between releases**. The earlier framing — "SD mount is
the owner" — is correct about who spends the memory and wrong about what changed.

## What changed is the baseline, by a constant offset

| Point | v0.9.9 | 0.9.10 | delta |
|---|---|---|---|
| static globals | 186976 | 190776 | −3800 |
| after m5 begin | 142432 | 138056 | −4376 |
| after display | 76892 | 72516 | −4376 |
| after DSP buffers | 49108 | 44732 | −4376 |
| after SD mount | 18968 | 14592 | −4376 |
| after SMF | 9660 / largest 3060 | 5284 / largest 2292 | −4376 |
| after setup | ~6116 / largest 2548 | 1824 / largest 1012 | −4292 |
| outcome | stable, 70 s+, 0 panics | 11 boots / 11 panics | |

An almost perfect constant displacement. Downstream subsystems are not more
expensive; they subtract from a smaller base.

## Careful wording about the 1281 B allocation

The failing request was observed on a 0.9.10 diagnostic ELF, not on v0.9.9. The
defensible statement is therefore:

```
v0.9.9  largest = 2548 B  is sufficient for a 1281 B contiguous allocation.
0.9.10  largest = 1012 B  is not.
An independently observed 0.9.10 control-path failure requested 1281 B.
```

This does **not** assert that v0.9.9 executed the same call site; that was never
instrumented on v0.9.9.

## Symbol-level attribution of the +3800 B — complete

Symbols restricted to `.dram0.data` + `.dram0.bss` by address range. The top 16
account for the entire delta (+3793 measured against the linker's +3800).

| delta | v0.9.9 | 0.9.10 | symbol |
|---|---|---|---|
| **+8040** | 8848 | **16888** | **`g_miniAcidInstance`** |
| −3580 | 4436 | 856 | `(anonymous namespace)::g_output` |
| −2128 | 2128 | 0 | `phraseEvolutionCatalog()::storage` |
| +656 | 2944 | 3600 | `QuantizedGenerationDetail::g_slots` |
| +452 | 0 | 452 | `cardputerUartMidiTransport()::transport` |
| +200 | 1984 | 2184 | `g_performanceKeyboard` |
| +76 | 0 | 76 | `(anonymous namespace)::uart()::port` |
| +36 | 0 | 36 | `(anonymous namespace)::g_wire` |
| +32 | 16 | 48 | `g_internalSynthOutput` |
| +17 | | | guard variables, `pat_flg` |
| | | | **= +3793** |

Readings:

- **`g_miniAcidInstance` is the sole driver at +8040 B.** Everything else nets to
  −4247 and partially offsets it.
- `−2128` is the compile-time phrase-catalog move already landed; without it the
  regression would be that much worse. It is a saving already spent.
- `+452 +76 +36 +8 = +572` is the new DIN/UART MIDI transport in its entirety — a
  legitimate new capability, cheaply priced.
- `−3580` on `g_output` is a real reduction on the USB MIDI side.

NOT PROVEN: what inside `g_miniAcidInstance` accounts for the +8040. Prior
measurements attribute roughly 2568 B to P3 phrase buffers, leaving ~5470 B
unattributed. That needs a member-level breakdown, not a symbol-level one.

## The remaining ~576 B, resolved

It is not heap and not runtime. From the section table:

```
.dram0.dummy    49664 -> 50176    +512
.dram0.data     34200 -> 35304   +1104
.dram0.bss     152776 -> 155472   +2696
                                 ------
sections total                   +4312     (observed heap displacement +4376)
symbols (data+bss)               +3793
```

`.dram0.dummy` is the placeholder reflecting IRAM occupancy overlapping DRAM; it
grew 512 B. That is the part invisible in the linker's "Global variables" line.
The residual ~64 B is alignment. So the boundary the displacement originates from
is **before `M5.begin()`** — it is link-time, not an initialization path
difference.

## What this says about the static ceiling

The 191488 B ceiling was not wrong as an answer. It correctly answered:

```
STATIC PRODUCT DRAM <= 191488 ?   ->  190776, PASS
```

The error was using that answer as a surrogate for a different question:

```
DOES THE PRODUCT HAVE ENOUGH RUNTIME MEMORY ?
   post-setup free   1824 B
   largest block     1012 B
   11 / 11 boots panic
   -> FAIL
```

Both statements are true simultaneously, which is precisely the failure of the
model. A release contract needs more than one dimension:

```
static DRAM  +  post-setup free internal  +  largest internal block
             +  critical-operation local minimum  +  task stack margin
```

The static ceiling stays as one guardrail, not as proof of product memory health.

## Why restoring 4376 B would not be a fix

v0.9.9 survives at `largest = 2548 B`. That is not an engineering margin; it is
the right side of a cliff. Returning the 4376 B would restore roughly

```
~6 KB free, ~2.5 KB largest
```

and the next sampler, long-note, UI or MIDI capability would cross the same
boundary again. 0.9.10 did not create an expensive SD path — it exhausted a
memory model that was already at its limit.

The goal is a measurable operational reserve, not the restoration of a previous
number.

## Evidence status for this addendum

```
PROVEN      SD mount costs ~30140 B in BOTH versions; its cost is not the regression.
PROVEN      v0.9.9 mounts the same card and runs 70 s+ with zero panics.
PROVEN      The difference is a constant ~4376 B baseline displacement.
PROVEN      +3793 B of it is attributed symbol by symbol, top 16 covering all of it.
PROVEN      g_miniAcidInstance (+8040 B) is the sole growth driver.
PROVEN      ~512 B of the residual is .dram0.dummy growth, i.e. link-time.
NOT PROVEN  The member-level composition of the +8040 B inside g_miniAcidInstance.
NOT PROVEN  That v0.9.9 executes the same 1281 B call site; it was not instrumented there.
```
