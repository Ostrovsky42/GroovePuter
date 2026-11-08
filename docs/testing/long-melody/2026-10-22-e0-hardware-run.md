# Long Melody E0 hardware run — 2026-10-22

Status: **PARTIAL**. The earlier source-tagged no-census image `2d97ac49…` has one clean cold boot and a same-boot post-churn plateau. The corrected review image `31e12c81…` ran PLAY/STOP without underruns, but its cold boot was not captured. The latest installed diagnostic image is `50d15ed9…`; its first 180-second log shows stable idle, while the `GEN FAILED` cause remains under investigation. E0 repeatability and controlled slot switching are open on the latest image.

- Flashed BIN SHA-256: `5df406a2f8aa2fa2ef6bcb91859f50b309c059f5a55fbd3c31188e5fbead2940`.
- Serial captures: `logs/serial-180s-20261022-134626.log` (Reset, boot, PLAY, page changes) and `logs/serial-180s-20261022-134934.log` (continued PLAY).
- Boot reached `[BOOT-STAGE] 100 setup-complete`; `[E0-STATE]` reported `sdMounted=1`, `songLoaded=1`, and `playing=0` after startup. The later PLAY state was `playing=1`; UI page values 12, 0, 6, and 2 appeared in the first capture.
- All observed `[MEM-BASE]` samples reported `integrity=1`; all observed `[PERF]` samples reported `underruns=0`. No panic/abort line or unexpected reboot was observed.
- During the second 180-second PLAY capture, free internal 8-bit heap ranged from 23,116 to 23,392 B. Largest internal 8-bit block was 14,836 B in every sample, so this interval showed no downward ratchet. This is a PLAY plateau only, not a post-churn recovery result.
- The post-Reset boot reported loop stack free 23,228 B, audio stack free 7,076 B, and dispatch stack free 2,396 B at `runtime-start`. During later PLAY, audio stack free reached about 6,640 B.
- Scene load telemetry reported one load of 114,616 B in about 1.33 s. `openFail=4` was already present at boot; this counter alone does not identify which optional file lookups failed.
- Follow-up serial capture `logs/serial-180s-20261022-135616.log` recorded STOP and START transitions, several Q–Y slot-key presses, two Scene saves, and a second physical Reset. The second boot again reached `setup-complete`, mounted SD, loaded Song, and stayed idle without panic or underruns. Both captured `runtime-start` snapshots had a largest internal block of 17,396 B; after warm-up on UI page 2, the second boot held 14,836 B for over a minute. The saves coincided with UI pauses up to about 2.8 s and a brief drop of the largest block to 14,324 B; these are observed costs, not audio underruns.

The two clean boots and stable PLAY/idle plateaus justified advancing to M2 measurement. The missing same-boot post-churn plateau remains a closure item for formal E0 acceptance.

## Source-tagged no-census rebuild

- Rebuilt the runtime diagnostic without `GROOVEPUTER_MELODY_CENSUS`, retaining E0 heap/stack/audio/SD telemetry and the added `sourceA/sourceB` fields.
- BIN SHA-256: `2d97ac49eeae7887a8042612eeba8b620b19895ea81d217e8daff5052159cff0`.
- Fixed DRAM: 190,240 B; candidate-only FatFs link proof passed.
- Upload to `/dev/ttyACM0` exited 0 and esptool verified the data hash.
- The following 180-second monitor (`logs/serial-180s-20261022-153526.log`) connected but received no firmware text. No physical Reset was recorded during this window, so boot, runtime telemetry, and Pattern-only E0 interactions are **NOT VERIFIED** by this capture. The existing earlier no-census evidence above remains partial; the new build hash is ready for a monitor-before-Reset baseline.
- Two automated ROM reset attempts (`esptool run`) were then made without reflashing. One default attempt stopped at a missing system esptool stub JSON before it could reset; a `--no-stub` run completed its RTS reset. The subsequent 60-second captures (`logs/serial-60s-20261022-153930.log` and `logs/serial-60s-20261022-154042.log`) also received only the monitor connection line.
- Diagnosis: `esptool chip_id` with `--before no_reset --after no_reset` connected successfully and returned the ESP32-S3 MAC (`d8:85:ac:a4:4b:ec`), then explicitly stayed in bootloader. This confirms the board was in ROM download mode during the empty captures; the E0 application had not started. Release G0 and press the physical Reset button, then capture Serial. The firmware itself has not been shown to fail boot.

## Source-tagged no-census cold boot and churn — 2026-10-22

The monitor was attached before physical Reset on the already flashed no-census image (`2d97ac49eeae7887a8042612eeba8b620b19895ea81d217e8daff5052159cff0`). The new capture is `logs/serial-180s-20261022-185259.log`; the follow-up idle capture is `logs/serial-60s-20261022-185937.log`.

The later review build from the corrected diagnostic source is `build/long-melody-e0-review-runtime/GroovePuter.ino.bin` (SHA-256 `31e12c81297a01b29346288f41938867fc29b00cee24efcb9f9c1ff5814f0e16`). It fixes an instrumentation-only `SD.open` replacement inside a debug string. The hardware observations below belong to the previously installed `2d97ac49…` image; they do not validate the review BIN.

- The physical Reset reached `[BOOT-STAGE] 100 setup-complete`. Runtime startup reported `sdMounted=1`, `songLoaded=1`, `sourceA=STEPS`, `sourceB=STEPS`, `integrity=1`; runtime-start free heap/largest block were 25,616/17,396 B, and audio stack high-water free was 7,044 B.
- Pattern-only PLAY and STOP were exercised twice. Pages changed 2→7→2→7→2→10→13→2. Playback reached 68% average audio / 83.1% peak with zero underruns. No crash or unexpected reboot occurred.
- After the navigation and transport activity, the same boot remained idle for more than 60 seconds in the follow-up capture. It held 21,484 B free internal heap, 12,788 B largest block, and `integrity=1`; `sdMounted=1`, `songLoaded=1`, and `playing=0` remained stable. This closes the same-boot post-churn plateau for this run.
- Scene autosave caused a measured UI peak of 2,178,910 µs and a transient boot-minimum heap reading of 15,228 B; the heap returned above 21 KB and audio underruns stayed at zero. `sceneSaveCount` later reached three serialization attempts, and `sceneSaveMaxUs` reached 2,459,573 µs for JSON serialization/write/flush. That field excludes file close, verification, and promotion; keep the full UI pause visible in later endurance checks.
- The second cold boot requested after this run was not captured: `logs/serial-60s-20261022-190134.log` stayed at the same increasing uptime and had no new boot markers. The device remained healthy and idle, but the repeatability criterion is still open.
- USB-MIDI diagnostics accumulated endpoint-busy rejects and transport-send failures while this internal-audio run was active. No external MIDI acceptance is inferred from this; inspect it separately in E1–E6 MIDI testing.

## Post-review upload of corrected diagnostic — 2026-10-22

- Flashed `build/long-melody-e0-review-runtime/GroovePuter.ino.bin`, SHA-256 `31e12c81297a01b29346288f41938867fc29b00cee24efcb9f9c1ff5814f0e16`, to `/dev/ttyACM0`. `scripts/upload.sh` exited 0; esptool verified the written data hash and reported the serial port again after RTS reset.
- The immediately following 60-second monitor, `logs/serial-60s-20261022-201813.log`, connected but captured no firmware lines. This is not a boot result. The next test is monitor-before-physical-Reset with G0 released, followed by a second captured boot and controlled slot switching on this exact image.
- A later 180-second monitor, `logs/serial-180s-20261022-202217.log`, captured the running application on `31e12c81…`: 176 state/heap samples with SD mounted, Song loaded, A+B sources both STEPS, `integrity=1`; 36 performance samples had `underruns=0` and peak audio load 90.9%. PLAY/STOP and UI pages 1, 2, and 5 were observed. The window began at uptime 49 s and contains no `setup-complete` or `runtime-start` line, so it confirms runtime operation but not a captured cold boot or E0 repeatability. The user reported a `GEN FAILED` toast while pressing G on Synth pages; the log records repeated G key events but the current runtime diagnostic does not print the generation result/status, so the precise failure cause is still under investigation.

## GEN FAILED diagnostic upload — 2026-10-22

- The runtime-only instrumenter now prints `[GEN-DIAG]` on a failed Synth-page G, recording `result`, quantized status, genre/recipe, and migration status/context. It does not change generation logic. The FS1B build passed with fixed DRAM 190,240 B and no stock FatFs references.
- `build/long-melody-e0-gen-diagnostic/GroovePuter.ino.bin` SHA-256 `50d15ed9a036331c0ea9982e53a9be527b8455f329b22e0584d7b01e306ccb34` was flashed to `/dev/ttyACM0`; esptool verified the data hash and the port returned.
- `logs/serial-180s-20261022-203552.log` captured stable idle telemetry (`sdMounted=1`, `songLoaded=1`, both sources STEPS, `integrity=1`, `underruns=0`) but no G key event or `[GEN-DIAG]` line. A controlled G press at STOP and PLAY is still needed to identify the actual failure branch. This window also has no cold-boot marker.
- A further 180-second capture, `logs/serial-180s-20261022-223108.log`, found the same installed diagnostic image running at STOP on Synth A (`uiPage=1`, A/B STEPS). Its periodic heap samples retained `integrity=1`, largest internal block 14,836 B, and PERF samples reported `underruns=0`. It contains no `[KEY]`, `[DSP] START`, or `[GEN-DIAG]`, so it adds idle stability only and does not identify the earlier `GEN FAILED` cause.

## GEN diagnostic reproduction — 2026-10-23

- `logs/serial-180s-20261023-100412.log` captured active PLAY on Synth A/B with the installed runtime diagnostic. It contains 44 `[GEN-DIAG]` results; all 44 were `result=0 status=4` (`Failed` with quantized status `Busy`), across voices A and B and genre IDs 4–12. Thus the captured refusals came from the shared pending-generation queue before genre preparation; the genre varied and is not the cause of these 44 failures.
- The capture contains 100 G key events and transport activity. It does not isolate one accepted PLAY request followed by a second G after a full bar without other input, so it cannot establish whether the pending slot clears at the next boundary. On one observed sequence, G during STOP produced no failure diagnostic; PLAY then accepted one G (no diagnostic) and rejected the next about one second later with Busy, after which STOP followed about one second later.
- The follow-up `logs/serial-60s-20261023-100822.log` remained at STOP on Synth A and captured no keys, so the controlled post-boundary retry was not obtained. Both logs retained `integrity=1` and `underruns=0`; this is transport/runtime health evidence, not proof of correct queue release.

- After the captured run, the user confirmed by direct hardware observation that the pending queue does free. This closes the queue-release question as a user-reported result; it is not visible in the earlier serial captures. The rapid retry is therefore expected to return Busy until release. A new runtime image was built and flashed on 2026-10-23 with the UI feedback changed to `GEN BUSY: WAIT`; bin SHA-256 `12241952004829a4ad33640e7d2a38668321f33c5c95a356c0511a0f41bd5a9a`. FS1B candidate-only FatFs link proof passed, fixed DRAM was 190,240 B (1,248 B below the provisional 191,488 B gate), and esptool verified the upload hash. Hardware confirmation of the new toast remains pending.

- After the 2026-10-23 upload, the attached 60-second Serial window (`logs/serial-60s-20261023-105203.log`) received periodic runtime output from the new image: `sdMounted=1`, `songLoaded=1`, `integrity=1`, `playing=0`, and `underruns=0`; no reset or crash appeared. It did not capture any G key events, so the new Busy toast is not yet verified on-device.
