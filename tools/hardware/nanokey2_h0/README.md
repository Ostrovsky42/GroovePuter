# nanoKEY2 H0 — RX2 packet diagnostics

Standalone diagnostic; flashing replaces GroovePuter. This revision initializes
the display and registers an ESP-IDF USB host client. It inspects the active
configuration for MIDIStreaming interfaces and bulk endpoints. RX2 claims
the MIDI interface and submits one max-packet-sized bulk IN transfer at a time.
It does not drive UART or ARP. Disconnect cancels transfers before releasing
the interface and closing the device.

This probe exercises the **USB Host** role only. The same Type-C OTG controller
cannot simultaneously remain the current USB MIDI **Device** for a computer;
role switching must happen at boot (or via a deliberate reboot). See
[`docs/midi/2026-09-08-nanokey2-usb-role-options.md`](../../../docs/midi/2026-09-08-nanokey2-usb-role-options.md)
for the Host/Device/UART alternatives and lifecycle contract.

The original e70fc634 probe had no display initialization and was built with
CDC enabled. Its successful upload did not demonstrate successful host startup
or VBUS power. Its blank monitor output is not runtime evidence.

Build from the worktree root using the installed M5Stack 3.2.2 core:

```sh
arduino-cli compile --fqbn 'm5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,USBMode=default,CDCOnBoot=default,UploadMode=cdc' --build-path build/nanokey2-h0-rx2 tools/hardware/nanokey2_h0
```

Upload that exact build with the same FQBN and
`--input-dir build/nanokey2-h0-rx2`; resolve the connected port first.
CDC-on-boot is forbidden by a compile-time check. Do not use the old
`build/nanokey2-h0` artifact.

Expected screen: `nanoKEY2 H0 / RX2`, then `WAITING USB` with a growing
uptime. Connect nanoKEY2 through the intended USB-C adapter while Cardputer
has its own power. `MIDI RX READY` means the initial transfer was submitted;
increasing ON/OFF counters demonstrate validated note messages. RAW retains
the last nonzero packet; NOTE retains the last valid NoteOn/Off. Zero packets
never overwrite either. `--` means no matching message has arrived.
The pkt counter includes zero blocks; zero counts them separately, bad includes
incomplete packets, reserved CIN and malformed note messages. Disconnect/reconnect
should increment the attach/detach counters. Errors display their stage and
ESP error name; `WAITING USB` alone does not establish a power fault.

VBUS voltage has not been measured. No GPIO power
switch is asserted by this sketch. Native USB Serial is unavailable in this
host configuration; observe the display. If automatic upload is unavailable,
use the board's documented G0 boot/download procedure and reconnect to the PC.

User-observed enumeration: VID:PID `0944:0115`, IN `0x81`, OUT `0x02`.
On the initial RX version the packet counter increased while last displayed
zeros. This does not prove the keyboard sent valid notes; RX2 distinguishes
trailing empty blocks from an entirely empty stream. After RX2 the user reported
that it works: qualitative USB RX confirmation on this setup, not end-to-end
PERFORM/UART acceptance. Quantitative note counts, reconnect soak and power/memory
measurements remain pending. See the current
[contracts](../../../docs/midi/2026-09-07-midi-io-contracts.md).
Native regression test:

```sh
g++ -std=c++17 -Wall -Wextra -Werror -I. tests/test_nanokey2_packet_diagnostics.cpp -o /tmp/test_nanokey2_packet_diagnostics
/tmp/test_nanokey2_packet_diagnostics
```
