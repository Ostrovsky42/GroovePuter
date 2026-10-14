# Fn+D frame composition and focused SDL M:SCALE check

Candidate: `/home/gg/.codex/worktrees/hgs-review-5e98`, HEAD `5e98acbd`
with uncommitted boundary fixes. Main checkout was not changed.

## Failure and correction

The diagnostic toggle draws through `drawUI()`, but the 40 ms periodic update
called `MiniAcidDisplay::update()` directly and erased the overlay. The overlay
also flushed separately after the page, allowing an intermediate page frame.

All Cardputer UI updates now use `drawUI()`. An optional callback composes the
enabled diagnostic overlay inside `MiniAcidDisplay::update()`, after the page
and toast, before its single flush. Splash rendering follows the same rule.
The callback has no resident storage or allocation. Toggling off uses the
normal page redraw. Only diagnostic builds supply the callback.

Boundary UI gate PASS: two consecutive updates retain the overlay at flush,
each performs one flush, and a subsequent update without the callback removes
it. Existing GRAPH/project-key/Ctrl+A tests also PASS. Final host run used -O0.
SDL build (`make -C platform_sdl -j2 CXX=g++`) and `git diff --check` PASS.

## Focused SDL evidence

Actual X11 window, 480x270 (240x135 logical), dummy audio. Keyboard input was
verified via X events and a debugger breakpoint at GenrePage::handleEvent.
The original keyboard layout group was restored afterwards. Source was not
modified while performing the interactive check.

| Scenario | Result | Screenshot |
| --- | --- | --- |
| M on HARMONY changes DOR to PHR | PASS | `/tmp/gp-mscale-11-phr.png` |
| CHR shows N/A:SCALE; held H shows M:SCALE without clipping | PASS | `/tmp/gp-mscale-12-chromatic-hints.png` |
| CHR to MIN restores GRAPH availability | PASS | `/tmp/gp-mscale-14-minor-graph.png` |
| M in GEN 2/3 preserves MIN on return; existing APPLY cycling remains | PASS | `/tmp/gp-mscale-17-gen-roundtrip.png` |
| MIN to MAJ shows matching body key and toast | PASS | `/tmp/gp-mscale-18-toast.png` |
| G after scale change reports Acid/BASE: NEW TAKE | PASS, UI feedback | `/tmp/gp-mscale-19-take-toast.png` |
| Additional combined repetition script | NOT RUN to completion | SDL window disappeared; cause not established |

Logs: `/tmp/gp-mscale-sdl-build.log`, `/tmp/gp-mscale-gdb.log`,
`/tmp/gp-overlay-frame-tests.log`.

## BLOCKERS FOR HARDWARE

No blocker established for the focused M:SCALE workflow. The broader SDL smoke
matrix was outside this request. The extra repetition script's missing window
must not be reported as a successful stress test.

## SDL UX FINDINGS

M:SCALE behaves correctly on the focused GENRE HARMONY row. Its footer hint is
visible while holding H; the normal footer shows project/profile information.
The scale and GRAPH refusal reason fit the 240x135 logical frame.

## NOT OBSERVABLE IN SDL

Physical Fn+D input/display persistence, USB Host, runtime heap/stack margin,
audible generated pitch/articulation, and Synth B release remain physical gates.
Dummy audio and NEW TAKE feedback do not certify audible results.

## Firmware artifact

Clean FS1B diagnostic Cardputer ADV build PASS. Profile matches the previous
diagnostic image: CDCOnBoot=default, PSRAM disabled, huge_app,
GROOVEPUTER_USB_ACCEPT_DIAG. Static DRAM 189,652 bytes (unchanged), flash
1,669,318 bytes (+52). Artifacts: `build/review-overlay-20261014/`.

ELF SHA-256: `e219c096007d0f87157c7ddfd9834c3a9e5c97ceaf2f00c6bdba85f0f5f4abeb`.
BIN SHA-256: `f2de97a4789ad95db20d177b09029297f3a8614287f0486c87ee86d2f7b2c8b6`.
Build log: `/tmp/gp-overlay-firmware-build.log`.

Upload PASS on `/dev/ttyACM0`: uploader exit 0, all written segment hashes
verified. Upload log: `/tmp/gp-overlay-upload.log`. Physical display persistence
after upload is not yet confirmed.
