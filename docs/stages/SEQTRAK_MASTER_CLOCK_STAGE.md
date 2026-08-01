# SEQTRAK MASTER CLOCK STAGE

## Purpose

Add two explicit transport roles while preserving Cardputer-Adv as a groovebox, MIDI instrument, PatternPlayer and SMF companion.

```text
GP MASTER
GroovePuter transport -> MIDI Clock/Start/Stop -> SEQTRAK

SEQ MASTER
SEQTRAK MIDI Clock/Start/Continue/Stop -> GroovePuter transport
```

In both modes Cardputer-Adv continues sending musical MIDI events to SEQTRAK. `SEQ MASTER` suppresses only outbound transport realtime messages; PERFORM, PatternPlayer and SMF NoteOn/NoteOff remain enabled.

## Hardware list

- M5Stack Cardputer-Adv
- Yamaha SEQTRAK
- data-capable USB-C cable
- optional powered USB-C hub if power negotiation is unstable
- optional 3.5 mm cable from Cardputer audio output to SEQTRAK AUDIO IN

## Wiring

```text
SEQTRAK USB-C <----------> Cardputer-Adv USB-C
  F8/FA/FB/FC  ---------> external transport input
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

On the MIDI Player page:

```text
C       GP MASTER / SEQ MASTER
Space   SMF Arm / Pause
T       ORIGINAL / PROJECT
G       local Run / Stop in GP MASTER
        use SEQTRAK transport in SEQ MASTER
Up/Down BPM control in GP MASTER
        read-only source BPM in SEQ MASTER
X       player-scoped panic
```

The source selection is runtime-only and safely defaults to `GP MASTER` after reboot. Persistence is intentionally deferred until the hardware clock-follow gate is accepted.

## Transport contract

### GP MASTER

- Existing PR #22 behavior remains unchanged.
- `G` starts or stops GroovePuter.
- AudioTask publishes the project timeline and outbound MIDI Clock at 24 PPQN.
- PROJECT SMF follows GroovePuter phase and BPM.

### SEQ MASTER

- TinyUSB input is read only by the existing `MidiDispatchTask`.
- `0xF8` disciplines tempo and project phase.
- `0xFA` starts a new session from phase zero and creates a new epoch.
- `0xFB` continues the preserved position.
- `0xFC` stops new project NoteOn traffic and preserves position for Continue.
- Outbound `F8/FA/FB/FC` is suppressed both before queue publication and immediately before physical USB write.
- PERFORM, PatternPlayer, SMF notes and cleanup traffic remain enabled.
- Changing the master source safely stops the current transport at an AudioTask boundary.

One MIDI Clock pulse equals:

```text
1 / 24 quarter note
4 GroovePuter ticks at 96 PPQN
1 / 6 project sixteenth-step
```

MIDI Clock does not carry swing. Match swing manually when both devices play dense sixteenth-note percussion.

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
  +-> bounded phase PLL
  +-> MiniAcid / PatternPlayer
  +-> ProjectTransportTimeline
          |
          +-> PROJECT SMF scheduler
```

Constraints:

- `MidiDispatchTask` remains the only mutable TinyUSB owner.
- No USB reads occur in UI, Arduino `loop()` or AudioTask.
- No MiniAcid mutation occurs from `MidiDispatchTask`.
- No heap allocation occurs in the realtime path.
- The queue is fixed at 128 events with eight entries reserved for Start/Continue/Stop.
- Critical overflow moves the follower to `LOST` and requests a safe local stop.
- Transport mutation occurs only at an AudioTask block boundary.

## Clock recovery and phase lock

The tracker uses:

```text
five-sample median
1/8 EMA pulse-period filter
pulseOrdinal gap reconstruction
WAIT -> LOCKING -> LOCKED -> HOLD -> LOST
adaptive timeout for 5-300 BPM
```

Consecutive F8 packets accumulated during a USB or SD stall are coalesced to the latest timestamp and ordinal before tempo measurement. The ordinal gap preserves every musical pulse without treating nearly identical receive timestamps as zero-length clock intervals.

Musical pulse position and accepted timing anchors are independent:

- every valid F8 advances the external musical pulse count;
- an unusable timestamp cannot poison the median or accepted tempo anchor;
- later ordinal gaps recover timing across the rejected sample.

The follower does not hard-seek the local sequencer on every F8. It compares external absolute project steps with the previously published local timeline and applies a bounded tempo trim:

```text
phase proportional gain: 1/8
maximum drive-tempo trim: +/-5%
maximum correction:       1/96 project step per audio block
```

The UI continues to show the actual SEQTRAK source BPM. The corrected drive BPM is internal to the follower. This avoids repeated note triggers and audible phase jumps while removing long-term drift.

At 512 frames / 22050 Hz, Start/Continue/Stop application latency is at most one render block, approximately 23.2 ms.

## Start, Stop and Continue

### Start (`FA`)

```text
new transport epoch
project phase resets to zero
MiniAcid and PatternPlayer start from beginning
active PROJECT SMF returns to MUSIC START
SMF follows its normal Immediate / NEXT BAR launch policy
```

### Stop (`FC`)

```text
new project NoteOn stops
active ownership receives scoped cleanup
MiniAcid and PatternPlayer pause
PROJECT SMF preserves its current tick
```

### Continue (`FB`)

```text
MiniAcid and PatternPlayer continue preserved phase
```

Only a PROJECT SMF that was actively playing before external Stop uses the Continue resume path. It resumes from its preserved tick after a bounded three-audio-block prefill, approximately 70 ms at the current audio configuration.

A newly loaded or manually armed file during an existing Continue session still uses normal NEXT BAR quantization. This prevents a new phrase from entering immediately merely because the master session was resumed rather than started.

## Build and flash

Run:

```bash
./tests/run_host_tests.sh
```

Then run the repository's clean SDL build and pinned Cardputer-Adv Arduino build. Flash the produced binary using the existing repository procedure.

Record:

```text
host-tests result
sdl-build result
cardputer-adv-build result
Flash bytes and percentage
DRAM globals and remaining bytes
firmware SHA-256
```

## Expected behavior

### SEQTRAK stopped

```text
CLOCK SEQ MASTER
STATE WAIT / LOCKING / LOCKED
TRANSPORT STOPPED
```

F8 may establish tempo lock, but GroovePuter does not start until `FA` or `FB` arrives.

### SEQTRAK Play

```text
FA -> project phase zero
PatternPlayer and internal audio start
armed PROJECT SMF follows its launch policy
Cardputer continues transmitting musical MIDI notes
no outbound realtime echo
```

### SEQTRAK Stop / Continue

```text
FC -> no new project NoteOn; position preserved
FB -> local transport continues preserved phase
previously active PROJECT SMF resumes after bounded prefill
```

### Tempo change

Changing SEQTRAK tempo changes incoming F8 spacing. Source BPM converges through the median/EMA filter. The bounded PLL briefly trims local drive tempo without transport restart, panic, hard phase seek or catch-up burst.

## Serial diagnostics

Current transport diagnostics include source, lock state, running state, source BPM, received realtime counts, queue pressure/failure and outbound suppression. Runtime state also carries signed phase error and per-block phase correction for debugger/UI inspection.

Representative line:

```text
[MIDI-RX] source=SEQ MASTER state=LOCKED running=1 bpm=120.00
          rx=clock/start/continue/stop ignored=...
          queue=... clockDrop=... criticalOverflow=... failures=...
          txSuppressed=...
```

## Troubleshooting

### State remains WAIT

- Verify the cable carries data.
- Verify SEQTRAK MIDI Clock output is enabled.
- Confirm the TinyUSB MIDI interface is mounted.
- Inspect `externalRxClock`.

### State remains LOCKING

- Verify F8 intervals remain within 5-300 BPM.
- Inspect pulse gaps and interval outliers.
- Disable MIDI Thru loops.

### State changes to HOLD or LOST

- Check cable and USB power.
- Confirm SEQTRAK continues producing Clock when expected.
- Verify no critical inbound queue overflow occurred.

### SEQTRAK receives duplicate Clock

Treat this as a failure. In `SEQ MASTER`, GroovePuter must not transmit `F8/FA/FB/FC` regardless of SEQTRAK MIDI Thru settings.

### Notes disappear in SEQ MASTER

Transport suppression must not suppress musical traffic. Verify PERFORM, PatternPlayer and SMF NoteOn/NoteOff independently from realtime transport counters.

### Internal audio and SEQTRAK feel different

MIDI Clock does not transmit swing. Avoid duplicating dense hats on both devices unless swing settings are matched manually.

## Acceptance checklist

```text
[ ] GP MASTER is the safe default after reboot
[ ] GP MASTER Clock/Start/Stop behavior remains unchanged
[ ] SEQ MASTER receives F8/FA/FB/FC through MidiDispatchTask only
[ ] no second TinyUSB owner or USB task exists
[ ] F8 before Start can lock tempo but cannot start GroovePuter
[ ] Start resets phase and creates a new transport epoch
[ ] Stop prevents new project NoteOn and preserves position
[ ] Continue resumes without phase-zero restart
[ ] active PROJECT SMF resumes after bounded three-block prefill
[ ] newly armed PROJECT SMF still enters on NEXT BAR
[ ] buffered F8 packets preserve pulse count without BPM collapse
[ ] missing queued F8 is reconstructed from pulseOrdinal
[ ] normal USB jitter remains LOCKED
[ ] 5 BPM and 300 BPM endpoints lock correctly
[ ] Clock loss progresses LOCKED -> HOLD -> LOST
[ ] LOST performs safe local stop and scoped cleanup
[ ] SEQ MASTER emits no outbound F8/FA/FB/FC
[ ] SEQ MASTER continues PERFORM MIDI output
[ ] SEQ MASTER continues PatternPlayer MIDI output
[ ] SEQ MASTER continues SMF NoteOn/NoteOff output
[ ] PROJECT SMF follows the recovered project timeline
[ ] ORIGINAL SMF remains independent
[ ] bounded phase correction converges instead of accumulating drift
[ ] 32-bar recording shows no accumulating phase drift
[ ] no catch-up burst after USB or scheduling delay
[ ] no stuck notes after Stop, loss or reconnect
[ ] no watchdog/reset or sustained audio underrun
[ ] host-tests green
[ ] SDL build green
[ ] Cardputer-Adv build green
```

## Cardputer-Adv hardware pass

```text
1. Flash the current branch build.
2. Open MIDI PLAYER and press C until SEQ MASTER is visible.
3. With SEQTRAK stopped, confirm F8 reaches LOCKED without starting audio.
4. Load a PROJECT MIDI and press Space; it remains ARMED.
5. Press Play on SEQTRAK; FA starts internal audio and PatternPlayer from zero.
6. Verify the armed SMF follows its normal launch boundary.
7. Press Stop immediately before an expected note; no new NoteOn follows FC.
8. Press Continue; local phase and the previously active SMF resume, not bar one.
9. Arm a different SMF after Continue; it must still wait for NEXT BAR.
10. Change SEQTRAK 90 -> 110 BPM and then 120 -> 80 -> 140 BPM.
11. Confirm no restart, panic, Clock echo or note burst.
12. Exercise dense MIDI while observing queue drops and USB backpressure metrics.
13. Disconnect the cable; verify LOCKED -> HOLD -> LOST and no stuck notes.
14. Reconnect; transport must not start until a new FA or FB.
15. Record at least 32 bars and compare beginning, middle and end against SEQTRAK drums/metronome.
16. Return to GP MASTER with C and verify normal G-controlled transport.
```
