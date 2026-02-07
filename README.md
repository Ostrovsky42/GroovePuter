# MiniAcid

**MiniAcid** is a portable, real-time groove computer for the **M5Stack Cardputer**.

It is not just a sequencer — it is a **time-feel engine**:
a system that separates *what is generated* from *how it feels in time* and *how it sounds right now*.

> Less genre switching. More control over groove, density, and perception of time.

Based on the original [grooveputer](https://github.com/urtubia/grooveputer) by **urtubia**, heavily re-architected and extended.

## Features

* 🎛 **Two TB-303–style synth voices**
* 🥁 **TR-808–inspired drum engine**
* ⏱ **Time-based FEEL system** (half / normal / double feel)
* 🎚 **Live texture layer** (Lo-Fi / Drive / Tape)
* 🎼 **Genre-driven generative engine**
* 🔁 **Pattern + song arrangement**
* 💾 **Full scene persistence**
* ⚡ **44.1 kHz Hi-Fi audio, DRAM-only optimized**
* 🧠 **Stable under heavy DSP load (adaptive FX safety)**

## Architecture

MiniAcid is built around **explicit separation of responsibilities**:

```
GENRE     → What is generated (musical logic)
SETTINGS  → How patterns are generated (regen-time)
FEEL      → How time is perceived (live)
TEXTURE   → How sound is colored (live)
```

This separation ensures predictable behavior: changing genre affects note patterns but not timing, changing feel affects playback but not stored patterns, and texture is purely a real-time audio effect.

---

## Pages Overview

### GENRE

Controls **generative behavior** with two modes:
- **CURATED**: Safe, pre-designed style combinations
- **ADVANCED**: Full control over all parameters

Parameters include:
- Motif structure
- Note density
- Rhythmic masks
- Scale and bias

### FEEL / TEXTURE

**FEEL** controls time perception (live):
- Grid resolution (1/8, 1/16, 1/32)
- Timebase (half, normal, double)
- Pattern length (1-8 bars)

**TEXTURE** shapes the sound (live):
- Lo-Fi processing
- Drive/saturation
- Tape effects

### SETTINGS

Affects **future pattern generation only** (regen-time):
- Swing amount
- Ghost notes
- Note and octave ranges
- Quantization
- Humanization

All regen parameters are clearly marked as **(regen)** in the UI.

## Performance & Stability

* DSP hotspots optimized in critical paths
* Adaptive FX safety (FX dry-down instead of audio crackle)
* Stable page switching during playback
* No PSRAM dependency (Cardputer compatible)

## Controls & Docs

* 📘 **Keyboard map**: [`keys_sheet.md`](keys_sheet.md)
* 🧭 **Design & transition notes**: [`uneversal.md`](uneversal.md)
* 🎛 **FEEL recipes**: [`docs/FEEL_RECIPES.md`](docs/FEEL_RECIPES.md)

## Build & Flash

### Prerequisites

- **Hardware**: M5Stack Cardputer (ESP32-S3)
- **Software**: Arduino CLI or PlatformIO

> **Note**: M5Stack Cardputer has no PSRAM. MiniAcid is optimized for DRAM-only operation with 44.1 kHz audio.

### Quick Build (Recommended)

```bash
# Build release firmware
./build.sh

# Upload to device
./upload.sh /dev/ttyACM0
```

### Manual Build (Arduino CLI)

```bash
# Compile
arduino-cli compile \
  --fqbn m5stack:esp32:m5stack_cardputer:PSRAM=enabled

# Upload
arduino-cli upload \
  --fqbn m5stack:esp32:m5stack_cardputer:PSRAM=enabled \
  -p /dev/ttyACM0
```

### Desktop Build (SDL)

For development and testing on desktop:

```bash
cd platform_sdl
make
./miniacid
```

---

## Getting Started

1. **Flash the firmware** to your M5Stack Cardputer
2. **Read the controls**: Open [`keys_sheet.md`](keys_sheet.md) for complete keyboard reference
3. **Select a genre**: Choose a musical style or create your own
4. **Shape time**: Use FEEL page to control groove and timing
5. **Color the sound**: Apply TEXTURE effects for sonic character
6. **Compose**: Build patterns and arrange songs

## Project Structure

```
.
├── src/
│   ├── dsp/          # Audio engine and DSP
│   ├── ui/           # User interface and pages
│   ├── audio/        # Audio I/O and recording
│   └── sampler/      # Sample playback system
├── platform_sdl/     # Desktop SDL build
├── docs/             # Additional documentation
└── keys_sheet.md     # Complete keyboard reference
```

## Contributing

Contributions are welcome! Please:
- Follow the existing code style
- Test on hardware before submitting PRs
- Update documentation for new features

## Credits

Based on [grooveputer](https://github.com/urtubia/grooveputer) by **urtubia**.

Heavily re-architected with new features:
- Dual song slots
- FEEL/TEXTURE separation
- Genre system expansion
- Performance optimizations

## License

MIT License - see LICENSE file for details
