# 0.9.10 R7 — Persisted MIDI input configuration

R7 persists only the stable controller configuration: enabled, OMNI/SINGLE channel, channel 1..16, and target Synth A/Synth B/Drums. The fixed GM drum map is not duplicated in storage.

A pure versioned 32-bit codec lives in `src/input`; Arduino `Preferences` remains platform-owned by the existing Cardputer MIDI settings session. The new key `midi_in` shares the existing `grooveputer` namespace but remains independent of outbound DeviceProfile settings.

Defaults are input OFF / OMNI / Synth A. Invalid magic/version/target decodes to those safe defaults. Active notes, session identity, queue state, overflow state and diagnostics are never persisted.

Boot applies the persisted input config to the authoritative `MidiInputRouter` before the USB dispatcher starts. UI mutations in R8 use the same session API, which persists before applying cleanup-first runtime config.
