# Long Melody M2 hardware run — 2026-10-22

Status: **M2 census PASS; internal-audio session accepted by user; full E1–E6 gate unverified**. The four-buffer residency measurement completed on Cardputer ADV. The first 180-second census was idle; a later source-tagged run captured B Melody with A Steps. The user reported that everything worked and ended device testing for now. Dual-voice Melody playback, edits, NEXT, save/load, SD-removal behavior, and external MIDI have no captured acceptance evidence.

- Flashed BIN SHA-256: `e96dab5b27f60b6bdfe6ff571b207090a8e915d6f191214557a1647c658f0275`; esptool verified the write and the device returned on `/dev/ttyACM0`.
- Serial capture: `logs/serial-180s-20261022-135953.log`.
- Boot completed with `sdMounted=1`, `songLoaded=1`, `playing=0`, heap integrity 1, and audio underruns 0.
- M2 allocated four internal 1,284-byte buffers: `allocations=4`, `allocFailures=0`. The staged largest-free-block readings were 14,836 B before buffers, 13,300 B at A active, 11,764 B with pending A, 10,740 B at A+B active, and 9,204 B with pending A+B.
- All 40 reload/activation cycles held `free=17,920 B` and `largest=9,204 B` at churn-10/20/30/40 and settled. Read latency: 16,415–19,884 µs; `failures=0`.
- The diagnostic file `/__gp_diag/lm-00.bin` was created uniquely and removed (`cleanup=removed`). The repeated completion line continued to report zero failures.
- No panic/reboot or audio underrun appeared during the capture. These are idle-state measurements with additive diagnostic buffers, not proof that real A+B Melody plus drums/FX is safe during PLAY.

Next hardware cases: after E0 repeatability is captured, use the source-tagged M2 image `545955f1…` documented below. Switch Synth A and B between STEPS and MELODY (`Alt+R` on each synth editor), then exercise A Melody/B Pattern, A Pattern/B Melody, and A+B Melody with drums and FX. For each combination, capture `sourceA/sourceB`, PLAY and STOP, NEXT requests before the same bar boundary, page/slot changes, and a 10-second idle plateau. Watch `underruns`, `integrity`, free/largest internal heap, allocation/load failures, and stuck notes. Keep 16-bar support and the full Phrase UI deferred until E0–E6 device evidence is complete.

## M2 monitored PLAY/STOP window (14:07–14:10)

- Capture: `logs/serial-180s-20261022-140728.log`; monitor stayed connected to `/dev/ttyACM0` for the full 180 seconds.
- One PLAY interval was observed from device uptime 460,902 to 506,737 ms (about 46 seconds), followed by STOP and over 130 seconds idle. The firmware did not emit per-synth SOURCE, so the A/B source combination and audible result are **unconfirmed**.
- All 178 sampled heap checks reported `integrity=1`; no panic, allocation/load failure, or monitor reconnect was recorded. The 36 performance samples reported `underruns=0` and peak audio load 29.3%.
- The largest internal free block settled at 8,180 B after STOP (runtime sampled minimum 7,668 B); free internal heap settled at 16,740 B. The current window does not establish a same-configuration before/after plateau because user navigation and a scene save occurred during the run.
- At this point E1–E6 had no tagged musical acceptance run. The later source-tagged session below covers A Steps/B Melody only.

## Source-tagged M2 build, upload, and initial empty windows

The generic runtime diagnostic now prints `sourceA` and `sourceB` as `STEPS`, `MELODY`, or `NONE` in every `[E0-STATE]` line. This reads the engine's active sequenced source for each synth and does not change playback. The source-regression test first failed without the fields and then passed with them; the Cardputer build and candidate-only FatFs link check passed.

- BIN: `build/long-melody-e0-m2-source-diagnostic/GroovePuter.ino.bin`
- BIN SHA-256: `545955f10e33f31e11361b8ef80e96703f8fea0bf0c47eabbc2459aa005eeea4`
- Fixed DRAM: 190,352 B, unchanged from the prior M2 image.
- Device upload: **PASS** on `/dev/ttyACM0` later the same day. `scripts/upload.sh` exited 0, esptool reported `Hash of data verified`, and the upload port reappeared after its RTS reset.
- Runtime boot/source-tagged PLAY capture: **NOT VERIFIED**. The immediately following 60-second and 180-second serial windows (`logs/serial-60s-20261022-144018.log`, `logs/serial-180s-20261022-144125.log`) each contain only the monitor connection line; no `[E0-STATE]` or boot record was received. The screen/physical-reset state is awaiting user confirmation. An empty serial capture is not evidence of a successful boot or of a failed firmware boot.

The planned next capture at that time was A Melody/B Steps, A Steps/B Melody, then A+B Melody with the internal synth and drums. Each combination needs `sourceA/sourceB` while `playing=1`, an audible check of both voices and stuck notes after STOP, and a post-STOP heap plateau. External MIDI remains a separate acceptance case because the user's MIDI device is unavailable.

## Source-tagged internal-audio session (15:13)

The user confirmed that GroovePuter opened after the upload. A later 180-second capture (`logs/serial-180s-20261022-151315.log`) restored live Serial telemetry without another firmware change. It recorded `songLoaded=1`, 176 state samples, and a PLAY interval with `sourceA=STEPS, sourceB=MELODY` (65 sampled seconds), including source toggles and STOP/PLAY transitions. The M2 census remained at four allocations, zero allocation/load failures, and `cleanup=removed`.

All 176 heap checks had `integrity=1`; 36 performance records reported `underruns=0`, peak audio load 72.0%. After STOP, free internal heap was 15,448 B and the largest free block 7,668 B. Scene save and Melody candidate activity occurred in the same window, so this is not a controlled before/after memory plateau. At the time of this capture, the user's audible assessment and stuck-note report were pending. There was no A=MELODY sample, and the session does not establish A+B Melody safety or external MIDI behavior.

A second 180-second capture (`logs/serial-180s-20261022-151633.log`) remained idle at `sourceA=STEPS, sourceB=MELODY`: 179 state samples, zero key presses, no panic, `integrity=1`, and `underruns=0`. Free/largest internal heap stayed at 15,448/7,668 B for the full idle window. This is a clean post-activity plateau for that settled state, but it still does not exercise A Melody or A+B Melody.

## Session close (15:26)

The user reported “everything works,” considered that hardware check closed, and requested no further tests for that session. Treat this as acceptance of the observed internal-synth session, not as captured evidence for the unrun E1–E6 cases or MIDI. E0 testing resumed later on the no-census image.
