# 0.9.10 R6 — Controller behavior boundary

R6 deliberately keeps normalized controller messages representable without pretending they have product behavior.

For 0.9.10 live input, CC64 Sustain, Pitch Bend, Poly Pressure, Channel Pressure and Program Change remain ignored by `MidiInputRouter`. They do not alter active-note ownership, OutputOwnership, DeviceProfile, or synth internals.

This is an explicit release decision: Synth A/B monitoring is STOP-only and there is no audited common target-capability owner for sustain/bend. Adding either here would couple controller policy directly to DSP ownership. They may be introduced later as separate capability checkpoints.
