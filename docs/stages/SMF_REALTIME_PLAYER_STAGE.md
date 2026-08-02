# Realtime SMF Player Stage

## Purpose

Add a faithful Standard MIDI File player alongside the existing destructive `MidiImporter`.

The product contract is:

```text
PROJECT -> MIDI browser -> selected .mid
                          |- PLAY
                          |- IMPORT TO GROOVEBOX
                          `- INFO / ROUTING
```

`PLAY` must not rewrite GroovePuter patterns or scenes. `IMPORT TO GROOVEBOX` keeps the existing quantized importer workflow.

The accepted design remains one USB owner:

```text
SMF timeline -------------------\
PatternPlayer -> scheduled notes +--> MidiDispatchTask -> TinyUSB
transport sync -> F8/FA/FC -----/
```

No UI, parser, DSP callback, or Arduino `loop()` may write TinyUSB directly.

## Hardware list

- M5Stack Cardputer-Adv / ESP32-S3;
- microSD card containing `.mid` files under `/midi`;
- data-capable USB-C cable;
- Yamaha SEQTRAK or another class-compliant USB MIDI receiver;
- optional Linux MIDI monitor for timestamp capture.

## Wiring

Cardputer-Adv USB-C data connection -> Yamaha SEQTRAK USB MIDI path.

PORT.A is not used by this test. GPIO2 SDA / GPIO1 SCL, `Wire`, Scroll Unit and display initialization order are unchanged.

## UX contract

Selecting a MIDI file keeps the existing actions:

```text
PLAY
IMPORT TO GROOVEBOX
INFO / ROUTING
```

The existing `/midi` browser and file scan are reused; do not add a second browser.

### Directory browser memory contract

Cardputer-Adv has no PSRAM, and a loaded SMF intentionally keeps its stream,
queues and file state alive while playback is paused. The browser therefore
must not retain every directory entry as a `std::string`.

The browser uses three bounded operations:

```text
refresh / enter directory
    -> scan the complete directory and count folders plus .mid files

draw / scroll
    -> scan only far enough to fill a fixed seven-row display window

Enter
    -> resolve the selected full filename with one bounded rescan
```

Only truncated display labels for seven rows remain resident. The full name is
resolved again before loading, so display truncation cannot select a different
file. Directories remain before MIDI files; FAT directory order is preserved
within each group to avoid a heap-backed sort.

The diagnostic contract is:

```text
[SMF-BROWSE] root.open ok=1 isDir=1 ...
[SMF-BROWSE] scanned=... dirs=... files=... complete=1 ...
```

`complete=1` means the entire directory was counted. The browser must never
stop because free heap crossed a threshold and then present the partial result
as a complete folder.

## Playback model

### Tempo

- `ORIGINAL`: follow the SMF tempo map exactly;
- `PROJECT`: preserve ticks, durations and tuplets while scaling the timeline to GroovePuter BPM.

`PROJECT` is retiming, not quantization.

### Routing

- `RAW`: preserve source channels;
- `SEQTRAK`: map musical roles to the SEQTRAK profile;
- `CUSTOM`: later per-track destination editing.

SEQTRAK profile:

```text
CH1  KICK
CH2  SNARE
CH3  CLAP
CH4  HAT1
CH5  HAT2
CH6  PERC1
CH7  PERC2
CH8  SYNTH1
CH9  SYNTH2
CH10 DX
CH11 SAMPLER
```

The instrument destinations are intentionally distinct. DX is a dedicated FM track and must not be used as the generic fallback for unrelated melodic or texture material. SAMPLER is a separate playable destination. Track FX, Delay/Reverb sends and Master FX belong to a future control/CC domain and are not NoteOn targets.

The deterministic v1 SEQTRAK melodic mapping is:

```text
source CH1  -> SYNTH1  / CH8
source CH2  -> SYNTH2  / CH9
source CH3  -> DX      / CH10
source CH4+ -> SAMPLER / CH11
```

A GM drum track on source CH10 is split by drum note into SEQTRAK drum tracks 1..7.

## Build / Flash

```bash
bash tests/run_host_tests.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

## Expected behavior

- SMF Type 0/1 PPQN playback remains bounds checked.
- NoteOn/NoteOff durations and simultaneous notes survive playback.
- tempo/time-signature maps remain intact.
- seek/restart/stop invalidate stale scheduled events.
- SEQTRAK routing keeps SYNTH1/SYNTH2/DX/SAMPLER distinct.
- extra melodic source channels do not alias to DX.
- GM drums split onto native SEQTRAK CH1..7.

## Troubleshooting

- **MIDI sounds quantized:** that is the destructive `MidiImporter`; realtime PLAY is a separate path.
- **Stuck notes after seek/restart:** player ownership must be released before generation invalidation changes position.
- **Too many unrelated parts play on DX:** only source CH3 belongs to DX/CH10 in fixed SEQTRAK mode; CH4+ uses SAMPLER/CH11 until custom routing exists.
- **Folder becomes empty or shows only its first files after playback:** inspect
`[SMF-BROWSE]`. `exists=1`, `root.open ok=1` and `complete=1` mean SD is still
mounted and the complete directory was counted. Older builds stopped scanning
at a low-memory guard while retaining filenames, so a pause/error made the
same truncation reproducible; it was not an SD disconnect.
- **Screen shows `SD UNAVAILABLE`:** the directory could not be opened even
  after the shared platform mount retry. This is a real storage-path failure;
  reseat the card and press `R`. Do not call `SD.begin()` from the page: SD pin
  ownership belongs to `src/platform/cardputer_sd.cpp`.
  `[SMF-BROWSE] window.open failed` means the count pass succeeded but the
  bounded seven-row window could not reopen the same directory.

## Acceptance checklist

- [ ] RAW routing is unchanged.
- [ ] source CH1 -> CH8 SYNTH1.
- [ ] source CH2 -> CH9 SYNTH2.
- [ ] source CH3 -> CH10 DX.
- [ ] source CH4+ -> CH11 SAMPLER.
- [ ] GM source CH10 drums split across native CH1..7.
- [ ] original note lengths remain recognizable.
- [ ] chords remain polyphonic.
- [ ] triplets are not flattened to 1/16.
- [ ] tempo changes reproduce without restart.
- [ ] seek/restart/stop leave no stuck notes.
- [ ] Cardputer-Adv -> SEQTRAK timing is stable under UI navigation.
- [ ] no internal audio underrun/watchdog regression.
- [ ] a large `/midi` folder reports `complete=1` and all entries remain reachable after Play, Pause, USB WAIT and Error.
- [ ] a failed directory open displays `SD UNAVAILABLE`, not `NO MIDI FILES`.
