# 0.9.10 R3b2 — USB MIDI runtime wiring

R3b2 connects the pure R3b1 USB-MIDI channel-voice adapter to the canonical R2
queue and R3a router without introducing a second TinyUSB owner.

Runtime path:

`MidiDispatchTask -> readPacket -> realtime owner OR channel-voice parser ->
MidiInputQueue -> MidiInputRouter -> MusicalEventRouter`.

The application owns the fixed queue/router objects. `MidiDispatchTask` remains
the only TinyUSB FIFO reader. USB mount edges create a non-zero input session
identity; disconnect/reconnect cleanup discards pending ingress and panics only
MIDI-input-owned notes before publishing a replacement session.

Input remains disabled by default. No Drums, sustain, pitch-bend application,
persistence, UI, MIDI THRU, USB Host mode, or recording is introduced here.
