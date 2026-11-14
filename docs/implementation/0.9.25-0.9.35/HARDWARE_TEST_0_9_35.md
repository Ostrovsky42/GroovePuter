# GroovePuter 0.9.35 — owner hardware acceptance

Status: **APP FLASH WRITTEN AND READBACK VERIFIED — HARDWARE ACCEPTANCE PENDING**.
Only the firmware write, readback hash and USB re-enumeration were observed.
Display, audio, keyboard, SD and MIDI acceptance remain pending.

## Candidate identity

- Candidate version: `0.9.35-rc+615ac96`
- Source commit SHA: `615ac9642f1103ce1f9d4fcddcb083710b171db4`
- Candidate bundle: the archive containing this checklist; verify its sidecar.
- Verify the bundle against its adjacent `.zip.sha256` sidecar; do not rely on
  a copied checksum value.
- Merged artifact: `artifacts/GroovePuter.ino.merged.bin`, 4,194,304 B,
  SHA-256 `2d3d78a10baf44af9e966b42fd8d72326412d01dcef2cadd40daf360058d5c19`,
  flash address `0x0`; spans the first 4 MiB of the board's 8 MiB flash.
- App-only artifact: `artifacts/GroovePuter.ino.bin`, 1,808,512 B,
  SHA-256 `6a6c920b3cab4c241464b02209680962743211d4d592af6c6adfd5a315339567`,
  flash address `0x10000`; use only when the installed bootloader and partition
  table match the candidate.
- Observed device: ESP32-S3 QFN56 rev 0.2, 8 MiB GD flash. The current app
  contained GroovePuter; enclosure revision was not independently inspected.
- Candidate-compatible layout read from device: bootloader at `0x0` SHA-256
  `71242cb75e7388e23b901146ba56790eb8ab9b0667e1cbc3c28e6770fe60296f`,
  partition table at `0x8000` SHA-256
  `aaae2888c5a6a348004b5b436f47abb25ae32e72d9003902955a998eda723edd`, and
  `boot_app0` at `0xe000` SHA-256
  `f94c5d786a7a8fab06ac5d10e33bf37711a6697636dc037559ea19cc410a17f0`.
- Test date / operator: `2026-11-14 / Luna, at owner's request`
- Previous accepted rollback: `.16` merged image,
  SHA-256 `53f49d6ad92a386a6a0034f754db22d43e15107fe7f8d0e05e2960a2c62ec39f`;
  full restore steps are in `rollback/ROLLBACK.md` in the bundle.
- SD card make/capacity: `PENDING`
- External MIDI device / firmware: `PENDING`

## Install and recovery instructions

1. Back up project files from the SD card. The app-only update preserves other
   flash partitions; the merged image overwrites the first 4 MiB and can erase
   NVS/SPIFFS data in that span.
2. Extract the bundle and verify `sha256sum -c SHA256SUMS.txt`; also verify the
   bundle SHA against the `.zip.sha256` sidecar before extracting.
3. For an app update, first confirm the existing bootloader at `0x0` and
   partition table at `0x8000` match the candidate. On the connected device,
   both matched exactly. Then write the app-only image at `0x10000`:

   ```sh
   esptool --chip esp32s3 --port <PORT> --baud 115200 --no-stub --after hard_reset write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x10000 artifacts/GroovePuter.ino.bin
   ```

   This exact app-only write was performed with esptool v4.7.0. Esptool
   reported `Hash of data verified`; an independent `verify_flash` reported
   `verify OK (digest matched)`. The board re-enumerated on USB after reset.
   The display and application workflow were not observed.

4. Use the merged image at `0x0` only for an intentional full-layout install
   after backing up internal flash data. It is 4 MiB on an 8 MiB device; it does
   not represent a full readback backup. The command is:

   ```sh
   esptool --chip esp32s3 --port <PORT> --baud 115200 --no-stub --after hard_reset write_flash --flash_mode keep --flash_freq keep --flash_size keep 0x0 artifacts/GroovePuter.ino.merged.bin
   ```

   Keep the accepted `.16` rollback bundle available and see
   `rollback/ROLLBACK.md` for recovery details.

Record the candidate SHA from both the handoff manifest and firmware diagnostic
identity before testing. If they disagree, stop and resolve provenance first.

## Result key

For every row record `PASS`, `FAIL`, or `NOT RUN`, plus a short observation. A
test result applies only to the named hardware, firmware SHA and setup. Stop the
session on a suspected data-loss issue, stuck note, repeated crash, overheating
or unrecoverable boot failure; preserve logs and use the documented rollback.

## A. Install and startup

| Check | Expected result | Result / notes |
|---|---|---|
| Flash candidate with its documented package/path | Tool accepts the exact artifact; no address/layout guesswork | `PASS` — app-only at `0x10000`; esptool write hash and independent readback verification passed; USB re-enumerated after reset (2026-11-14) |
| Cold start | GroovePuter reaches the main UI without a reset loop | `NOT RUN` |
| Second cold start | Same candidate starts again and retained project data remains available | `NOT RUN` |
| Display | Text and selected states are readable on the physical 240×135 LCD | `NOT RUN` |
| Keyboard | Documented key actions work; no unusable required chord blocks the Golden Path | `NOT RUN` |
| SD card | Project browser lists files; a project can be saved and reopened | `NOT RUN` |

## B. Internal sound

| Check | Expected result | Result / notes |
|---|---|---|
| Start and stop playback | Sound starts and stops; no stuck internal notes | `NOT RUN` |
| Generate and change genre | New material plays and the genre change is audible/visible as expected | `NOT RUN` |
| Bass, synth and drums | Each committed part is audible and remains in time | `NOT RUN` |
| Song section transition | Transition occurs at the displayed section boundary without hanging playback | `NOT RUN` |
| Sustained playback | No freeze, reboot or progressive timing failure during the agreed listening interval | `NOT RUN` |

Record audible defects separately from software correctness. Musical usefulness
is an owner judgment and cannot be inferred from this checklist's structural
results.

## C. Performance macros

For each macro, record the source Song/Pattern, starting value, change made and
audible result.

| Check | Expected result | Result / notes |
|---|---|---|
| TENSION | Bounded performance change; no loss of the committed source material | `NOT RUN` |
| ENERGY | Bounded intensity change; primary rhythmic anchors remain | `NOT RUN` |
| SPACE | More/less room as indicated; no unintended route change | `NOT RUN` |
| MUTATION | Bounded variation related to the source; no hidden permanent overwrite | `NOT RUN` |
| Reset and switch | Neutral reset is clear; switching macro during play does not hang or leave stuck sound | `NOT RUN` |
| Save/reload | Temporary macro values do not silently replace the committed project | `NOT RUN` |

## D. USB MIDI and SEQTRAK

| Check | Expected result | Result / notes |
|---|---|---|
| USB connection / enumeration | Expected endpoint appears and selected profile is visible | `NOT RUN` |
| Note On / Note Off | Receiver plays notes and releases them at the expected time | `NOT RUN` |
| Stop during notes | Stop releases all notes owned by GroovePuter | `NOT RUN` |
| Clock and tempo change | Receiver follows one clock source and responds to tempo change without runaway tempo | `NOT RUN` |
| Channel / role routing | Synth and drum roles reach the selected destination channels/voices | `NOT RUN` |
| SEQTRAK profile and CC | Only documented profile messages are sent; note and controller response is recorded | `NOT RUN` |
| Route change | Profile/output controls are unavailable during playback; after stop/restart new settings take effect | `NOT RUN` |
| Disconnect / reconnect | Disconnect does not leave audible stuck notes; reconnect returns to a usable state | `NOT RUN` |

Capture the selected Device Profile, channels, USB role, connection order and
any observable LED/display state. Host fake-transport traces do not substitute
for this section.

## E. Full user path

| Check | Expected result | Result / notes |
|---|---|---|
| Generate | Genre-based material is created and played | `NOT RUN` |
| Keep/edit | Accepted Pattern/Phrase can be changed and Undo restores the prior state | `NOT RUN` |
| Develop | Related material is added without replacing the source unexpectedly | `NOT RUN` |
| Arrange | FORM creates editable Song rows and section transitions are audible | `NOT RUN` |
| Save/load | Save, restart, reload and continue editing preserve the project | `NOT RUN` |
| Export MIDI | Export creates a `.mid` file in `/midi`; copy it to a computer and open it in an independent parser/DAW | `NOT RUN` |
| Reopen and continue | Reloaded project remains editable after export | `NOT RUN` |

The SMF is an interchange projection. Temporary macros, Voice audio, Drum FX
and internal Drum automation are not represented as rendered audio; conditional
steps are flattened and marked.

## F. Reliability and diagnostics

| Check | Expected result | Result / notes |
|---|---|---|
| Rapid page changes during playback | UI remains responsive and playback state remains coherent | `NOT RUN` |
| Repeated Play / Stop | Every stop leaves no stuck internal or external notes | `NOT RUN` |
| Song boundaries and Note Off | No hanging or prematurely cut notes at repeated section transitions | `NOT RUN` |
| MIDI disconnect cycles | Repeated disconnect/reconnect does not crash or strand notes | `NOT RUN` |
| SD write error handling | Failed save/export is reported; no failed export appears as a valid final file | `NOT RUN` |
| Extended session | No unexpected reset, lockup or unrecoverable state during the agreed duration | `NOT RUN` |

For each failure capture: candidate SHA, exact action sequence, selected profile,
current Song/section, whether transport was active, connected endpoint, screen
message, approximate timestamp, reset reason and serial/USB diagnostics if
available. Preserve the project and any temporary/export files before retrying.

## Acceptance summary

- Failures / severity: `PENDING`
- Questions that require listening or physical operation: `PENDING`
- Rollback used: `PENDING`
- Owner decision: `NOT ACCEPTED`
- Accepted areas: `NONE RECORDED`
- Rejected areas / follow-up issues: `PENDING`

Only the owner records hardware acceptance. This limited app write/readback
does not establish startup, internal audio, USB MIDI, SEQTRAK or musical quality.
Record those outcomes separately; one passing area does not imply another passed.
