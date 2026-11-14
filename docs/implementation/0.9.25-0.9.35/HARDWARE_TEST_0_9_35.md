# GroovePuter 0.9.35 — owner hardware acceptance

Status: **PLANNED — NO HARDWARE TESTS PERFORMED**. This checklist is for the
single owner acceptance pass after the software candidate is ready. Blank result
fields are intentional; do not convert them to PASS from host or SDL evidence.

## Candidate identity

- Candidate version: `0.9.35`
- Source commit SHA: `PENDING`
- Firmware artifact and SHA-256: `PENDING`
- Install path / flash address: `PENDING_FINAL_PACKAGE`
- Previous accepted image and rollback instructions: `PENDING_FINAL_PACKAGE`
- Test date / operator: `PENDING`
- Cardputer ADV hardware revision: `PENDING`
- SD card make/capacity: `PENDING`
- External MIDI device / firmware: `PENDING`

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
| Flash candidate with its documented package/path | Tool accepts the exact artifact; no address/layout guesswork | `NOT RUN` |
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

Only the owner records hardware acceptance. Record Cardputer startup, internal
audio, USB MIDI, SEQTRAK and musical-quality outcomes separately; one passing
area does not imply another passed.
