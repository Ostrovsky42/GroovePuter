# External MIDI compatibility acceptance

## Accepted hardware baseline

| Field | Value |
|---|---|
| GroovePuter firmware | `b256ff180165e8db37e61be8658b13c0ae2bcd5c` |
| Commit | `feat: follow SEQTRAK MIDI clock and transport (#25)` |
| Hardware | M5Stack Cardputer ADV, ESP32-S3FN8, PSRAM disabled |
| Receiver | Yamaha SEQTRAK |
| Receiver OS | 2.00 |
| Connection | Direct USB-C data connection |
| USB role: GroovePuter | Class-compliant USB-MIDI device |
| USB role: SEQTRAK | USB host |
| Date recorded | 2026-08-03 |

This acceptance applies to the exact firmware SHA above. It must not be silently generalized to a later `dev` head after MIDI ownership, routing, transport, or USB lifecycle changes.

## Tested behavior

| Capability | Result | Notes |
|---|---|---|
| USB enumeration | PASS | SEQTRAK enumerates GroovePuter as a class-compliant USB-MIDI device. |
| Note On / Note Off | PASS | Direct Cardputer ADV to SEQTRAK. |
| Native SEQTRAK routing | PASS | CH1-7 drums, CH8 Synth 1, CH9 Synth 2, CH10 DX; CH11 remains the Sampler destination. |
| GroovePuter MIDI Clock output | PASS | SEQTRAK follows GroovePuter when GroovePuter owns clock. |
| GroovePuter Start output | PASS | SEQTRAK transport starts. |
| GroovePuter Stop output | PASS | SEQTRAK transport stops. |
| SEQTRAK external-master clock follow | PASS | GroovePuter follows clock received from SEQTRAK. |
| SEQTRAK Start receive | PASS | PROJECT transport starts from MUSIC START. |
| SEQTRAK Stop receive | PASS | PROJECT transport pauses/stops according to the accepted lifecycle. |
| SEQTRAK Continue receive | NOT AVAILABLE FROM TARGET | SEQTRAK OS 2.00 did not transmit MIDI Continue (`0xFB`) during the physical Stop -> Play workflow. |
| PROJECT SMF synchronization | PASS | PROJECT playback follows the accepted external clock and transport behavior. |
| Physical SEQTRAK Stop -> Play | PASS WITH DOCUMENTED SEMANTICS | The second Play produces Start behavior, so the active PROJECT SMF restarts from MUSIC START rather than preserving its former position. |
| Position-preserving Continue | CAPABILITY, NOT SEQTRAK PATH | GroovePuter can preserve position when a controller actually transmits MIDI Continue (`0xFB`). |
| Song Position Pointer / seek | NOT CLAIMED BY THIS ACCEPTANCE | Capability-gated SPP work requires its own recorded test result. |
| Panic / cleanup matrix | NOT CLAIMED AS A SEPARATE TEST HERE | Later release acceptance must record Stop, route-change, disconnect, mute, and explicit Panic outcomes separately. |

## Important Continue semantics

SEQTRAK OS 2.00 transmits MIDI Clock, Start, and Stop in the tested workflow, but it does not transmit MIDI Continue when the user presses Play after a physical Stop.

Therefore:

```text
SEQTRAK Stop -> Play
  -> Stop, then Start
  -> PROJECT SMF restarts from MUSIC START
```

This is target behavior, not a GroovePuter failure to recognize Continue. Position-preserving resume is available only when the external controller sends `0xFB`.

## What this acceptance does not prove

It does not prove:

- compatibility with every later GroovePuter commit;
- direct USB-C compatibility with any untested instrument;
- target acknowledgement of notes, transport, mute, solo, or recording;
- SPP transmit/receive behavior;
- remote SEQTRAK CC23/CC24 state changes;
- compatibility through DIN/TRS or third-party USB host bridges.

## Required record for every future hardware test

Each new compatibility result must record:

- exact GroovePuter firmware SHA;
- instrument model and firmware/OS version;
- physical connection and cable/adapter topology;
- USB host/device roles, or DIN/TRS electrical path;
- selected GroovePuter MIDI profile;
- Note On / Note Off result;
- channel-routing result;
- Clock result;
- Start result;
- Stop result;
- Continue result;
- SPP/seek result;
- Panic and stuck-note result;
- whether the result is `TESTED`, `LIKELY`, `BRIDGE REQUIRED`, or `UNSUPPORTED`.

## Acceptance checklist for the next release candidate

- [ ] Flash one exact `dev` SHA and record it above the test results.
- [ ] Record the target OS/firmware version.
- [ ] Record USB roles and cable topology.
- [ ] Test Note On and Note Off on every claimed target lane.
- [ ] Test Clock in both ownership directions where supported.
- [ ] Test Start, Stop, and Continue as separate messages.
- [ ] Test physical Stop -> Play behavior.
- [ ] Test SPP/seek only when the profile claims it.
- [ ] Test local mute separately from remote target mute.
- [ ] Test explicit Panic, disconnect, route change, and source switch.
- [ ] Confirm zero stuck notes.
- [ ] Update the public compatibility table only from this recorded result.
