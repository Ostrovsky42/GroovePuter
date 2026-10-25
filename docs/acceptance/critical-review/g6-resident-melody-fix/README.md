# Resident Melody validation — software checkpoint, G6 OPEN

Parent: b4eb046a (the local page/final-ack repair), based on accepted #495. Existing hardware still runs accepted #495; this image is NOT flashed.

RED: A+B accepted 8-bar Melody files corrupt; Load reports SUCCESS, canonical/project switches, neither voice holds a loaded Melody. GREEN: all resident Melody descriptors, including unselected slots, must decode through the existing A/B/legacy fallback before Scene publication. Header corruption, full-size CRC corruption, missing files and nonselected material all refuse without replacing canonical/project or losing either voice's unaccepted Working Melody. Positive intact project load is tested first. Run tests/run_g6_failed_load_tests.sh.

Host suite and the previous refusal checks PASS. SDL/ADV PASS. Independent read-only review finds no definite introduced Critical/Important correctness issue; it flags runtime memory and later reads as unverified. A deterministic runtime projection corpus of 3072 cases passes ASan/UBSan; no loaded-input late Pattern-bank failure was reproduced. This does not prove arbitrary fault atomicity or certify the full Load pipeline.

## Memory and limits

.data 41996, .bss 142256, fixed DRAM 184252 (unchanged), flash 1593262 (+1604 versus #495, +1080 versus previous narrow repair). Limit 191488 unchanged.

ESP32 compiler stack-usage measurement, same target arguments with -fstack-usage and source include paths restored: Scene loader 2928, Melody loader 2816, decoder 1344 bytes. Sum of these nested frames is 7088 bytes; caller/filter/virtual I/O frames are additional. This is not actual 32-KiB loop-stack headroom. Runtime heap/largest/high-water remain OPEN. The existing loader can retain two file vectors and a legacy fallback vector; its whole-file resize lacks an encoded-size cap. Oversized corrupted-file failure must be checked before qualifying this path on hardware.

Only resident page 0 is checked. Nonresident pages/Melody files remain OPEN. Selected Melody is read again during hydration; an I/O fault between validation and hydration is still outside the transaction boundary. No new owner, queue, Scene buffer or audio-thread allocation was introduced. Power-cut atomicity UNVERIFIED. No push/merge.
