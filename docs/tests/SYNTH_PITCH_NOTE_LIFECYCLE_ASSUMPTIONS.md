# Assumptions

- GroovePuter remains a musical instrument rather than a cycle-exact chip emulator.
- AY uses 1.7734 MHz tone-period quantization and host-rate waveform rendering.
- SN76489 frequencies below its divider floor are folded upward by octaves.
- `Oct+` explicitly means upward octave stacking.
- The public live-note range remains C1..B4.
- Hardware acceptance on Cardputer-Adv is required before merge.
