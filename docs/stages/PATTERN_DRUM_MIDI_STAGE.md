# Pattern Drums -> SEQTRAK MIDI stage

## Purpose

Complete the external PatternPlayer path so GroovePuter patterns and Song rows can drive the native Yamaha SEQTRAK drum tracks without adding a second sequencer or a wall-clock MIDI scheduler.

```text
96 PPQN PatternPlayer
 -> actual internal drum trigger
 -> PatternPublishingDrumVoice
 -> MusicalEventQueue
 -> AudioTask block/frame timestamp
 -> ScheduledMusicalEventQueue
 -> MidiDispatchTask
 -> bounded 80 ms gate
 -> UsbMidiOutput
 -> TinyUSB
 -> SEQTRAK
```

`MidiDispatchTask` remains the only physical USB-MIDI owner.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3;
- Yamaha SEQTRAK;
- data-capable USB-C cable;
- optional Linux `aseqdump` monitor;
- existing GroovePuter Pattern/Song project.

## Wiring

```text
Cardputer-Adv USB-C -> Yamaha SEQTRAK USB-C
```

PORT.A is unchanged: GPIO2 SDA / GPIO1 SCL.

## Native routing

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

Mid Tom and Rim intentionally share PERC1/CH6 but remain distinct logical lanes.

## Timing

Pattern drum NoteOn timestamps use the same AudioTask block/frame phase as Pattern Synth A/B. NoteOff gates are derived from the original sample position.

The gate does not use `millis()`, `delay()`, `vTaskDelay()`, a new FreeRTOS task or a second sequencer.

The v1 gate is 80 ms. A retrigger/flam/roll extends the deadline to 80 ms after the newest hit. Repeated NoteOn packets are preserved. The bounded gate slot retains the number of logical retriggers and unwinds them only at the final extended deadline.

## Build / Flash

```bash
bash scripts/install_arduino_deps.sh
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Optional Linux inspection:

```bash
aconnect -l
aseqdump -l
aseqdump -p <client:port>
```

## Expected behavior

- internal drum audio remains unchanged;
- Pattern Kick/Snare/Clap/Hats/Perc reach native SEQTRAK channels;
- simultaneous hits remain independent;
- retrig/flam/roll preserve repeated NoteOn timing;
- final NoteOff follows the newest gate deadline;
- Mid Tom release cannot silence Rim on shared CH6/N60 and vice versa;
- Song continues to use PatternPlayer rather than a second Song MIDI renderer.

## Troubleshooting

- **Internal drums sound, external drums do not:** verify USB READY and inspect `aseqdump` before SEQTRAK sound selection.
- **Flam/roll collapses:** confirm multiple NoteOn packets; only NoteOff timing may be extended.
- **Retrigger cuts early:** an old gate deadline survived instead of being replaced.
- **Mid Tom/Rim cut each other:** inspect CH6/N60 reference ownership.
- **Stuck note after Stop/Song/scene change:** verify Pattern Drums generation/panic clears gate slots and wire ownership.
- **UI affects MIDI timing:** Pattern drums must remain on AudioTask timestamps and `MidiDispatchTask`, never UI loop dispatch.

## Acceptance checklist

- [ ] host source regression sees normalized PatternPlayer/Drums publication.
- [ ] eight internal drum voices have explicit Pattern lanes.
- [ ] native channel map is tested.
- [ ] Drums has separate generation/panic state.
- [ ] 80 ms block/frame conversion is tested.
- [ ] retrigger extends the gate and retains release count.
- [ ] gate state is fixed-size/allocation-free.
- [ ] no wall-clock gate scheduler exists.
- [ ] repeated NoteOn packets survive.
- [ ] Mid Tom/Rim shared ownership is tested.
- [ ] host-tests pass.
- [ ] SDL build passes.
- [ ] Cardputer-Adv build passes.
- [ ] firmware boots normally.
- [ ] internal audio remains unchanged.
- [ ] Kick reaches CH1.
- [ ] Snare reaches CH2.
- [ ] Clap reaches CH3.
- [ ] Closed Hat reaches CH4.
- [ ] Open Hat reaches CH5.
- [ ] Mid Tom reaches CH6.
- [ ] Rim reaches CH6 without cutting Mid Tom.
- [ ] High Tom reaches CH7.
- [ ] simultaneous Kick + Hat works.
- [ ] retrig/flam/roll remain recognizable.
- [ ] Pattern velocity survives.
- [ ] Stop/Panic/scene changes leave no stuck drum notes.
- [ ] Song 1B/2B/4B/8B transitions leave no stale gates.
- [ ] reconnect does not replay stale hits.
- [ ] no new audio underruns/watchdog/reset.
