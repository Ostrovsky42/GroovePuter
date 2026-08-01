# SEQTRAK MASTER CLOCK STAGE

## Purpose

Add a second explicit transport-clock source without changing GroovePuter's role as a groovebox, MIDI instrument, PatternPlayer and SMF companion.

```text
GP MASTER
GroovePuter transport -> MIDI Clock/Start/Stop -> SEQTRAK

SEQ MASTER
SEQTRAK MIDI Clock/Start/Continue/Stop -> GroovePuter transport
```

In both modes Cardputer-Adv continues to send musical MIDI events to SEQTRAK. `SEQ MASTER` suppresses only outbound transport realtime messages; it must not suppress PatternPlayer, PERFORM or SMF NoteOn/NoteOff traffic.

## Hardware list

- M5Stack Cardputer-Adv
- Yamaha SEQTRAK
- data-capable USB-C cable
- optional powered USB-C hub if power negotiation is unstable
- optional 3.5 mm cable from Cardputer audio output to SEQTRAK AUDIO IN for GroovePuter internal synth audio

## Wiring

```text
SEQTRAK USB-C <----------> Cardputer-Adv USB-C
  F8/FA/FB/FC  ---------> external transport input
  MIDI notes   <--------- PERFORM / PatternPlayer / SMF

Cardputer audio out ----> SEQTRAK AUDIO IN  (optional)
SEQTRAK audio out ------> headphones / speakers
```

PORT.A is not used by this stage. If unrelated I2C hardware is connected, Cardputer-Adv PORT.A remains:

```text
SDA GPIO2
SCL GPIO1
3.3 V logic
```

## Transport contract

### GP MASTER

- `G` starts or stops GroovePuter.
- GroovePuter publishes MIDI Clock at 24 PPQN.
- GroovePuter publishes Start/Stop.
- PROJECT SMF follows GroovePuter's AudioTask timeline.
- Existing PR #22 behavior remains unchanged.

### SEQ MASTER

- Incoming `0xF8` measures tempo and project phase.
- Incoming `0xFA` starts from project phase zero and creates a new transport epoch.
- Incoming `0xFB` continues from the preserved position.
- Incoming `0xFC` stops new NoteOn traffic and preserves position for Continue.
- GroovePuter does not echo `F8/FA/FB/FC` back to SEQTRAK.
- PatternPlayer, PERFORM, PROJECT SMF and internal audio follow the same recovered project timeline.

One MIDI Clock pulse equals:

```text
1 / 24 quarter note
4 GroovePuter ticks at 96 PPQN
1 / 6 project sixteenth-step
```

Swing is not transmitted by MIDI Clock. Match swing manually when both devices play dense sixteenth-note percussion.

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
  +-> MiniAcid transport
  +-> PatternPlayer
  +-> ProjectTransportTimeline
          |
          +-> PROJECT SMF scheduler
```

Constraints:

- `MidiDispatchTask` remains the only mutable TinyUSB owner.
- No USB reads in UI, Arduino `loop()` or AudioTask.
- No direct MiniAcid mutation from `MidiDispatchTask`.
- No heap allocation in the realtime path.
- Start/Continue/Stop have reserved queue capacity.
- A critical queue overflow forces a safe external-clock failure state.

## Current foundation

The first implementation slice provides:

- `TransportClockSource` with `GP MASTER` and `SEQ MASTER`;
- outbound-clock policy helper;
- compact 16-byte external transport event;
- 128-slot SPSC transport queue with eight critical-reserve entries;
- external Clock tracker with a five-sample median and 1/8 EMA;
- pulse-ordinal gap handling;
- `WAIT`, `LOCKING`, `LOCKED`, `HOLD`, `LOST` states;
- adaptive Hold/Lost timeout;
- deterministic host regressions.

USB RX, AudioTask application, persistence and UI wiring are completed in subsequent commits in the same draft PR.

## Build and validation

Run host regressions:

```bash
./tests/run_host_tests.sh
```

Then run the repository's existing clean SDL build and pinned Cardputer-Adv Arduino build.

Record:

```text
host-tests result
sdl-build result
cardputer-adv-build result
Flash bytes and percentage
DRAM globals and remaining bytes
```

## Expected behavior

### Clock stopped

```text
CLOCK SEQ MASTER
STATE LOCKED or WAIT
TRANSPORT STOPPED
```

F8 may establish tempo lock, but GroovePuter does not start until Start or Continue arrives and external follow is enabled.

### Start

```text
SEQTRAK Play
  -> FA
  -> GroovePuter starts from phase zero
  -> PatternPlayer starts from phase zero
  -> PROJECT SMF enters by Immediate or Next Bar policy
```

### Continue

```text
SEQTRAK Continue
  -> FB
  -> GroovePuter resumes preserved phase
```

### Stop

```text
SEQTRAK Stop
  -> FC
  -> no new project NoteOn after the stop boundary
  -> active owners receive scoped cleanup
  -> position remains available for Continue
```

### Tempo change

Changing SEQTRAK tempo changes the spacing of incoming F8 pulses. The tracker filters USB scheduling jitter and updates GroovePuter without restarting transport or producing catch-up bursts.

## Troubleshooting

### State remains WAIT

- Verify the USB cable carries data.
- Verify SEQTRAK MIDI Clock output is enabled.
- Confirm the TinyUSB MIDI interface is mounted.
- Inspect `externalRxClock` diagnostics.

### State remains LOCKING

- Verify F8 intervals are inside the supported 5-300 BPM range.
- Check `externalIntervalOutliers` and `externalPulseGaps`.
- Disable MIDI Thru loops.

### State changes to HOLD or LOST

- Check cable and USB power.
- Confirm SEQTRAK continues sending Clock while stopped if pre-lock is expected.
- Inspect the filtered pulse period and adaptive timeout diagnostics.

### SEQTRAK receives duplicate or unstable Clock

Treat this as a failure. In `SEQ MASTER`, GroovePuter must suppress outbound `F8/FA/FB/FC` regardless of SEQTRAK MIDI Thru settings.

### Notes stop in SEQ MASTER

Transport suppression must not suppress musical queues. Verify PatternPlayer, PERFORM and SMF NoteOn/NoteOff counters independently from realtime transport counters.

### Internal audio and SEQTRAK sound feel different

MIDI Clock does not carry swing. Avoid duplicating dense hats on both devices unless their swing settings are matched manually.

## Acceptance checklist

```text
[ ] GP MASTER remains the default after upgrade
[ ] invalid persisted source normalizes to GP MASTER
[ ] GP MASTER outbound Clock/Start/Stop remains unchanged
[ ] SEQ MASTER receives F8/FA/FB/FC through MidiDispatchTask only
[ ] no second TinyUSB owner or USB task exists
[ ] F8 before Start may lock tempo but does not start GroovePuter
[ ] Start resets project phase and creates a new epoch
[ ] Stop blocks new NoteOn and preserves Continue position
[ ] Continue resumes without phase-zero restart
[ ] one missing queued F8 is reconstructed from pulseOrdinal
[ ] normal USB jitter remains LOCKED
[ ] 5 BPM and 300 BPM endpoints lock correctly
[ ] Clock loss progresses LOCKED -> HOLD -> LOST
[ ] LOST performs safe local stop and scoped cleanup
[ ] SEQ MASTER emits no outbound F8/FA/FB/FC
[ ] SEQ MASTER continues PERFORM MIDI output
[ ] SEQ MASTER continues PatternPlayer MIDI output
[ ] SEQ MASTER continues SMF NoteOn/NoteOff output
[ ] PROJECT SMF follows recovered project timeline
[ ] ORIGINAL SMF remains independent
[ ] Cardputer internal audio remains available through AUDIO IN
[ ] 32-bar recording shows no accumulating drift
[ ] no catch-up burst after USB or scheduling delay
[ ] no stuck notes after Stop, loss or reconnect
[ ] no watchdog/reset or sustained audio underrun
[ ] host-tests green
[ ] SDL build green
[ ] Cardputer-Adv build green
```
