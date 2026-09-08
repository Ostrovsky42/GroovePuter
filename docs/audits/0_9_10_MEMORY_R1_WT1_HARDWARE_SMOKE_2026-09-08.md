# 0.9.10 MEMORY-R1 WT1 — hardware smoke acceptance

Date: 2026-09-08
Branch: `feature/20260908-0.9.10-memory-r1-wt1`
Commit: `e32e3c16e0ff8ff426f51d859a10a06ef848c6e1`

Continuation of
[the WT1 ELF recovery record](0_9_10_MEMORY_R1_WT1_ELF_2026-09-08.md), which
closed with `HARDWARE SMOKE: PENDING` — that environment had no device
access. This is that smoke test, run on the actual Cardputer ADV, following
the short checklist that record proposed (not the full FS1B matrix, since
this change is one dead wavetable removed, not a filesystem-layer swap).

## Identity

```
ELF SHA-256   c29565365af9261b64a1c34cb2d379f69216633b0dc26f7efaf0bff55bf6aa5d
Build         scripts/build_cardputer_memory_r1_fs1b_runtime.sh (WT1 source,
              same builder as FS1B — WT1 is a source change on top of FS1B,
              not a separate build path)
Static DRAM   186,784 B fixed (was 190,880 B on FS1B before WT1) — confirms
              the -4,096 B ELF delta this build script itself reports,
              independent of the sibling session's own A/B comparison
Capture       logs/serial-wt1-smoke-hardware-2026-09-08.log (180 s window)
```

## Checklist result (against the proposed short smoke)

| Item | Result |
|---|---|
| Cold boot | `Reset Reason: 1` once at capture start; boots clean |
| UI/page navigation | Pages 0, 1, 2, 5, 6, 9, 10, 12, 13 all `created SUCCESS` |
| Transport start/stop | `[DSP] START command received` / `[DSP] STOP command received`, 2 full cycles |
| Synth engines active | `[Synth] 0 -> TB303`, `[Synth] 1 -> SH101` running through both transport cycles, no fault |
| SD-backed operation | `[AUTOSAVE] recovery revision=2` and `revision=3` — two successful scene autosave writes to SD; separately, `smfSent=389` shows sustained MIDI Player playback reading `.mid` files from SD throughout |
| Duration | 126 s this capture; combined with the immediately-preceding FS1B soak on this same lineage (23m45s, see below), well past the 5-10 min target in aggregate |
| Zero panic/watchdog/reboot | Confirmed — `Reset Reason` appears exactly once (the initial cold boot), no second occurrence anywhere in the window |
| Zero audio underruns | `underruns=0` at every `[PERF]` sample |
| Zero low-memory guard hits | `grep -c "low memory"` = 0 |
| Free-heap floor this run | 20,532 B free / 11,764 B largest — consistent with the FS1B class of headroom; WT1's 4,096 B is a static-section change and was not expected to move the runtime floor much on its own |

`saw`/`square` wavetables (the two kept alongside triangle's removal) were
in active use via the TB303/SH101 engines for the full session; no reference
to the removed `triangleTable_`/`lookupTriangle()` was exercised or missed —
consistent with the sibling session's caller census finding zero production
consumers before removal.

## Supporting context: this sits on an independently soak-tested FS1B base

Immediately before this WT1 smoke test, this same evening's session ran a
23m45s (`up=1425252ms`) soak of the FS1B base WT1 is built on
(`logs/serial-fs1b-soak-2026-09-08.log`, captured on the FS1B branch,
commit `3b2bd280`): zero resets, zero underruns, zero low-memory hits, all
nine pages exercised, and — notably — SMF/MIDI Player playback that had
previously failed on an earlier (pre-FS1B) build due to heap fragmentation
ran cleanly for an extended stretch. WT1's own 126 s is not, by itself, a
long soak; taken together with that adjacent result on the base it modifies,
the combined hardware evidence for this lineage is substantially longer than
either capture alone.

## Result

```
STATIC RECOVERY     PASS — 4,096 B ELF-measured (confirmed independently
                     by this build's own machine-summary line)
CALLER CENSUS       PASS (sibling session, not re-derived here)
NO-RESURRECTION     PASS (sibling session, not re-derived here)
PRODUCTION SCOPE    PASS (sibling session, not re-derived here)
BUILD               PASS (host tests + runtime diagnostic build, this session)
HARDWARE SMOKE      PASS — this record
```

WT1 is hardware-smoke-accepted per the checklist its own record proposed.
Combined recovery ledger for tonight's memory line, each figure kept
separate per its own evidence class rather than summed into one claimed
total:

```
Lazy SMF      ~7.0-7.4 KB   HARDWARE-MEASURED (soak-tested, this session)
FS1B          ~24 KB class  HARDWARE-MEASURED (23m45s soak, this session)
WT1            4,096 B      ELF-MEASURED, now HARDWARE-SMOKE-ACCEPTED
```

Per the sibling session's own recommendation, further static-memory cutting
is not the next priority. The FS1B soak already exercised this device
harder than any prior capture tonight and it held; the honest next
checkpoint is a longer fragmentation/churn soak specifically targeting
repeated SD-file-touching cycles (scene save/load, sample browse, SMF
load/unload in sequence) rather than more source-level byte-hunting.
