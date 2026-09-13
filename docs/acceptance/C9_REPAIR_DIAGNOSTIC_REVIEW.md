# C9 repair diagnostic review

Base: `af11031b379a633cb3ce3855bbe270a0e61980db`.
Branch: `repair/20260913-c9-final-stabilization`.

The previously cited `45e0aacf...` is not authoritative.
Hardware reset is reported; its firmware SHA, raw panic/backtrace and root
cause are not established. No hardware acceptance is claimed here.

## Crash instrumentation

Transition-only UI breadcrumbs bracket restore, bounds/style, onEnter, title,
and the first tick/draw/frame completion. ESP32 logs include free internal
8-bit memory and largest internal block. No steady-state frame logging or
audio/MIDI path instrumentation is added. Serial delivery can lose the final
message during a reset; the last received breadcrumb is a bound, not proof of
the failing instruction. Logging can affect timing.

On this base page 8 is FeelTexturePage and page 9 is SettingsPage. Reproduction
must record the actual numeric transition rather than assuming page 9 is FEEL.

## MIDI review findings

The USB RX drain currently handles realtime transport only and ignores melodic
NoteOn/NoteOff. MidiInputRouter is absent from this tree. Restoring configurable
input requires review of the historical router and lifecycle owner as well as
runtime wiring. Output channel mapping is a separate responsibility.

TeeMidiTransport already probes the demoted primary and restores authority
after a successful send. Existing tee tests cover those behaviors.

A deterministic transport-level witness exposes incomplete cleanup:

1. Both endpoints accept NoteOn(0, 60, 100).
2. USB rejects 64 timing-clock sends and is demoted; DIN remains available.
3. NoteOff(0, 60, 0) is rejected by USB, accepted by DIN, and acknowledged by Tee.
4. USB resumes accepting timing clock and regains authority.
5. No NoteOff was sent to USB and no retained cleanup is replayed.

Observed: `release_ack=1 usb_noteoff=0 din_noteoff=1 recovered=1`.
This is a transport cleanup gap, not yet a hardware hanging-note reproduction.
The caller cannot infer partial delivery from the successful return. A repair
needs endpoint cleanup accounting; merely changing probing cannot solve it.

## Build evidence

Local FS1B Cardputer build passed with 187168 bytes DRAM globals against 191488
budget. Final link contains candidate FatFs and excludes stock FatFs.
C4 ingress/panic and C5 source/style contracts passed. Existing tee tests passed.
This local diagnostic build is not the C8 CI artifact or a final repair candidate.
MIDI fixes and comprehensive final review remain outstanding.
