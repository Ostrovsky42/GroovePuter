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
