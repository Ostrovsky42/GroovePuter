# SEQTRAK MASTER CLOCK STAGE

## Purpose

Provide two explicit transport roles while keeping Cardputer-Adv usable as a groovebox, MIDI instrument, PatternPlayer and SMF companion.

```text
GP MASTER
GroovePuter Clock / Start / Stop -> SEQTRAK

SEQ MASTER
SEQTRAK Clock / Start / Continue / Stop -> GroovePuter
```

Both modes continue sending PERFORM, PatternPlayer and SMF musical MIDI to SEQTRAK. `SEQ MASTER` suppresses only outbound system-realtime transport.

## Hardware list

- M5Stack Cardputer-Adv
- Yamaha SEQTRAK
- data-capable USB-C cable
- optional powered USB-C hub if USB power is unstable
- optional 3.5 mm cable: Cardputer audio out -> SEQTRAK AUDIO IN

## Wiring

```text
SEQTRAK USB-C <----------> Cardputer-Adv USB-C
  F8/FA/FB/FC  ---------> GroovePuter transport
  MIDI notes   <--------- PERFORM / PatternPlayer / SMF

Cardputer audio out ----> SEQTRAK AUDIO IN  (optional)
SEQTRAK audio out ------> headphones / speakers
```

PORT.A is not used. If unrelated I2C hardware is connected:

```text
SDA GPIO2
SCL GPIO1
3.3 V logic
```

## Controls

On MIDI Player:

```text
C       GP MASTER / SEQ MASTER
Space   SMF Arm / Pause
T       ORIGINAL / PROJECT
G       local Run / Stop in GP MASTER
        use SEQTRAK transport in SEQ MASTER
Up/Down GP MASTER BPM
        SEQ MASTER BPM is read-only
X       player-scoped panic
```

Clock-source selection is runtime-only and defaults safely to `GP MASTER` after reboot. Persistence is intentionally deferred until this hardware gate passes.

## Transport behavior

### GP MASTER

- Existing PR #22 behavior remains unchanged.
- AudioTask owns project phase and BPM.
- GroovePuter publishes MIDI Clock at 24 PPQN plus Start/Stop.
- PROJECT SMF follows the same project timeline.

### SEQ MASTER

- TinyUSB input is read only in the existing `MidiDispatchTask`.
- `0xF8` disciplines source BPM and project phase.
- `0xFA` starts a new session at phase zero.
- `0xFB` continues the preserved position.
- `0xFC` prevents new project NoteOn and preserves Continue position.
- GroovePuter does not echo `F8/FA/FB/FC` back to SEQTRAK.
- PERFORM, PatternPlayer, SMF notes and cleanup remain enabled.
- Source switching stops the current transport at an AudioTask boundary.

One F8 equals:

```text
1 / 24 quarter note
4 GroovePuter ticks at 96 PPQN
1 / 6 project sixteenth-step
```

MIDI Clock does not transmit swing. Match swing manually when both devices play dense sixteenth-note percussion.

## Realtime architecture

```text
TinyUSB RX
  |
  v
MidiDispatchTask
  |
  v
ExternalMidiTransportEventQueue
  |
  v
AudioTask block boundary
  |
  +-> ExternalMidiClockTracker
  +-> stable phase PLL
  +-> MiniAcid / PatternPlayer
  +-> ProjectTransportTimeline
          |
          +-> PROJECT SMF scheduler
```

Invariants:

- one mutable TinyUSB owner;
- no USB reads from UI, Arduino `loop()` or AudioTask;
- no direct engine mutation from `MidiDispatchTask`;
- no heap allocation in realtime paths;
- fixed 128-event SPSC queue;
- eight queue entries reserved for Start/Continue/Stop;
- critical overflow causes `LOST`, local stop and scoped cleanup;
- engine transport changes only on audio-block boundaries.

## Clock recovery

The tracker uses:

```text
5-sample median
1/8 EMA pulse-period filter
pulseOrdinal gap reconstruction
WAIT -> LOCKING -> LOCKED -> HOLD -> LOST
adaptive timeout for 5-300 BPM
```

Musical pulse position and accepted timing anchors are separate:

- every received F8 ordinal advances musical pulse position;
- an unusable timestamp cannot poison the median or timing anchor;
- later ordinal gaps recover elapsed timing;
- the first F8 after FA counts as the first real 1/24 pulse even without pre-lock.

F8 packets accumulated during USB or SD delay may arrive with almost identical timestamps. Only Clock packets within a 1 ms receive window are coalesced. Normal F8 intervals are at least about 8.3 ms at 300 BPM and remain distinct for initial lock.

## Stable phase PLL

The follower does not hard-seek sequencer phase and does not continuously vary `bpmQ16` every block.

Source BPM handling:

```text
source display:       measured SEQTRAK BPM
local base quantum:   0.1 BPM
base hysteresis:      0.15 BPM
```

Phase correction has three stable states:

```text
slow trim:   -2%
neutral:      0%
fast trim:   +2%
```

Hysteresis:

```text
enter trim:  |phase error| > 1/32 project step
leave trim:  |phase error| < 1/96 project step
maximum per-block correction: 1/96 project step
```

This matters for PROJECT SMF. The scheduler treats BPM revisions as tempo re-anchors. Stable trim states ensure constant SEQTRAK Clock does not create a re-anchor every audio block. Re-anchor is expected only during real tempo movement or a phase-trim state transition.

Normal re-anchor remains non-destructive: active-note ownership survives and only future deadlines are rebuilt.

At 512 frames / 22050 Hz, external transport application latency is at most one render block, approximately 23.2 ms.

## Start, Stop and Continue

### Start (`FA`)

```text
new transport epoch
project phase resets to zero
MiniAcid and PatternPlayer restart
an active stopped PROJECT SMF returns to MUSIC START
SMF uses normal Immediate / NEXT BAR launch policy
```

### Stop (`FC`)

```text
new project NoteOn stops
active ownership receives scoped cleanup
MiniAcid and PatternPlayer pause
active PROJECT SMF preserves its current tick
```

### Continue (`FB`)

```text
MiniAcid and PatternPlayer resume preserved phase
```

Only a PROJECT SMF that was actively playing before external Stop receives the Continue-resume path. It resumes its saved tick after a bounded three-block prefill, about 70 ms with the current audio block.

A newly loaded or manually armed SMF during a Continue session still waits for its ordinary NEXT BAR boundary.

## Build / flash

Run:

```bash
./tests/run_host_tests.sh
```

Then run the clean SDL build and pinned Cardputer-Adv Arduino build. Flash the generated binary with the repository's existing procedure.

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

### Stopped but receiving Clock

```text
CLOCK SEQ MASTER
STATE WAIT / LOCKING / LOCKED
TRANSPORT STOPPED
```

F8 may establish tempo lock, but GroovePuter does not run until FA or FB.

### Play

```text
SEQTRAK FA
  -> project phase zero
  -> internal audio and PatternPlayer start
  -> armed PROJECT SMF follows its launch policy
  -> Cardputer musical MIDI continues
  -> no outbound Clock echo
```

### Stop / Continue

```text
FC -> no new project NoteOn; position retained
FB -> local phase continues; previously active SMF resumes after prefill
```

### Tempo movement

Changing SEQTRAK tempo updates the filtered source BPM. The stable phase PLL may enter a temporary ±2% drive trim. There must be no transport restart, panic, hard seek or catch-up burst.

## Troubleshooting

### WAIT does not become LOCKED

- verify a data-capable cable;
- enable SEQTRAK MIDI Clock output;
- confirm the TinyUSB MIDI interface is mounted;
- inspect `externalRxClock`, queue drops and interval outliers.

### HOLD or LOST

- inspect USB power/cable;
- confirm Clock is still being transmitted;
- check critical inbound queue overflow;
- verify reconnect does not auto-start without a new FA or FB.

### Duplicate or unstable Clock at SEQTRAK

Treat as failure. `SEQ MASTER` must suppress outbound `F8/FA/FB/FC` at both publication and physical-write boundaries.

### Musical notes disappear

Transport suppression must not suppress PERFORM, PatternPlayer, SMF NoteOn/NoteOff or cleanup.

### Devices feel rhythmically different

MIDI Clock does not carry swing. Avoid duplicating dense hats unless swing settings are matched manually.

## Acceptance checklist

```text
[ ] GP MASTER remains the reboot default
[ ] GP MASTER Clock/Start/Stop behavior is unchanged
[ ] SEQ MASTER RX occurs only in MidiDispatchTask
[ ] no second TinyUSB owner or USB task exists
[ ] F8 before Start can lock tempo but cannot start playback
[ ] first F8 after FA advances the first 1/24 pulse without pre-lock
[ ] Start resets phase and creates a new epoch
[ ] Stop prevents new project NoteOn and preserves position
[ ] Continue does not restart at phase zero
[ ] previously active PROJECT SMF resumes after three-block prefill
[ ] newly armed PROJECT SMF still uses NEXT BAR
[ ] compressed buffered F8 preserves every pulse
[ ] a missing queued F8 is recovered from pulseOrdinal
[ ] ordinary USB jitter remains LOCKED
[ ] 5 BPM and 300 BPM lock correctly
[ ] Clock loss follows LOCKED -> HOLD -> LOST
[ ] LOST stops locally and performs scoped cleanup
[ ] SEQ MASTER emits no outbound F8/FA/FB/FC
[ ] PERFORM MIDI remains active
[ ] PatternPlayer MIDI remains active
[ ] SMF NoteOn/NoteOff remains active
[ ] ORIGINAL SMF remains independent
[ ] stable Clock does not increment tempoReanchor every block
[ ] trim-state changes are bounded and explainable
[ ] 32-bar recording shows no accumulating drift
[ ] no catch-up burst after USB/SD delay
[ ] no stuck notes after Stop, loss or reconnect
[ ] no watchdog/reset or sustained underrun
[ ] host-tests green
[ ] SDL build green
[ ] Cardputer-Adv build green
```

## Cardputer-Adv hardware procedure

```text
1. Flash the current branch build.
2. Open MIDI PLAYER; press C until SEQ MASTER appears.
3. Keep SEQTRAK stopped and confirm F8 reaches LOCKED without audio start.
4. Load PROJECT SMF and press Space; it remains ARMED.
5. Press SEQTRAK Play; verify FA starts internal audio and PatternPlayer at zero.
6. Confirm SMF enters at its expected launch boundary.
7. Press Stop 1-2 ms before an expected note; no NoteOn follows FC.
8. Press Continue; verify preserved local phase and bounded SMF resume.
9. Arm another SMF after Continue; it must wait for NEXT BAR.
10. Test 90 -> 110 and 120 -> 80 -> 140 BPM changes.
11. Confirm no restart, panic, realtime echo or burst.
12. Exercise dense MIDI and observe queue/backpressure diagnostics.
13. Disconnect USB; verify HOLD -> LOST and no stuck notes.
14. Reconnect; require a new FA/FB before transport runs.
15. Record at least 32 bars and compare beginning, middle and end.
16. Confirm tempoReanchor is event-driven, not one increment per block.
17. Return to GP MASTER with C and verify normal G-controlled transport.
```
