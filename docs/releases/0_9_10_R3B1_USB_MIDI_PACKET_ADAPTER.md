# GroovePuter 0.9.10-R3b1 — USB-MIDI channel-voice packet adapter

## Purpose

Prove USB-MIDI 1.0 Event Packet framing for controller channel-voice messages before changing the existing Cardputer TinyUSB runtime.

Exact stacked base:

```text
0.9.10-R3a routing/ownership
a1d252cc63c864b14cd53d325734f786c1b82226
```

R3b1 is parser-only. R3b2 performs the physical runtime wiring after this gate is clean.

## Architecture

```text
USB-MIDI Event Packet bytes
        ↓
parseUsbMidiChannelVoice()
        ↓
NormalizedMidiInputMessage
        ↓
R2 queue later in R3b2
```

The parser is pure C++ and has no Arduino, TinyUSB, FreeRTOS, Cardputer, routing, UI, persistence, or output-ownership dependency.

## Framing contract

Accepted USB-MIDI 1.0 CIN values:

```text
0x8 Note Off
0x9 Note On
0xA Poly Pressure
0xB Control Change
0xC Program Change
0xD Channel Pressure
0xE Pitch Bend
```

The CIN must agree with the MIDI status high nibble. Cable number bits in the USB packet header do not affect parsing.

Program Change and Channel Pressure contain one MIDI data byte; USB packet byte 3 is padding and is ignored. Other accepted channel-voice messages require two valid 7-bit data bytes.

The parser delegates canonical MIDI semantics to the R2 normalizer, including `NoteOn velocity=0 -> NoteOff` and explicit transport/session identity.

System Common, SysEx and System Realtime are rejected. Existing Clock/Start/Continue/Stop remain owned by `usb_midi_realtime_parser.h` and the external transport queue.

## Runtime boundary

R3b1 does not modify `cardputer_usb_midi_transport.cpp` and does not instantiate the R2 queue/router. A source regression requires zero runtime references to `parseUsbMidiChannelVoice()` outside the parser itself.

The existing sole TinyUSB FIFO owner remains `MidiDispatchTask`; no `tud_midi_rx_cb` is introduced.

## Hardware list

None for R3b1.

## Wiring

None. Direct USB controller behavior is not claimed by this parser checkpoint.

## Build / test

```bash
bash tests/run_midi_input_0_9_10_r3b1_tests.sh
```

The runner compiles with C++17, `-Wall -Wextra -Werror -pedantic`.

## Build / flash

No firmware flash is required. R3b1 has no runtime integration.

## Expected behavior

Focused output must end with:

```text
0.9.10 R3b1 USB MIDI parser/source boundaries: PASS
0.9.10 R3b1 USB MIDI channel-voice framing: PASS
```

Tests cover NoteOn/Off, velocity-zero canonicalization, Poly Pressure, CC, Program Change, Channel Pressure, centered Pitch Bend, cable-number independence, CIN/status mismatch, invalid data bytes, invalid transport/session IDs, and rejection of realtime/system/SysEx framing.

## Troubleshooting

If a realtime `0xF8/0xFA/0xFB/0xFC` packet passes this parser, restore the channel-voice boundary; do not merge controller input with transport ownership.

If Program Change or Channel Pressure rejects non-zero USB padding byte 3, the parser is incorrectly treating padding as MIDI data.

If CIN/status mismatch is accepted, do not trust status alone; malformed USB Event Packets must fail closed.

## Acceptance checklist

- [ ] exact base is software-clean R3a `a1d252cc63c864b14cd53d325734f786c1b82226`;
- [ ] parser has no TinyUSB/Arduino/FreeRTOS/Cardputer dependency;
- [ ] CIN 0x8..0xE is required and agrees with status class;
- [ ] Program/Channel Pressure one-data-byte framing is correct;
- [ ] realtime/system/SysEx remains outside this parser;
- [ ] R2 normalization is reused;
- [ ] no second TinyUSB RX owner exists;
- [ ] no production runtime uses the parser yet;
- [ ] focused R3b1 tests pass on exact candidate SHA;
- [ ] inherited regression CI remains green;
- [ ] no hardware behavior is claimed;
- [ ] R3b2 is the only next checkpoint allowed to wire this parser into the existing `MidiDispatchTask`.
