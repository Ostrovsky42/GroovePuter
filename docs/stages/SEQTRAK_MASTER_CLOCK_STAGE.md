# SEQTRAK MASTER CLOCK STAGE

## Purpose

Provide two explicit transport roles while keeping Cardputer-Adv usable as a groovebox, MIDI instrument, PatternPlayer and SMF companion.

```text
GP MASTER
GroovePuter Clock / Start / Stop -> SEQTRAK

SEQ MASTER
SEQTRAK Clock / Start / Stop -> GroovePuter
```

Both modes continue sending PERFORM, PatternPlayer and SMF musical MIDI to SEQTRAK. `SEQ MASTER` suppresses only outbound system-realtime transport.

## Hardware list

- M5Stack Cardputer-Adv
- Yamaha SEQTRAK
- data-capable USB-C cable
- optional powered USB-C hub if power is unstable
- optional 3.5 mm cable: Cardputer audio out -> SEQTRAK AUDIO IN

Configure SEQTRAK before connecting Cardputer:

```text
USB Role                    To Device
USB MIDI In/Out             On
MIDI Clock Out              On
Transmit Sequencer Control  On
MIDI Sync                   Internal
```

SEQTRAK transmits `F8`, `FA` and `FC`, but not `FB`. GroovePuter still accepts
`FB Continue` from other MIDI controllers.

## Wiring

```text
SEQTRAK USB-C <----------> Cardputer-Adv USB-C
  F8/FA/FC     ---------> GroovePuter transport
  MIDI notes   <--------- PERFORM / PatternPlayer / SMF

Cardputer audio out ----> SEQTRAK AUDIO IN  (optional)
SEQTRAK audio out ------> headphones / speakers
```

PORT.A is not used. If unrelated I2C hardware is attached:

```text
SDA GPIO2
SCL GPIO1
3.3 V logic
```

## Controls

On MIDI Player:

```text
C       GP MASTER / SEQ MASTER
G       GP MASTER: local Run / Stop
        SEQ MASTER: EXT FOLLOW ON / OFF
Space   SMF Arm / Pause
T       ORIGINAL / PROJECT
Up/Down GP MASTER BPM
        SEQ MASTER BPM is read-only
R       restart from MUSIC START
X       player-scoped panic
```

`EXT FOLLOW OFF` is a local safety gate:

- F8 continues updating lock state and source BPM;
- FA/FB is ignored;
- switching OFF requests local Stop at the next audio boundary;
- switching ON does not replay an earlier command;
- a new FA or FB is required after switching ON.

## Persisted settings

Clock source and External Follow are stored in the existing versioned MIDI settings record.

```text
schema v2: 46 bytes
  existing MIDI routes/settings
  transportClockSource
  externalFollowEnabled

schema v1: 44 bytes
  decoded without data loss
  missing fields migrate to GP MASTER / FOLLOW ON
```

Implementation:

```text
src/midi/midi_companion_settings.*
src/midi/midi_companion_settings_codec.*
src/platform/cardputer_midi_settings_session.h
```

Cardputer storage:

```text
NVS namespace: grooveputer
key:           midi_cfg
```

The root UI loads settings once during construction. `C` and `G` persist changes from the UI task. No Preferences/NVS operation occurs in AudioTask, MidiDispatchTask or SmfPlayerTask.

Missing or corrupt data resolves to safe defaults:

```text
GP MASTER
EXT FOLLOW ON
```

## Realtime architecture

```text
SEQTRAK F8 / FA / FC
          |
          v
MidiDispatchTask             sole mutable TinyUSB owner
          |
          v
ExternalMidiTransportEventQueue
          |
          v
AudioTask block boundary
          |
          +-> ExternalMidiClockTracker
          +-> bounded phase PLL
          +-> MiniAcid / PatternPlayer
          +-> ProjectTransportTimeline
                    |
                    +-> PROJECT SMF scheduler
```

Invariants:

- no second USB task or mutable TinyUSB owner;
- no USB reads from UI, Arduino `loop()` or AudioTask;
- no engine mutation from MidiDispatchTask;
- no storage I/O in realtime tasks;
- fixed 128-event SPSC queue;
- eight entries reserved for Start/Continue/Stop;
- critical overflow causes LOST, local Stop and scoped cleanup;
- transport changes apply only on audio-block boundaries.

## Clock tracking and bounded phase correction

The tracker uses:

```text
5-sample median
1/8 EMA pulse-period filter
pulseOrdinal gap reconstruction
WAIT -> LOCKING -> LOCKED -> HOLD -> LOST
adaptive timeout for 5-300 BPM
```

One F8 equals four GroovePuter ticks at 96 PPQN, or one sixth of a project sixteenth-step.

Musical pulse position and accepted timing anchors remain separate. Every real F8 advances musical phase, while an unusable timestamp cannot poison the tempo median. F8 packets compressed into a 1 ms receive window are coalesced without creating false `pulseGaps`.

The engine is not hard-seeked on every F8. The follower compares external absolute project steps with the previous actual project timeline and applies one stable local drive state:

```text
slow trim:   -2%
neutral:      0%
fast trim:   +2%
```

```text
source BPM quantum:   0.1 BPM
base hysteresis:      0.15 BPM
enter trim:           |phase error| > 1/32 step
leave trim:           |phase error| < 1/96 step
max correction/block: 1/96 step
```

This avoids hard phase jumps and prevents a constant external Clock from causing an SMF tempo-reanchor every audio block.

## Hardware finding: SEQTRAK does not transmit Continue

The first hardware run exposed an incorrect assumption in the original stage
contract. MIDI Start, Continue and Stop are symmetric in the MIDI protocol, but
they are not symmetric in SEQTRAK's implementation:

```text
SEQTRAK transmit: F8 Clock, FA Start, FC Stop
SEQTRAK receive:  F8 Clock, FA Start, FB Continue, FC Stop
```

The authoritative transmit flow is documented in the Yamaha SEQTRAK Data List:

```text
https://usa.yamaha.com/files/download/other_assets/5/2226075/SEQTRAK_data_list_En_D0.pdf
```

### Previous behavior

After an active PROJECT SMF received `FC`, GroovePuter preserved its tick and
waited for a possible `FB`. Pressing Play on the physical SEQTRAK sent `FA`
instead. GroovePuter correctly interpreted that as a new transport epoch, but
then applied the ordinary launch policy. With `NEXT BAR`, this looked like:

```text
PAUSE -> manual RESTART MIDI / full-bar wait
```

There was no firmware error, but the workflow depended on a message SEQTRAK
cannot transmit and therefore was not usable from the hardware controls alone.

### Corrected behavior

An SMF that was active before external Stop now records a bounded relaunch
intent. The next transport command is classified explicitly:

```text
FC -> FA  Restart
    return SMF to MUSIC START
    use bounded three-block prefill
    do not require R
    do not wait for NEXT BAR

FC -> FB  Continue
    preserve the saved SMF tick
    use bounded three-block prefill

newly armed SMF
    retain ordinary Immediate / NEXT BAR launch policy
```

This distinction is implemented in:

```text
src/midi/smf_player_service.h
    SmfExternalRelaunchMode
    smfExternalRelaunchMode()

src/platform/cardputer_smf_player.{h,cpp}
    projectRelaunchAfterExternalStop_
    ARMED / RESTART
    ARMED / CONTINUE

tests/test_smf_external_transport_policy.cpp
    normal launch vs post-Stop Restart vs post-Stop Continue
```

The firmware cannot infer a preserved-position Continue from `FA`: MIDI defines
`FA` as Start, and SEQTRAK resets its own transport. Therefore physical
SEQTRAK Stop/Play restarts the SMF from MUSIC START. Preserved-position Continue
remains available only from a controller or test host that actually sends `FB`.

## Start, Stop and Continue

### Start (`FA`)

```text
new transport epoch
project phase resets to zero
MiniAcid and PatternPlayer restart
active stopped PROJECT SMF returns to MUSIC START
active stopped SMF relaunches after bounded three-block prefill
newly armed SMF uses its normal Immediate / NEXT BAR policy
```

### Stop (`FC`)

```text
new project NoteOn is blocked
active ownership receives scoped cleanup
MiniAcid and PatternPlayer pause
active PROJECT SMF preserves its current tick
```

### Continue (`FB`)

```text
MiniAcid and PatternPlayer resume preserved phase
previously active PROJECT SMF resumes its saved tick
```

The active PROJECT SMF uses a bounded three-block prefill, approximately 70 ms at 512 frames / 22.05 kHz. It does not wait for another full bar. A newly loaded or manually armed file still uses its ordinary NEXT BAR policy.

SEQTRAK itself does not transmit `FB`. Its physical Stop/Play sequence is
`FC -> FA`, so GroovePuter automatically restarts an SMF that was active before
Stop. No manual `R` press and no full-bar wait are required.

## Build / flash

Run:

```bash
./tests/run_host_tests.sh
```

Then run the repository's clean SDL and pinned Cardputer-Adv Arduino builds. Flash the resulting binary using the existing Cardputer procedure.

Record:

```text
host-tests
sdl-build
cardputer-adv-build
Flash bytes / percentage
DRAM globals / remaining
firmware SHA-256
```

## Expected behavior

### GP MASTER

- existing PR #22 behavior remains unchanged;
- `G` starts/stops GroovePuter;
- SEQTRAK receives Clock/Start/Stop;
- PROJECT SMF follows GroovePuter project phase.

### SEQ MASTER while stopped

```text
SEQ MASTER  WAIT / LOCKING / LOCKED
TRANSPORT   STOPPED
```

F8 may establish BPM lock, but GroovePuter does not run until SEQTRAK sends FA
while Follow is ON. A third-party controller may also start it with FB.

### SEQ MASTER Play

```text
SEQTRAK FA
  -> project phase zero
  -> internal audio and PatternPlayer start
  -> armed PROJECT SMF follows launch policy
  -> Cardputer musical MIDI continues
  -> no outbound realtime echo
```

### Stop / Play

```text
FC -> no new NoteOn; local and SMF position retained
FA -> local phase and active SMF restart after bounded prefill
```

Synthetic or third-party `FB` input still continues the preserved position.

### Reboot

The last selected master and Follow setting are restored before user interaction. Loading the record does not immediately rewrite NVS.

## Troubleshooting

### WAIT does not become LOCKED

- verify a data-capable cable;
- enable SEQTRAK MIDI Clock output;
- confirm the TinyUSB MIDI interface is mounted;
- inspect `externalRxClock`, queue drops and interval outliers.

### Play does not start GroovePuter

- check whether the screen says `FOLLOW OFF`;
- press `G` to enable Follow;
- press Play again so SEQTRAK emits a new FA;
- when using a different controller, a new FB is also accepted.

### Master choice is not restored

Inspect serial output:

```text
[MIDI-SETTINGS] load=...
[MIDI-SETTINGS] save=...
```

A missing or invalid record intentionally falls back to GP MASTER / FOLLOW ON.

### HOLD or LOST

- inspect USB power and cable;
- confirm Clock is still transmitted;
- inspect inbound critical-overflow diagnostics;
- reconnect, then press SEQTRAK Play to send a new FA.

### Duplicate Clock at SEQTRAK

Treat as failure. SEQ MASTER must suppress outbound F8/FA/FB/FC at both publication and physical-write boundaries.

### Devices feel rhythmically different

MIDI Clock does not carry swing. Match swing manually when both devices play dense sixteenth-note percussion.

## Acceptance checklist

```text
[ ] GP MASTER Clock/Start/Stop and PROJECT SMF are unchanged
[ ] C selects GP MASTER / SEQ MASTER
[ ] G toggles EXT FOLLOW in SEQ MASTER
[ ] F8 locks BPM while stopped without starting playback
[ ] first F8 after FA advances the first real pulse
[ ] FA starts MiniAcid and PatternPlayer at phase zero
[ ] FC prevents NoteOn after the Stop boundary
[ ] FC -> FA restarts active PROJECT SMF without manual R or NEXT BAR delay
[ ] synthetic FB continues preserved local phase
[ ] active PROJECT SMF relaunches without NEXT BAR delay
[ ] newly armed PROJECT SMF still uses NEXT BAR
[ ] Follow OFF ignores FA/FB but keeps BPM visible
[ ] Follow ON does not replay an ignored command
[ ] source switch stops the previous transport safely
[ ] SEQ MASTER emits no outbound F8/FA/FB/FC
[ ] PERFORM, PatternPlayer and SMF musical MIDI remain active
[ ] stable Clock does not reanchor SMF every block
[ ] 90 -> 110 and 120 -> 80 -> 140 cause no restart or burst
[ ] disconnect follows LOCKED -> HOLD -> LOST
[ ] reconnect does not auto-start
[ ] SEQ MASTER / Follow setting survives reboot
[ ] schema-v1 settings migrate to GP MASTER / FOLLOW ON
[ ] 32-bar recording shows no accumulating drift
[ ] no stuck notes, watchdog, reset or sustained underrun
[ ] host-tests green
[ ] SDL build green
[ ] Cardputer-Adv build green
```

## Cardputer-Adv hardware procedure

```text
1. Flash the current branch build.
2. Open MIDI PLAYER and select SEQ MASTER with C.
3. Keep SEQTRAK stopped; verify WAIT -> LOCKING -> LOCKED without audio start.
4. Press G for FOLLOW OFF; verify Play no longer starts GroovePuter.
5. Press G for FOLLOW ON; verify no automatic start occurs.
6. Send a new FA; verify internal audio and PatternPlayer start at phase zero.
7. Arm PROJECT SMF with Space; verify normal launch behavior.
8. Press SEQTRAK Stop immediately before a note; verify no NoteOn follows FC.
9. Press SEQTRAK Play; verify active SMF restarts without R or NEXT BAR wait.
10. Inject FB only with a controller/test host that can transmit Continue;
    verify preserved-position continuation.
11. Arm a different SMF after relaunch; verify it still waits for NEXT BAR.
12. Test 90 -> 110 and 120 -> 80 -> 140 BPM changes.
13. Verify no panic, realtime echo or catch-up burst.
14. Disconnect USB; verify HOLD -> LOST and no stuck notes.
15. Reconnect; require a new SEQTRAK Play/FA before transport runs.
16. Select SEQ MASTER and desired Follow state, reboot, and verify restoration.
17. Return to GP MASTER, reboot, and verify GP MASTER restoration.
18. Record at least 32 bars and compare beginning, middle and end.
19. Confirm tempoReanchor is event-driven, not one increment per block.
```
