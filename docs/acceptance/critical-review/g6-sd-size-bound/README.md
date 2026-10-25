# Bounded Melody SD read — narrow G6 repair

Base: 08a52482 (resident validation, itself based on accepted #495). Not flashed; hardware remains #495.

RED: the SD reader accepts a 1301-byte file and expands an output buffer to that size before decoder rejection. A damaged size can exhaust Cardputer heap. Both supported formats allow at most 128 ten-byte events; v1 header=16, v2 header=20, so no valid file exceeds 1300 bytes.

GREEN: SdFileSystem rejects sizes below16 or above1300 before vector resize, closes the file and leaves the output buffer/capacity unchanged. No new payload format, allocation pool or owner. ASan/UBSan verifies rejection at1301/65536/0/15 and acceptance of max128-event v2/v1 and minimum16-byte empty v1. Full clean host suite includes whole-Load oversized-file refusal retaining canonical/project and unaccepted Working A+B.

Read-only independent reviewer /root/g6_load_boundary_review found no introduced Critical/Important issue; all production read uses are Melody payload/readback. Existing decoder still validates content within the size range. Existing A/B/legacy fallback is retained.

Fresh host build, SDL forced rebuild and ADV build PASS. .data41996, .bss142256, fixed DRAM184252, limit191488 unchanged. Flash1593162 (versus accepted #4951591658: +1504; previous resident fix1593262: -100). Actual target heap/high-water/CPU and physical SD failure tests remain OPEN. Compiler optimization affects flash, not claimed runtime RAM savings.

G6 NOT COMPLETE: nonresident material validation, later hydration rereads, physical cold save/load and SD faults remain OPEN. No push/merge or flash performed. Reproduce with bash tests/run_g6_failed_load_tests.sh.
