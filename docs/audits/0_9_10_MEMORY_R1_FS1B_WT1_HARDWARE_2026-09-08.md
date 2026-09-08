# 0.9.10 MEMORY-R1 FS1B — WT1 hardware acceptance

Date: 2026-09-08
Branch: `feature/20260908-0.9.10-memory-r1-dram-recovery`
Commit: `36a67113059e72b907e7f08e79ec1f44382b6c5d`

Continuation of
[the FS1B filesystem ownership census](0_9_10_MEMORY_R1_FS1B_OWNERSHIP_CENSUS_2026-09-08.md),
which closed with: *"Until they are rerun on the integrated R1 image, FS1B
remains a production candidate, not an accepted R1 recovery."* This is that
rerun, on the actual Cardputer ADV that the census's own worktree had no
network/device access to. This is a Claude Code session, run in a different
environment from the one that produced the FS1B candidate; it does not
replace that session's own review of its source-ownership claims, only
supplies the hardware step it explicitly could not take.

## Identity

```
ELF SHA-256   ef96a269808d947f7673e28e00ed1ee56eb63524191ed29845fd96dac66ddc94
Build         scripts/build_cardputer_memory_r1_fs1b_runtime.sh (default args)
FQBN          m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,
              USBMode=default,CDCOnBoot=cdc,UploadMode=cdc
Serial port   /dev/ttyACM0
SD            present, mounted successfully
Capture       logs/serial-wt1-fs1b-hardware-2026-09-08.log (60 s window,
              scripts/serial_monitor.py, full raw transcript)
Reset Reason  4 (power/RTS reset before capture) — zero further resets
              anywhere in the 60 s window (`grep -c "Reset Reason"` = 0
              after the initial boot line)
```

## Result: SD's static/mount cost collapsed, and it held under real navigation

Boot-stage `INTERNAL|8BIT` free/largest, this run:

| Boundary | Free (B) | Cost |
|---|---:|---:|
| before SD mount | 44,288 | — |
| after SD mount (dynamic FatFs, FS1B) | 38,172 | **-6,116** |
| `minFree8Boot` (lowest point anywhere in setup) | **29,380** | |
| `setup()` complete | 33,604 free, largest 25,588 | |

Compare to every prior measurement this investigation has made on the
non-FS1B stock FatFs archive (this branch's own parent, and the sibling P3
branch independently): SD mount has consistently cost **~30,144 B**, and
`minFree8Boot` has ranged from **208 B to ~1,400 B** depending on which other
fixes were already in place. FS1B's dynamic buffers replace that ~30 KB
static/mount cost with roughly a fifth of it, and the boot floor moves from
"a few hundred bytes on a bad day" to **29,380 B** — over 20x higher than
the best floor measured on this device all session.

## Hardware exercise: every page, both mutation-guarded ones included

60 s of live navigation, all through the actual keyboard (not scripted):
pages 1, 2, 5, 7, 9, 10, 13 each constructed at least once. Two of these are
exactly the pages the low-memory guard (`src/ui/miniacid_display.cpp`,
ported from the P3 branch earlier this session) was written to protect and
has been observed blocking on every prior build tonight:

- **Page 5 (Drum Sequencer)** — needs ~1,528 B by this session's own
  empirical table. Created successfully with **29,980–30,704 B left over**
  after construction, every time it was requested. Never once hit the "low
  memory" guard.
- **Page 10 (Project)** — needs ~2,176 B (this table's largest, scene/sample-
  list dependent entry) and was the page most consistently blocked on every
  build before FS1B tonight. Created successfully with **32,124 B left**.

`[DSP] START command received` was exercised live (audio engine actually
running, not just idle) with no underruns reported and no reset. The capture
ends with `up=57436ms` and the loop task still on ordinary `phase=control`
checkpoints — no watchdog, no panic, no `RDIAG-PREV` (crash-retained) record
at any point in this file.

## What this does and does not close

This closes the hardware half of WT1 for **this specific ELF**: FS1B's
dynamic-FatFs candidate boots, mounts SD, survives sustained real navigation
across the two previously-blocked pages, and leaves DRAM in a completely
different regime (tens of KB free, not hundreds of bytes) than every prior
measurement on this device tonight.

It does **not** by itself close everything the census flagged as
still-required:

- **Fragmentation over time / long-session soak** — 60 s is not the "20
  reconnect cycles" or extended-session check the memory plan's M2/M3
  sections call for. The SMF-lazy fix earlier tonight is a direct precedent
  for why this matters: it recovered real headroom at boot, and *still* hit
  a fragmentation-caused failure (a 6,144 B task-stack allocation against a
  5,108 B largest-contiguous-block after ~80 s of navigation) despite total
  free memory being higher than the request. A longer soak on this FS1B
  image, deliberately including repeated file-touching operations (scene
  save/load, sample browse, SMF load if attempted), is the next real gate,
  not an assumption that 30 KB of headroom makes fragmentation moot.
- **nanoKEY2 Host-mode admission** — this run did not attempt USB Host mode
  at all. The `docs/midi/2026-09-08-memory-report.md` gap analysis (written
  before this FS1B result) estimated the device was short by "10-15 KB more
  headroom" against the plan's own admission rule. FS1B's ~24 KB net swing
  in `minFree8Boot` plausibly closes most or all of that gap, but this has
  not been measured with Host mode actually enabled — M1 (Host's own
  incremental cost, measured inside this product, not the isolated H0
  probe) is still not done, and admission math should be redone against
  these new numbers rather than assumed.
- **Static DRAM gate re-validation** — `provisional_status=exception_unvalidated`
  was still printed by the build script itself; this run is a runtime/hardware
  result, not a resolution of that provisional static-budget exception.

## Bottom line

FS1B is hardware-validated on real Cardputer ADV for boot + SD mount + full
page navigation, with zero crashes and a dramatically higher memory floor
than anything measured on this device before it. It is a strong, evidenced
step toward closing MEMORY-R1 — not yet the full R1 acceptance (soak testing
and a redone nanoKEY2 admission calculation remain open), and it does not
retroactively validate the source-ownership claims in the FS1B census, which
is a separate kind of evidence this session did not re-derive.
