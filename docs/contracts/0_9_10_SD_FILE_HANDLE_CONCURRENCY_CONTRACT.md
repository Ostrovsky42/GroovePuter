# SD file handle concurrency and failure atomicity contract (R1-FS0)

Follows the file-slot residency census in
[../audits/SD_RUNTIME_RESIDENCY_LOCALIZATION_2026-09-06.md](../audits/SD_RUNTIME_RESIDENCY_LOCALIZATION_2026-09-06.md),
which established that a FatFs file slot costs ~4096-4128 B and that
`max_files=5` therefore holds 16432 B of sector caches above the 13708 B
one-slot mount.

The question this contract answers is **not** how many features exist, but how
many files are open at the same instant in an admissible workflow — and what
must happen when a file cannot be opened.

## 1. Ownership

Established by reading the code, not by sampling a session.

| Subsystem | Handles | Lifetime |
|---|---|---|
| SMF playback (`cardputer_smf_player.h:108`) | 1 | whole playback |
| Audio recording (`cardputer_audio_recorder.h:26`) | 1 | whole recording |
| Voice playback (`voice_cache.h:252`) | 1 | whole voice playback |
| `copyFile()` (`pattern_paging.cpp:104`) | **2** | one copy transaction |
| Scene load / save | 1 | one operation, closed promptly |
| Sample load (`loadWavFile`) | 1 | load only |
| Directory traversal | 1 | `entry` only; `dir` uses opendir and takes no file slot |
| MIDI import | 1 | one operation |

Two corrections to earlier assumptions, both from source:

- `openMainOrBackup` (`scene_storage_cardputer.cpp:81`) is not a double open. A
  failed primary open yields an invalid `File` holding no slot, and only then is
  the backup opened. **One handle.**
- **Samples do not stream from the card.** `RamSampleStore` is RAM-resident and
  `loadWavFile` opens, reads and closes. No slot is held while a sample plays.

## 2. Concurrency classification

Handles are **not** summed mechanically. Only combinations the product actually
permits enter the worst case.

| Combination | Class | Basis |
|---|---|---|
| SMF + recording | MUST coexist | recording the output of MIDI playback is a real workflow |
| SMF + voice | MAY coexist | voice feedback fires from the engine at any time |
| recording + voice | MAY coexist | same |
| all three long-lived | permitted today | no gate found in code |
| scene save over the three | MUST NOT fail destructively | see §3 — it already satisfies this |
| sample / MIDI import over the three | MAY be deferred | bounded user action, deferral is explainable |
| **`copyFile(2)` over the three** | **candidate for deferral** | short control operation, not a realtime musical lifetime |

```
3 long-lived + any ordinary 1-file operation = 4
3 long-lived + copyFile(2)                   = 5
```

Only `copyFile` raises the ceiling from 4 to 5, and it is the one member of the
set that is a control action rather than a continuous musical lifetime.

**Current product semantics therefore require 5.** `max_files=5` matches the
concurrency the product actually permits; it is neither a legacy default nor
slack.

## 3. Failure atomicity

What must hold when a file cannot be opened, and what actually holds today.

| Operation | Fails before mutation | Old state safe | Partial output removed | Retry safe |
|---|---|---|---|---|
| Scene save | **yes** | **yes** | yes | yes |
| `copyFile()` | **no** | **NO** | yes | source-dependent |

Scene save is correct by construction: it writes to a temp path, verifies the
size, then `commitTempFile` (`scene_storage_cardputer.cpp:58`) renames
main → backup and temp → main, rolling the backup back if promotion fails. A
slot shortage returns false before anything is touched.

`copyFile` is not:

```cpp
if (!removeIfExists(targetPath)) { ... }   // destination deleted here
File target = SD.open(targetPath, FILE_WRITE);
if (!target) { source.close(); return false; }  // returns with destination gone
```

The destination is destroyed **before** the open that can fail. Slot exhaustion
is exactly such a failure. This is a pre-existing correctness defect, present at
`max_files=5` today and independent of the memory work: a page copy can destroy
the destination page and report only `false`.

The remedy already exists in this repository — the temp-then-rename sequence
scene storage uses. Recorded here as a required fix; it is a correctness change,
not a memory optimisation, and it does not change the handle count (source +
temp are still 2).

## 4. Decision gate

Agreed before the data was collected, restated here unchanged:

```
required handles = 5   -> max_files stays 5; the configuration lever is
                          exhausted, and the honest next question becomes
                          R1-FS1: the product genuinely needs 5 concurrent FIL
                          objects, but need not pay 5 x 4 KiB sector caches.

required handles <= 3  -> max_files=3 candidate (+8224 B, +2560 B largest),
                          with hardware acceptance under worst-case concurrency.

required handles = 4   -> keep 5 regardless: the census showed 5 -> 4 returns
                          ~4.1 KB of total free and moves largest by ZERO, so it
                          does not address the observed contiguous-allocation
                          failure at all.
```

**Result of this contract: required handles = 5.** No product restriction is
introduced, because introducing one to reach `max_files=4` would change musical
concurrency for a change the census proves cannot help the failure class.

## 5. What this hands to R1-FS1

The census separated two quantities that were previously conflated:

```
5 logical file handles   -- required by product semantics
5 physical 4 KiB caches  -- an implementation choice of the FatFs configuration
```

Whether these must be coupled at all is the question for shared-cache /
`FF_FS_TINY` characterization. That is now a well-posed investigation with a
justified premise, rather than "the default looks too big".

## 6. FS0C — deferred

```
Observed:
copyFile() is not transactional: it deletes the destination before opening
the file whose failure it must survive.

Current call-site semantics:
- copyProjectPages() intentionally clears the whole destination project first,
  and clears it again on failure; replacing the destination is the requested
  operation.
- legacy migration writes into a freshly created destination.

Therefore:
no current user-visible durability violation is proven. An earlier claim in
this investigation that a page copy "can destroy the destination today" was
made by reading copyFile in isolation and is retracted.

Future invariant:
a caller requiring preserve-on-failure semantics must not use the current
destructive copy primitive without transactional wrapping. The sequence to
use is the one scene storage already implements: write temp, verify,
main -> backup, temp -> main, roll back on promotion failure.
```

Deferred deliberately: making this change now would put a production edit
inside a memory investigation and blur the evidence for FS1.

## 7. FS1 entry condition — and its blocker

The premise is well-posed:

```
PRODUCT REQUIREMENT      5 simultaneous logical File handles
CURRENT IMPLEMENTATION   5 logical handles -> 5 private ~4 KiB sector caches
                         ~16.4 KiB above the one-slot mount
QUESTION                 must one logical file session own one 4 KiB cache?
```

But the lever is **not reachable by configuration in this toolchain**, established
before spending effort on it:

```
libfatfs.a   present in esp32-arduino-libs
ff.c         absent from the core entirely
```

FatFs ships precompiled. `FF_FS_TINY` is baked into `libfatfs.a`, and the slot
allocation happens inside `esp_vfs_fat_register`, i.e. inside that binary. Editing
`ffconf.h` in the core tree cannot change the library; it can only desynchronise
this project's view of `FIL` from the library's, which is worse than ineffective.

Reaching this lever therefore requires rebuilding `libfatfs.a` against ESP-IDF
`v5.4-858a988d` with `CONFIG_FATFS_PER_FILE_CACHE=n` and substituting it into the
core — a maintained build line, not a build flag, that must be re-done on every
core update.

That cost is now a decision to take deliberately, with the ~16.4 KiB prize and
the 7-point acceptance matrix (firewall, mount residency, five-handle
concurrency, filesystem correctness, I/O behaviour, realtime impact, runtime
memory) both known in advance.

## 8. R1-FS1A — ABI / rebuild radius census

Read-only against the exact source the core was built from, ESP-IDF `858a988d`,
inspected without checking it out. No byte of production changed.

### Radius

| Component / archive | Uses FIL/FATFS | By value | sizeof-dependent | Must rebuild |
|---|---|---|---|---|
| `components/fatfs` -> `libfatfs.a` | yes (28 files) | yes | yes, `vfs/vfs_fat.c` | **yes** |
| `components/vfs` | 3 hits | no | no | no |
| Arduino `SD` / `FS` (compiled per sketch) | yes | **no — pointers only** | no | no |
| all other prebuilt archives | none | — | — | no |
| `wear_levelling` | not linked at all | — | — | no |

Evidence:

- Every `sizeof(FIL)` / `sizeof(FATFS)` in the tree is in
  `components/fatfs/vfs/vfs_fat.c`, the same component as `src/ff.c`. The context
  allocator is `vfs_fat.c:198`,
  `sizeof(vfs_fat_ctx_t) + max_files * sizeof(FIL)`.
- The three `components/vfs` hits are in `test_apps/` and are only the string
  "FATFS" inside test-case names. They compile into nothing shipped.
- The Arduino SD library touches these types by pointer only:
  `FATFS *fsinfo` (`SD.cpp:95,111`), `FATFS *fs` (`sd_diskio.cpp:748`). No
  by-value use, no `sizeof`. A layout change cannot reach it.
- Of every prebuilt archive in the core's `lib/`, only `libfatfs.a` has
  undefined references to `f_open` / `f_mount` / `esp_vfs_fat_register`.

### Production image has no wear-levelling user

Stronger than "not used in our code" — checked in the linked product ELF:

```
wl_mount / wl_unmount / wl_read / wl_write        0 symbols
esp_vfs_fat_spiflash_mount / _rw_wl               0 symbols
esp_vfs_fat_register / f_mount / f_open           present
```

The 4096-byte logical sector is imposed by a subsystem that is not linked into
the firmware at all. `FF_SS_SDCARD` is 512; the SD card's physical sector is 512.
Espressif documents per-file cache combined with `CONFIG_WL_SECTOR_SIZE_512` as a
supported way to reduce RAM, so this is a provided configuration, not a hack.

### Conclusion

```
REBUILD RADIUS = libfatfs.a only

Change:      CONFIG_WL_SECTOR_SIZE 4096 -> 512
             CONFIG_FATFS_PER_FILE_CACHE stays y
Effect:      FF_MAX_SS 4096 -> 512
             each FIL  4136 -> ~552      (x5)
             FATFS     4152 -> ~568
Expected:    ~21.5 KiB internal RAM recovered
Preserved:   5 logical handles, 5 private caches, no shared-cache contention,
             no realtime cache-ownership change, musical semantics untouched
```

This is preferable to `FF_FS_TINY`: it recovers more (~21.5 vs ~16.4 KiB) and,
unlike a shared cache, introduces no new contention between concurrent file
sessions and therefore no new realtime risk.

### Residual risk, stated rather than hidden

The archive scan checked **undefined references**. If some other prebuilt library
had included `ff.h` and inlined a sizeof-dependent constant, that would not appear
as an undefined symbol. Given that no archive other than `libfatfs.a` references
FatFs symbols at all, this is unlikely, but it is the one hole in this census and
should be closed by comparing symbol sets after the rebuild.

The local ESP-IDF working tree is v5.5.2; commit `858a988d` is present as an
object, so a build requires a worktree at that commit plus the core's own
toolchain (`esp-x32/2411`), both available.

## 9. FS1A measurement — the saving is confirmed, not estimated

Compiled with the core's own toolchain (`esp-x32/2411`) against the core's own
headers and include flags, differing only in `sdkconfig.h`.

| | WL=4096 (as shipped) | WL=512 | saving |
|---|---|---|---|
| `sizeof(FIL)` | **4136** | **552** | 3584 x 5 slots = **17920** |
| `sizeof(FATFS)` | **4152** | **564** | **3588** |
| `FF_MAX_SS` | 4096 | 512 | |
| | | | **21508 B** |

The baseline column reproduces the earlier on-target census byte for byte
(4136 / 4152 / 4096), which confirms the probe compiles in the same
configuration the shipped library was built with and is therefore measuring the
real layout rather than a hypothetical one.

Projected effect on the failing state:

```
now:       free 1824 B    largest 1012 B    11/11 boots panic
projected: free ~23.3 KB
```

Preserved: five logical handles, five private caches, no shared-cache
contention, unchanged realtime cache ownership, sector size brought to what the
SD card physically uses.

Remaining to do, in order: rebuild `libfatfs.a` at `858a988d` with
`CONFIG_WL_SECTOR_SIZE=512`, compare its exported symbol set against the shipped
archive (this also closes the inlined-constant hole recorded in §8), substitute
it, then run the 7-point acceptance on hardware. No production byte has been
changed to reach this point.

## 10. FS1-B hardware result — characterization STRONG PASS, acceptance OPEN

The candidate is not the sector-size change of §9. Reading the exact IDF showed a
better mechanism already present at `858a988d` and disabled by default:
`CONFIG_FATFS_USE_DYN_BUFFERS`. It makes both buffers pointer-based and allocates
them at the *actual* volume sector size, with the file buffer living only while
the file is open:

```c
ff.c:3445   fs->win = ff_memalloc(SS(fs));     // at mount, 512 for SD
ff.c:3885   fp->buf = ff_memalloc(SS(fs));     // at f_open, freed at f_close
```

### Identity

```
ESP-IDF source     858a988d (read-only archive export, no checkout)
sdkconfig delta    + CONFIG_FATFS_USE_DYN_BUFFERS 1     (nothing else)
max_files          5, unchanged
toolchain          esp-x32/2411, the core's own
stock  libfatfs.a  sha256 68b0c90d961705651ea723a24ee3afd5a8d219ec8a405e668ccfa8c8d269ce57
candidate          sha256 9ea6cc771c33464f90df500727a4760254b5faad5f7f96a9ea76d739f228d1c5
recipe             scripts/build_fatfs_dynbuffers_candidate.sh
link proof         GroovePuter.ino.map references /tmp/fatfs-build/libfatfs.a only
```

The shared Arduino install was never modified. The candidate is placed ahead of
the stock archive by search path for this build alone, because a dozen worktrees
of this project share that install. Injecting via `build.extra_libs` was
deliberately avoided: inside `-Wl,--start-group` some objects could resolve from
one archive and some from the other, producing a program whose translation units
disagree about `sizeof(FIL)` — worse than changing nothing.

### ABI gate

```
archive member set     identical (10 of 10)
defined global symbols identical
undefined symbols      no difference
ff_memalloc in ff.c.obj   stock 0  ->  candidate 1
```

Symbol-set equality also closes the residual hole recorded in §8: an inlined
sizeof-dependent constant in another archive would have shown as a difference.

### Hardware, one boot, SD inserted

```
boots 1    panics 0    uptime 105 s    Reset Reason: 1
```

| | before (A) | candidate (B) | change |
|---|---|---|---|
| SD mount, free | 44732 -> 14592 (**−30140**) | 44788 -> 38676 (**−6112**) | **+24028 B** |
| SD mount, largest | 31732 -> 7668 (−24064) | 31732 -> 26612 (**−5120**) | **+18944 B** |
| after SMF init | free 5284 / largest 2292 | free **29624** / largest **20468** | |
| at UI page creation | DRAM 1932 | DRAM **26272** | |
| after setup | free 1824 / largest 1012 | ~23800 | |
| outcome | **11/11 boots panic** | **0 panics** | |

The contiguous recovery matters more than the total. `largest` was the proximate
cause of the cliff — a 1281 B request failing against a 1012 B block — and the
mount now costs 5120 B of it instead of 24064 B.

Static DRAM is essentially unchanged (190784 vs 190776, +8 B), exactly as
expected: the `FIL` array lives in the heap-allocated `vfs_fat_ctx_t`, not in
`.bss`, so the whole saving is runtime and could only be proven on hardware.

### Status, deliberately not stronger

```
FS1-B CHARACTERIZATION   STRONG PASS
PRODUCTION ACCEPTANCE    OPEN
```

One boot and 105 s. Of the A-G acceptance only A, part of B and part of D are
exercised. Untested: 10+ min soak, SMF playback, scene save/load, sample load,
MIDI import, pattern copy, and the mandatory item G.

Item G is the one that could still invalidate this. Buffers now allocate at
`f_open`, so a fifth handle could fail under heap pressure at an unlucky moment.
The invariant to hold:

```
Dynamic residency may follow actual file lifetime,
but logical file concurrency must not decrease.
```

Recovering 24 KB while silently losing the fifth handle would be trading memory
for a hidden capability regression — the opposite of the contract this workstream
spent the day establishing.

## 11. Item G — five simultaneous handles, PASS

Diagnostic `scripts/build_cardputer_sd_handle_census.sh`, run on the candidate
image with the card inserted. Files are created, held open together, written to,
then closed and removed.

```
[SDHANDLE] baseline   free=38456 largest=26612
  open#1  ok=1  free -5524   largest -4096
  open#2  ok=1  free -5664   largest -5120
  open#3  ok=1  free -5200   largest -3584
  open#4  ok=1  free -5200   largest -5120
  open#5  ok=1  free -5200   largest -1024
[SDHANDLE] simultaneous=5/5   free=11668 largest=7668
[SDHANDLE] writable=5/5
[SDHANDLE] after-close free=37680 largest=26612
```

The invariant holds: dynamic residency followed file lifetime, and logical file
concurrency did not decrease. `writable=5/5` matters as much as `simultaneous`
— a handle that opens but cannot write would satisfy the count and fail the
contract.

Correction to an expectation stated earlier in this document: a handle costs
about **5200 B**, not 512. The FatFs sector buffer is the small part; the rest is
the Arduino `File`/VFS wrapper, which existed before this change too. Candidate B
did not make opens expensive — opens were always expensive; B removed the
permanent reservation at mount.

This implies something worth checking separately: the previous build had only
14592 B free after mount, while five simultaneous opens need roughly 26 KB. The
old firmware may not have been able to honour the five-handle contract at all.
Not asserted — the old build's per-open cost was never measured.

Memory returned on close: 37680 vs 38456 baseline, 776 B difference, consistent
with directory metadata for created-and-removed files rather than a leak.

## 12. Soak, 205 s under real use

Product image with the candidate library, card inserted, 59 key presses, page
navigation, SMF playback exercised by the operator.

```
panics 0    underruns 0 (the only value seen in the whole capture)
free    23664 -> 21980     minimum over the run 21656
largest 14836 -> 13300     minimum 13300
audio peak 76.8% with no underrun
```

No downward trend. The per-cycle leak risk flagged when the allocation model
changed did not materialise over 205 s of open/close activity.

For contrast, the same product code before this change: `free 1824 / largest
1012`, 11 panics in 11 boots.

## 13. Evidence files — correction

Two earlier commit messages in this workstream claimed raw captures were
committed alongside the audits. **They were not.** `*.log` is excluded by
`.gitignore:11`, so `git add docs/audits/evidence/` silently added nothing. The
documents landed; the logs never did. The captures exist only in the working
tree under `build/memory-matrix/` and `docs/audits/evidence/`, and will be lost
on a clean checkout unless deliberately preserved.
