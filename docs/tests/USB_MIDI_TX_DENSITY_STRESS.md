# USB MIDI TX density stress

## Purpose

Measure the practical USB-MIDI transmit throughput and the point where a host
that remains configured but stops draining its MIDI IN endpoint begins rejecting
packets.

The diagnostic emits alternating Note On and Note Off messages for MIDI note 60
on channel 16. It uses a dedicated bounded producer queue. `MidiDispatchTask`
remains the sole USB writer, and the existing physical packet pacing is unchanged.

## Hardware list

- M5Stack Cardputer ADV
- USB-C data cable
- USB host with a class-compliant MIDI receiver or monitor
- Optional host tool that can keep the MIDI port open while its reader is paused

## Wiring

No external wiring is required.

Connect the Cardputer ADV native USB-C port directly to the host. PORT.A is
unused. The built-in display is used for counters.

## Build and flash

```bash
python3 tests/test_usb_midi_source_regressions.py
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/upload.sh /dev/ttyACM0
```

Open the PROJECT page, then press `Ctrl+Alt+U` to enter the hidden
`USB TX STRESS` screen.

Controls:

```text
Space / Enter   start or stop the stream
Left / [        lower rate
Right / ]       raise rate
R               reset counters
Backspace / `   stop and exit
```

Available rates are 50, 100, 200, 400 and 800 messages per second. Changing the
rate resets all counters and starts a new stall-free interval when the stream is
running.

## Expected behavior

### Screen

The screen shows:

```text
RATE      selected messages per second
OK        packets accepted by the physical USB transport
REJECT    queue overflows plus failed physical writes
EP=BUSY   failed physical writes while the MIDI interface remains mounted
STABLE    seconds from stream start to the first EP=BUSY event
CH16      fixed diagnostic channel
NOTE60    fixed diagnostic note
```

`STABLE` continues increasing while no mounted-endpoint rejection has occurred.
It freezes at the first `EP=BUSY` event so rates can be compared. Stopping the
stream preserves the displayed counters until Reset or a rate change.

### Host throughput test

1. Open the GroovePuter MIDI port in a host monitor.
2. Start at 50 messages per second and verify balanced Note On/Off traffic.
3. Increase through each rate and record `OK`, `REJECT`, `EP=BUSY`, and `STABLE`.
4. Keep the host port configured but pause or suspend the reader so the endpoint
   stops draining.
5. Verify `EP=BUSY` becomes non-zero and `STABLE` freezes.
6. Resume the reader, stop the generator, reset, and repeat at another rate.

On Linux, a practical setup is to start `aseqdump` for the GroovePuter port and
pause that process with `SIGSTOP`; the USB/ALSA buffers may take time to fill
before endpoint backpressure reaches the device.

## Troubleshooting

### Screen does not open

Use the PROJECT page and press all three keys in `Ctrl+Alt+U`. The diagnostic is
intentionally hidden and is not part of normal workflow navigation.

### OK remains zero

Confirm that the host enumerated the USB MIDI interface and opened the
GroovePuter MIDI port. A CDC serial connection alone does not prove that the
MIDI interface is mounted.

### REJECT rises but EP=BUSY stays zero

The bounded diagnostic queue overflowed, or the MIDI interface was not mounted.
Lower the selected rate and confirm the host MIDI port is open.

### Host stops reading but no stall appears

Some operating systems continue draining USB into a large kernel MIDI buffer
even when the user application pauses. Leave the reader paused longer or use a
host setup that stops consuming the class endpoint while keeping it configured.

### A note remains audible on the host

Stop or exit the diagnostic. The dispatcher sends an uncounted Note Off for
channel 16, note 60 whenever the stream stops, resets, or changes rate.

## Acceptance checklist

- [ ] `python3 tests/test_usb_midi_source_regressions.py` passes.
- [ ] `bash tests/run_host_tests.sh` passes.
- [ ] Cardputer ADV firmware builds with `--warnings all`.
- [ ] `Ctrl+Alt+U` opens the hidden diagnostic from PROJECT.
- [ ] The stream is inactive after boot and normal playback behavior is unchanged.
- [ ] Space starts and stops alternating Note On/Off traffic on CH16 note 60.
- [ ] Left/right and brackets select 50, 100, 200, 400 and 800 messages per second.
- [ ] Changing rate resets OK, REJECT, EP=BUSY and STABLE.
- [ ] Stopping preserves counters for inspection until Reset or a rate change.
- [ ] A normally draining host produces increasing OK counts.
- [ ] A configured host that stops draining produces measurable EP=BUSY events.
- [ ] STABLE freezes at the first mounted-endpoint rejection.
- [ ] Stop, reset, rate change and page exit send cleanup through MidiDispatchTask.
- [ ] No RX, clock, SMF scheduling, note ownership or global pacing code changes.
