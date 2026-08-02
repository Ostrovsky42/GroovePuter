# SMF TRACK MUTE STAGE

## Purpose

Allow individual Standard MIDI File tracks to be muted while the realtime player continues running.

This is intended for reducing a full arrangement to useful material for Yamaha SEQTRAK, for example keeping one melody or bass track while removing doubled accompaniment.

The implementation uses the existing `SmfStreamEvent::trackIndex` and a fixed 64-bit mask. It does not load the file into RAM and does not create another scheduler or USB owner.

## Hardware list

- M5Stack Cardputer-Adv
- microSD card with one multi-track `.mid` file under `/midi`
- optional Yamaha SEQTRAK
- data-capable USB-C cable when testing external MIDI

## Wiring

For SEQTRAK testing:

```text
Cardputer-Adv USB-C
        |
        | USB MIDI
        v
Yamaha SEQTRAK USB
```

PORT.A is not used by this test. If unrelated I2C hardware is attached, Cardputer-Adv PORT.A remains:

```text
SDA GPIO2
SCL GPIO1
```

## Build / flash

Run repository host regressions:

```bash
./tests/run_host_tests.sh
```

Then use the repository's existing pinned Cardputer-Adv Arduino build and upload process. Do not change the M5Stack core version for this test.

## Controls

On the MIDI Player now-playing screen:

```text
J         previous SMF track
L         next SMF track
K         mute / unmute selected track
Shift+K   clear all track mutes
```

Track numbers are one-based in the UI and zero-based internally.

## Expected behavior

- Selecting a track does not pause, seek or restart playback.
- Muting suppresses future `NoteOn` events from the selected SMF track.
- `NoteOff` events remain enabled so notes already queued or playing can be released safely.
- Existing events already inside scheduler lookahead may still sound briefly after pressing `K`.
- Loading a different MIDI resets all track mutes.
- Track mute works in both RAW and SEQTRAK routing modes.
- PROJECT playback remains on the existing transport phase and is not re-armed by mute.

## Troubleshooting

### Muted track sounds briefly after pressing K

This is expected for the first bounded implementation. Events already scheduled ahead are not flushed because flushing would interrupt unrelated tracks and could disturb PROJECT timing.

### Muting seems to do nothing

Some Format-0 MIDI files contain the entire arrangement in one SMF track, even when they use many MIDI channels. Track mute operates on SMF tracks, not channels. Such a file will show only one selectable track.

### A note hangs after mute

Treat this as a failure. Press `X` for player panic and record the MIDI file, selected track number, routing mode and whether the note began before or after mute.

### The wrong musical part disappears

Track names are not parsed in this stage. Use `J/L` while listening to identify tracks by ear. Track-name metadata belongs to the later phrase/track browser stage.

## Acceptance checklist

```text
[ ] normal boot succeeds
[ ] MIDI Player loads a multi-track file
[ ] J/L wraps safely through all tracks
[ ] K marks selected track MUTE
[ ] future NoteOn from muted track stops
[ ] existing notes receive NoteOff
[ ] Shift+K restores every track
[ ] loading another file clears mute mask
[ ] RAW routing respects track mute
[ ] SEQTRAK routing respects track mute
[ ] PROJECT playback does not restart or re-arm on mute
[ ] ORIGINAL playback does not seek on mute
[ ] Stop/Panic leaves no stuck notes
[ ] no watchdog/reset
[ ] host-tests green
[ ] SDL build green
[ ] Cardputer-Adv build green
```
