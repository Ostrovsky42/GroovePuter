# External MIDI compatibility

GroovePuter acts as a class-compliant USB-MIDI **device**. A direct USB connection works only when the receiving instrument can act as a USB **host** and accepts a class-compliant MIDI device.

This document separates hardware-tested results from protocol-level candidates. A device is never promoted to `TESTED` from manuals alone.

## Status vocabulary

| Status | Meaning |
|---|---|
| **TESTED** | Direct hardware acceptance exists for an exact GroovePuter SHA, target firmware version, and connection topology. |
| **LIKELY** | Public documentation supports the required standard MIDI messages, but GroovePuter has not been connected and tested. |
| **BRIDGE REQUIRED** | The target documents MIDI support, but its physical/USB role does not allow direct GroovePuter USB-device connection; use DIN/TRS or a USB-MIDI host bridge. |
| **UNSUPPORTED** | Required messages, routing, electrical interface, or host role are unavailable or known not to work. |

`LIKELY` never means direct USB-C device-to-device operation has been tested.

## Tested

### Yamaha SEQTRAK, OS 2.00

**Status:** `TESTED`  
**GroovePuter SHA:** `b256ff180165e8db37e61be8658b13c0ae2bcd5c`  
**Connection:** direct USB-C  
**USB roles:** GroovePuter device; SEQTRAK host

Validated:

- USB enumeration;
- Note On / Note Off;
- native SEQTRAK 11-track routing;
- MIDI Clock;
- Start / Stop;
- SEQTRAK external-master follow;
- PROJECT SMF synchronization.

SEQTRAK did not transmit MIDI Continue (`0xFB`) during a physical Stop -> Play sequence. The following Play behaved as Start, so an active PROJECT SMF restarted from MUSIC START. GroovePuter still supports position-preserving Continue when another controller actually transmits `0xFB`.

Full record: [`../tests/EXTERNAL_MIDI_COMPATIBILITY_ACCEPTANCE.md`](../tests/EXTERNAL_MIDI_COMPATIBILITY_ACCEPTANCE.md).

## Protocol-compatible candidates — not hardware validated

### Akai MPC standalone models with USB host ports

**Status:** `LIKELY` for direct class-compliant note input; Clock/transport not claimed.

Akai documents that MPC standalone USB-A host ports accept class-compliant USB MIDI controllers. This makes direct enumeration plausible for models and OS versions that expose those host ports.

Before promotion to `TESTED`, verify:

- exact MPC model and OS;
- GroovePuter enumeration as an input port;
- Note On / Note Off;
- outbound GroovePuter Clock/Start/Stop reception;
- reverse clock ownership if claimed;
- Continue and SPP behavior;
- panic and reconnect cleanup.

Official references:

- https://support.akaipro.com/en/support/solutions/articles/69000858914-mpc-standalone-how-to-test-external-midi-controllers
- https://support.akaipro.com/en/support/solutions/articles/69000809463-akai-pro-mpc-live-syncing-a-midi-controller-with-an-mpc-live

### Novation Circuit Tracks

**Status:** `BRIDGE REQUIRED` until direct USB-host behavior is demonstrated.

Circuit Tracks documents Note, CC, Program Change, and MIDI Clock receive/transmit over USB and 5-pin DIN. Its USB-C port is documented as a class-compliant interface, but GroovePuter is also a USB device; direct device-to-device USB is therefore not claimed.

Preferred test path:

```text
GroovePuter USB device
  -> USB-MIDI host bridge
  -> Circuit Tracks 5-pin MIDI In
```

or a future GroovePuter DIN backend directly into Circuit Tracks MIDI In.

Official references:

- https://userguides.novationmusic.com/hc/en-gb/articles/25494476280850-Circuit-Tracks-hardware-overview
- https://userguides.novationmusic.com/hc/en-gb/articles/25494477003154-Circuit-Tracks-appendix

### Novation Circuit Rhythm

**Status:** `BRIDGE REQUIRED` until direct USB-host behavior is demonstrated.

Circuit Rhythm documents Note, CC, Program Change, and MIDI Clock receive/transmit over USB and 5-pin DIN. As with Circuit Tracks, no direct GroovePuter USB-device connection is claimed without a demonstrated USB host role.

Official references:

- https://userguides.novationmusic.com/hc/en-gb/articles/25494352288146-Circuit-Rhythm-hardware-overview
- https://userguides.novationmusic.com/hc/en-gb/articles/25494352755730-Circuit-Rhythm-appendix

### Roland MC-707

**Status:** `BRIDGE REQUIRED`.

MC-707 has 5-pin MIDI In/Out and a USB-B audio/MIDI device port. The USB-B port is not a host connection for GroovePuter, so use DIN MIDI or a USB-MIDI host bridge.

Official references:

- https://www.roland.com/global/products/mc-707/support/
- https://www.roland.com/global/support/by_product/mc-707/owners_manuals/

### Roland SH-4d

**Status:** `BRIDGE REQUIRED`.

Roland explicitly states that SH-4d's USB-C port has no USB host feature. A direct GroovePuter USB-device connection cannot work. Use the documented MIDI connectors through the appropriate cable/bridge.

Official reference:

- https://support.roland.com/hc/en-us/articles/16508634472987-SH-4d-Can-I-use-a-MIDI-controller-via-a-USB-connection

### Roland P-6

**Status:** `BRIDGE REQUIRED`.

P-6 is class-compliant as a USB audio/MIDI device and supports external MIDI through its TRS MIDI connection. No direct P-6 USB-host role is claimed. Its MIDI implementation recognizes Clock, Start, Continue, and Stop, but Continue is documented as behaving like Start; Song Position is not supported.

This makes P-6 a useful compatibility target, but not a model for position-preserving Continue.

Official references:

- https://static.roland.com/manuals/p-6/en-US/141557771143073803.html
- https://static.roland.com/manuals/p-6/en-US/144771723145436939.html

### Roland S-1

**Status:** `BRIDGE REQUIRED`.

S-1 supports MIDI over USB and external MIDI, but the published implementation chart transmits and recognizes Clock, Start, and Stop while Continue and Song Position are unsupported. Use TRS MIDI or a USB host bridge; do not claim position-preserving Continue.

Official references:

- https://static.roland.com/manuals/s-1_manual_v102/eng/87294690.html
- https://support.roland.com/hc/en-us/articles/32862076336283-S-1-I-can-t-start-stop-the-S-1-sequencer-with-an-external-MIDI-foot-switch

## Connection rule

Before adding a candidate, answer these separately:

1. Does the target recognize standard MIDI Note On / Note Off?
2. Does it recognize Clock, Start, Stop, Continue, or SPP?
3. Which physical connection carries those messages?
4. Which side is the USB host?
5. Is a bridge required?
6. Has that exact topology been tested with a recorded firmware SHA?

A positive answer to question 1 does not imply a positive answer to questions 3–6.

## Profile model required before broad compatibility claims

The runtime and documentation must distinguish at least:

```text
SEQTRAK_NATIVE_11
GM_16
GENERIC_16
```

- `SEQTRAK_NATIVE_11` owns Yamaha's fixed CH1–11 track layout and target-specific CC capabilities.
- `GM_16` uses the General MIDI channel/note conventions, including GM percussion behavior.
- `GENERIC_16` preserves ordinary 16-channel MIDI without silently assuming either Yamaha or GM semantics.

The current `Custom` profile must not be treated as an undocumented substitute for a stable `GENERIC_16` contract. Migration and naming should be handled in a focused compatibility PR with backward-compatible settings defaults.

## Public README rule

The README may list:

- `TESTED` devices with exact acceptance links;
- `LIKELY` candidates only with a conspicuous not-tested label;
- `BRIDGE REQUIRED` devices with the required connection topology;
- no unsupported direct-USB inference from class compliance alone.

Whenever runtime MIDI behavior changes, the compatibility claims remain tied to their recorded SHA until the acceptance matrix is rerun.
