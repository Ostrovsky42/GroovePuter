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
