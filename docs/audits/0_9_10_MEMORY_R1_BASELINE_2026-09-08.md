# 0.9.10 MEMORY-R1 — authoritative baseline and evidence ledger

Date: 2026-09-08

This document freezes the live remote state inspected before MEMORY-R1 changes.
It is an evidence ledger, not a release approval.

## Authoritative start

Repository: `Ostrovsky42/GroovePuter`

Live refs at inspection time:

```text
dev_0.9.10
  af6d76a1c9dfca0ef721adb0d2eca6f11eee8198
  Merge PR #438 — P2 common runtime playback / single lifetime owner

feature/20260906-05-0.9.10-sd-runtime-residency
  c408376a82d9e2ca07004e987726cac59bf1bf4f
  latest: five-handle dynamic-buffer acceptance characterization

feature/20260907-midi-io-nanokey2
  02bfe444647fc6755e0f21f58626ee654c311d0f
  latest: MIDI memory report; parent 8c275100 contains lazy-SMF boot deferral
```

No pull request existed for either the SD-residency branch or the MIDI/nanoKEY2
branch at inspection time.

The two feature lines are siblings, not ancestors of one another. Their merge
base is:

```text
aded0e183a934f78623030226b67b5d0b598648b
```

Therefore neither feature head may be treated as already containing the other
line's accepted work.

## MEMORY-R1 branch

```text
feature/20260908-0.9.10-memory-r1-dram-recovery
base = 02bfe444647fc6755e0f21f58626ee654c311d0f
```

Reason for choosing the MIDI head as the R1 base:

- it is the newer product line;
- it contains the hardware-measured lazy-SMF recovery;
- it contains the current DIN/nanoKEY2 direction and diagnostics;
- MEMORY-R1 must preserve those semantics while integrating the proven SD
  residency work.

The SD-residency line is treated as a sibling evidence/source line to port from,
not as already merged provenance.

## Evidence accounting at R1 start

### BANKED BEFORE R1

Lazy SMF boot deferral, introduced by `8c2751004f79513e7d1c2732ef0d304b226219fa`:

```text
category: HARDWARE-MEASURED on MIDI line
reported recovery: approximately 7.0–7.4 KB
R1 status: inherited baseline; do not count as new R1 recovery
```

### SIBLING EVIDENCE — NOT YET R1 ACCEPTANCE

FS1-B dynamic FatFs buffers on the SD-residency line:

```text
source head: c408376a82d9e2ca07004e987726cac59bf1bf4f
candidate mechanism: CONFIG_FATFS_USE_DYN_BUFFERS=1
CONFIG_WL_SECTOR_SIZE: unchanged at 4096
CONFIG_FATFS_PER_FILE_CACHE: preserved
max_files: preserved at 5

hardware characterization on sibling line:
  SD mount free recovery:    +24028 B
  SD mount largest recovery: +18944 B
  five simultaneous handles: 5/5
  writable handles:          5/5
  205 s use/SMF soak:        0 panics, 0 underruns
```

This is valid prior evidence for the mechanism, but it is **not** an R1 hardware
result. After the candidate is integrated with the MIDI/lazy-SMF line, R1 must be
rebuilt and remeasured on Cardputer ADV before the +24028 B value can be booked
as new R1 recovery.

### STATIC DRAM

The sibling FS1-B report observed essentially no static DRAM saving:

```text
190776 B -> 190784 B (+8 B)
```

The expected FS1-B gain is runtime residency, not `.data/.bss`.

### WT1

Historical repo census suggested `Wavetable::lookupTriangle()` has no production
caller and the table is 1024 floats (~4096 B), but this is **NOT PROVEN on the R1
head yet**. A fresh caller census and a failing source/ELF contract test are
required before any production removal.

## Gates before recovery can be claimed

FS1-B must satisfy on the integrated R1 line:

1. exact framework/core provenance still matches the `858a988d` source family;
2. only `CONFIG_FATFS_USE_DYN_BUFFERS` changes in the candidate FatFs config;
3. archive member and symbol sets remain ABI-compatible;
4. no other precompiled archive embeds `FIL`/`FATFS` by value;
5. open/close ownership census proves no `f_open`/`f_close` allocation enters an
   AudioTask or ISR hard-realtime path;
6. Cardputer build links only the candidate FatFs archive, never a mixed stock /
   candidate archive set;
7. hardware acceptance is repeated on this integrated line.

Until those gates pass:

```text
NEW R1 recovery = 0 B accepted
```

## Test truthfulness

A GitHub Actions run on the MIDI head exists and is green, but it is the
`Undo Safe Editing 0.9.8 R7` workflow. It is not evidence that MEMORY-R1,
Cardputer ADV, FatFs, MIDI I/O, or the full host suite are green.

The SD-residency head had no branch Actions runs at inspection time.

All R1 test reporting must classify PASS / FAIL / BUILD / TIMEOUT / INFRA and
must not promote unrelated green workflows into R1 evidence.
