# Pattern Drums -> SEQTRAK MIDI stage

## Purpose

Complete the external PatternPlayer path so GroovePuter patterns and Song rows can drive the native Yamaha SEQTRAK drum tracks without adding a second sequencer or a wall-clock MIDI scheduler.

The accepted event path is:

```text
96 PPQN MiniAcid PatternPlayer
        |
        +--> existing internal drum engine
        |
        `--> actual DrumSynthVoice trigger
                 |
                 v
       PatternPublishingDrumVoice
                 |
                 v
          MusicalEventQueue
      source = PatternPlayer
      target = Drums
      channel = internal voice 0..7
      note = 60
      velocity = actual hit velocity
                 |
        AudioTask block/frame timestamp
                 |
                 v
      ScheduledMusicalEventQueue
                 |
                 v
          MidiDispatchTask
        |                  |
        | NoteOn           `-- bounded 80 ms gate state
        |                         |
        |                         `--> sample-timed NoteOff
        v
      UsbMidiOutput
                 |
                 v
              TinyUSB
                 |
                 v
             SEQTRAK
```

`MidiDispatchTask` remains the only physical USB-MIDI owner.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3;
- Yamaha SEQTRAK;
- data-capable USB-C connection used by the accepted USB-MIDI path;
- optional Linux computer and `aseqdump` for packet inspection before direct SEQTRAK testing;
- existing GroovePuter SD/project setup for Pattern and Song tests.

## Wiring

Primary hardware test:

```text
Cardputer-Adv USB-C
        |
        v
Yamaha SEQTRAK USB-C
```

No PORT.A wiring is changed by this stage.

Cardputer-Adv PORT.A remains:

```text
SDA GPIO2
SCL GPIO1
```

Existing I2C, display and audio wiring are unchanged.

## Native routing

GroovePuter currently has eight sequenced internal drum voices while the SEQTRAK-native companion profile exposes seven drum tracks.

```text
Internal voice   SEQTRAK role   MIDI channel   Note
----------------------------------------------------
Kick             KICK           CH1            60
Snare            SNARE          CH2            60
Closed Hat       HAT1           CH4            60
Open Hat         HAT2           CH5            60
Mid Tom          PERC1          CH6            60
High Tom         PERC2          CH7            60
Rim              PERC1          CH6            60
Clap             CLAP           CH3            60
```

Mid Tom and Rim intentionally share the physical PERC1 destination. They remain separate logical Pattern lanes so ownership cleanup cannot silence one while the other is active.

## Timing contract

Pattern drum NoteOn timestamps come from the same AudioTask render phase already used by Pattern Synth A/B:

```text
AudioTask block sequence + exact frame offset
```

The NoteOff gate is derived from that sample position. It is not based on:

- `millis()`;
- `micros()` as a gate clock;
- `delay()`;
- `vTaskDelay()`;
- a new FreeRTOS task;
- a second sequencer.

The v1 gate uses the existing companion default:

```text
80 ms
```

A retrigger, flam or roll extends the logical voice's gate to 80 ms after the newest hit. Repeated NoteOn packets are still sent; older gate deadlines are replaced so they cannot cut a newer retrigger short.

## Ownership contract

The dispatcher keeps eight bounded logical gate slots. `UsbMidiOutput` keeps reference-counted physical channel+note ownership.

This matters for both:

```text
same voice retrig
Kick ON
Kick ON
... final gate ...
Kick OFF
```

and shared destinations:

```text
MidTom -> CH6 N60
Rim    -> CH6 N60
```

Releasing MidTom must not emit the final physical CH6/N60 NoteOff while Rim is still an owner.

Pattern Drums has its own generation and panic mask. Stop, scene changes, Song lifecycle changes, queue overflow and other Pattern cleanup invalidate stale drum events without aliasing the cleanup to Synth A or Synth B.

## Build / Flash

Use the repository-pinned Arduino/M5Stack environment:

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Optional Linux MIDI inspection:

```bash
aconnect -l
aseqdump -l
aseqdump -p <client:port>
```

## Expected behavior

### Basic Pattern routing

Create or select a Pattern with isolated drum steps. Expected external messages:

```text
Kick       -> CH1 N60
Snare      -> CH2 N60
Clap       -> CH3 N60
Closed Hat -> CH4 N60
Open Hat   -> CH5 N60
Mid Tom    -> CH6 N60
Rim        -> CH6 N60
High Tom   -> CH7 N60
```

Internal GroovePuter drums must remain audible exactly as before.

### Simultaneous hits

Patterns such as:

```text
Kick + Closed Hat
Snare + Closed Hat
Kick + Clap + Open Hat
```

must emit independent NoteOn messages at the same sample-timed position. One drum lane must not collapse another.

### Retrig / flam / roll

Fast repeated internal trigger calls must remain repeated external NoteOn messages with their existing sample spacing. The final NoteOff occurs after the newest hit's gate, not after the first hit's gate.

### Song

Song row changes continue to select the same internal patterns. There is no Song MIDI renderer. External drums follow the active PatternPlayer exactly as Synth A/B already do.

Expected:

```text
Song
 -> Pattern selection
 -> PatternPlayer A/B/Drums
 -> one sample-timed MIDI dispatcher
 -> SEQTRAK
```

## Troubleshooting

### Internal drums sound, but SEQTRAK drums do not

Check:

- USB MIDI status is READY;
- PatternPlayer MIDI output is enabled;
- the SEQTRAK project accepts native CH1..7 drum input;
- `aseqdump` shows PatternPlayer NoteOn packets before debugging SEQTRAK sound selection.

### One drum cuts another off

For Mid Tom + Rim specifically, inspect CH6/N60 ownership. Both logical lanes share the same physical destination. Only the final logical owner may emit the physical NoteOff.

### Flam or roll becomes one hit

Confirm multiple NoteOn packets are present. The gate scheduler must extend only NoteOff; it must never collapse repeated NoteOn events.

### A retrigger is cut approximately 80 ms after the first hit

That indicates a stale gate deadline survived a retrigger. The gate slot must be replaced by the newest hit deadline.

### Stuck drum after Stop / scene change / Song transition

Check the Pattern Drums generation/panic path. A Pattern lifecycle barrier must clear pending drum gates and release Pattern Drums ownership.

### MIDI timing changes when navigating UI

This stage must not dispatch from the UI loop. Inspect `MidiDispatchTask` diagnostics and verify that all Pattern drum events retain block/frame timestamps from AudioTask.

## Acceptance checklist

### Host / CI

- [ ] source regression confirms Pattern drum triggers publish through `MusicalEventQueue`.
- [ ] all eight internal voices have explicit Pattern MIDI lanes.
- [ ] native channel map is fixed and tested.
- [ ] Pattern Drums has a separate generation and panic mask.
- [ ] 80 ms gate conversion is tested in block/frame coordinates.
- [ ] retrigger replaces the old deadline and retains the required release count.
- [ ] gate scheduler is fixed-size and allocation-free.
- [ ] gate scheduler contains no `millis()`, `delay()` or FreeRTOS wait.
- [ ] repeated Pattern drum NoteOn remains repeated on USB output.
- [ ] shared CH6/N60 ownership for Mid Tom/Rim is tested.
- [ ] host tests pass.
- [ ] SDL build passes.
- [ ] Cardputer-Adv build passes.

### Hardware / serial

- [ ] firmware boots normally with no watchdog/reset.
- [ ] internal GroovePuter drum audio is unchanged.
- [ ] USB MIDI reaches READY.
- [ ] Kick reaches CH1.
- [ ] Snare reaches CH2.
- [ ] Clap reaches CH3.
- [ ] Closed Hat reaches CH4.
- [ ] Open Hat reaches CH5.
- [ ] Mid Tom reaches CH6.
- [ ] Rim reaches CH6 without cutting an active Mid Tom.
- [ ] High Tom reaches CH7.
- [ ] simultaneous Kick + Hat is audible externally.
- [ ] simultaneous Snare + Hat is audible externally.
- [ ] retrig produces repeated hits, not one collapsed hit.
- [ ] flam timing remains recognizable.
- [ ] roll timing remains recognizable.
- [ ] Pattern velocity differences remain audible / visible in MIDI capture.
- [ ] Stop leaves no active Pattern drum notes.
- [ ] Panic leaves no active Pattern drum notes.
- [ ] scene change leaves no stale drum notes.
- [ ] Song 1B/2B/4B/8B row transitions do not leave stale gates.
- [ ] reconnect does not replay stale Pattern drum hits.
- [ ] no new audio underruns are observed during dense drum patterns.
- [ ] UI navigation does not audibly change Pattern drum MIDI timing.
