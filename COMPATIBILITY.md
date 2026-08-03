# External MIDI compatibility

GroovePuter acts as a class-compliant USB-MIDI device. Direct USB connection requires the receiving instrument to operate as a USB host.

Compatibility claims are tied to an exact GroovePuter firmware SHA, target firmware version, and physical connection. Standard MIDI support alone does not prove direct USB-C device-to-device compatibility.

## Tested

### Yamaha SEQTRAK, OS 2.00

**GroovePuter firmware:** `b256ff180165e8db37e61be8658b13c0ae2bcd5c`  
**Connection:** direct USB-C  
**USB roles:** GroovePuter device; SEQTRAK host

Validated:

- Note On / Note Off;
- native SEQTRAK 11-track routing;
- MIDI Clock;
- Start / Stop;
- SEQTRAK external-master follow;
- PROJECT SMF synchronization.

SEQTRAK transmits MIDI Clock, Start, and Stop, but did not transmit MIDI Continue (`0xFB`) in the tested physical Stop -> Play workflow. After Stop -> Play, an active PROJECT SMF restarts from MUSIC START.

True position-preserving Continue remains available from MIDI controllers that actually transmit `0xFB`.

Detailed acceptance: [`docs/tests/EXTERNAL_MIDI_COMPATIBILITY_ACCEPTANCE.md`](docs/tests/EXTERNAL_MIDI_COMPATIBILITY_ACCEPTANCE.md).

## Protocol-compatible candidates — not hardware validated

### Akai MPC standalone models with USB host ports

Class-compliant note input is likely because supported MPC standalone USB-A host ports accept class-compliant USB MIDI controllers. Direct USB Clock and transport synchronization are not yet claimed.

### Novation Circuit Tracks / Circuit Rhythm

The devices document MIDI Note, CC, Program Change, and Clock over USB and 5-pin DIN. Direct GroovePuter USB-device connection is not claimed because a USB host role has not been established. Use DIN MIDI through a future GroovePuter DIN backend or a USB-MIDI host bridge.

### Roland MC-707

The MC-707 provides 5-pin MIDI and a USB-B audio/MIDI device port. Use DIN MIDI or a USB-MIDI host bridge; direct GroovePuter USB-device connection is not supported by this topology.

### Roland P-6, S-1, and SH-4d

These are protocol-compatible candidates through TRS MIDI or a USB-MIDI host bridge. Direct USB-C is not claimed. Continue and Song Position behavior differs by model:

- P-6 recognizes Continue but documents it as equivalent to Start and does not support Song Position;
- S-1 does not support Continue or Song Position;
- SH-4d explicitly has no USB host feature.

## Status meanings

| Status | Meaning |
|---|---|
| `TESTED` | Exact hardware acceptance exists for a recorded firmware SHA and topology. |
| `LIKELY` | Public MIDI documentation is compatible, but no GroovePuter hardware test exists. |
| `BRIDGE REQUIRED` | Standard MIDI is supported, but direct USB roles do not match. |
| `UNSUPPORTED` | A required message, route, connector, or USB role is unavailable. |

“Likely compatible” does not mean that direct USB-C device-to-device connection has been tested.

Full matrix and primary references: [`docs/reference/EXTERNAL_MIDI_COMPATIBILITY.md`](docs/reference/EXTERNAL_MIDI_COMPATIBILITY.md).
