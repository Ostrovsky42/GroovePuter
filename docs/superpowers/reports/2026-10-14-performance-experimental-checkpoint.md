# Experimental Performance / HGS checkpoint

Branch: `experiment/performance-hgs-diagnostics`, based on `5e98acbd`.
This is an experimental diagnostic direction, not a release or hardware acceptance.

## Saved changes

- Preserve the full harmonic event key; reject unsupported legacy quality mappings.
- Preserve existing input owners on capacity rejection; report sustain-owner overflow.
- Deduplicate Fn+D and draw its overlay inside each display frame before flush.
- Handle framebuffer allocation failure and allocate it before I2S initialization.
- Show project scale on GENRE/HARMONY, offer M:SCALE and explain GRAPH refusal.
- Give Ctrl+A TB303 cutoff feedback and reject that reset on other synth engines.
- Add bounded, control-side Fn+D TB303 amplitude/PCM/FX/queue diagnostics.
- Test actual internal PCM release for chords and EUCLID ON -> OFF.

The full 32-bit context ordinal is laid out with ordinary bool fields to keep
PerformanceContext <=24 bytes and retain strict C++17 compatibility. The earlier
intermediate bit-field implementation required C++20 and is superseded.

## Current evidence and limitations

HELD ARP works on hardware. Other pitch sources need captured memory/root or
valid generated Song harmony context; STOP alone is not a sufficient diagnosis.
User reports internal sustained sound after EUCLID ON -> OFF, while h=0 and
liveA/B=-1, in MONO and POLY. New notes coexist with that sound; changing voice
mode or starting transport clears it. MIDI hanging attribution was withdrawn.
This physical defect is unresolved. Explicit Opt+note HOLD is a separate idea,
not an implemented feature or justification for the unintended sustain.

All Smart Play groups, internal closure/cleanup tests, framebuffer/input-edge
checks and Undo source regression pass. Initialized real-engine UI/PCM tests
pass for both TB303 voices and the EUCLID transition. An earlier fixture omitted
engine.init(), causing uninitialized waveform overlay reads; that fixture is
corrected. Focused SDL M:SCALE interactions were checked; no full SDL acceptance
claim. Firmware uses the FS1B candidate FatFs profile and the diagnostic flag.

Next: flash the current diagnostic image when USB bootloader is available,
reproduce EUCLID ON -> OFF and compare TB303 active/envelope against final PCM.
The image uses Fn+D screen diagnostics, not CDC serial logging. No new image
has been flashed since review-audio-diagnostic-20261014. No release merge.

## Final checkpoint firmware build

Strict C++17 context layout rebuilt successfully with the same diagnostic
ADV/FS1B profile. Sketch uses 1670922 bytes (53%) of program storage space. Maximum is 3145728 bytes.
Global variables use 189652 bytes (57%) of dynamic memory, leaving 138028 bytes for local variables. Maximum is 327680 bytes.
Cardputer DRAM globals: 189652 bytes (budget 191488)

ELF SHA256 `05de4b93ddbfc28778a56b701f263e30d1bf54c4e7686b8967444cbea9873736`

BIN SHA256 `91a7c7a6f67e9495c3455e1165d6bd0289f2ba7c578fcf3e011d36dac6d26eb9`

Build log: `/tmp/gp-experimental-checkpoint-build.log`.
These replace the prior hashes for the same local build directory. Not flashed.
