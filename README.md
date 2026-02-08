# GroovePuter

[![Status](https://img.shields.io/badge/status-beta-yellow)](#status)
[![Platform](https://img.shields.io/badge/platform-M5Stack%20Cardputer%20\(ESP32--S3\)-blue)](#requirements)
[![Build](https://img.shields.io/badge/build-release.sh%20%7C%20arduino--cli-brightgreen)](#build--flash)

> **Portable real-time groove computer for M5Stack Cardputer.**
> A **time-feel engine** that separates **what** is generated from **how it feels in time** and **how it sounds right now**.

Based on the original **MiniAcid** by [urtubia/miniacid](https://github.com/urtubia/miniacid) — this fork focuses on **timing FEEL**, **scene persistence**.



## Status

**Beta.** Core flow is playable and stable on device. APIs/UI may change as the instrument evolves.

## Features

* **Two TB-303–style voices** (bass + lead)
* **TR-808–inspired drum section**
* **Pattern + song arrangement**
* **Dual song slots (`A/B`)** with split compare and live mix controls
* **RETRO split Song view** aligned with cyber theme styling
* **FEEL system (live):**
  * Grid: `1/8 · 1/16 · 1/32`
  * Timebase: `HALF · NORMAL · DOUBLE`
  * Length: `1B … 8B` (cycle length)
* **TEXTURE layer (live):** Lo-Fi / Drive / Tape
* **Genre-driven generator:** rhythmic masks, motif length, scale preference, density traits
* **Groove Lab page:** mode/flavor/macros + corridor/budget preview
* **Scene persistence:** safe load for older scenes (optional fields)


## Architecture

**Hard rule:** no hidden coupling.

```text
GENRE      → what is generated (musical logic)
GENERATOR  → how patterns are generated (regen-time)
FEEL       → how time is perceived (live)
TEXTURE    → how sound is colored (live)
```

Practical consequence:

* You can change FEEL/TEXTURE to reshape a performance **without regenerating**.
* You can change GENERATOR params to affect the next regenerate pass **without altering** the current pattern.

Groove routing today:

```text
GrooveProfile  → corridor source-of-truth + budget rules
ModeManager    → mode/flavor routing + preset application
GenrePage      → genre/texture selection + apply policy
```

---

## Requirements

* **Hardware:** M5Stack Cardputer (ESP32-S3)
* **Tooling:** `arduino-cli` (recommended) or Arduino IDE 2.x
* **ESP32 core:** `esp32:esp32`
* **Memory:** DRAM-only (Cardputer has no PSRAM)

> If your project pins specific library versions, put them in `docs/REQUIREMENTS.md` and link it here.

---

## Build & Flash

### Recommended

```bash
# Build release
./release.sh

# Upload to device
./upload.sh /dev/ttyACM0
```

### Manual (arduino-cli)

```bash
arduino-cli compile \
  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc

arduino-cli upload \
  --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc \
  -p /dev/ttyACM0
```

---

## Getting Started

1. Flash the device.
2. Open the keyboard map: [`docs/keys_sheet.md`](docs/keys_sheet.md)
3. Pick a **GENRE** preset.
4. Shape groove behavior in **GROOVE LAB** (`ModePage`).
5. Shape time with **FEEL**.
6. Add **TEXTURE** and **TAPE** for performance color.
7. Arrange in **SONG** (split compare `X`, edit/play slots `Alt+B` / `Ctrl+B`).

---

## Demos

* **Screenshots:** see [Screenshots](#screenshots)
* **Audio/video:** add links here when available (YouTube/SoundCloud/short clip)

---

## Screenshots

Drop images into `docs/screenshots/` and update the list below.

* `docs/screenshots/genre.png` — GENRE page
* `docs/screenshots/feel_texture.png` — FEEL/TEXTURE page
* `docs/screenshots/generator.png` — GENERATOR page
* `docs/screenshots/hud.png` — live HUD (G/T/L + cycle pulse)

```text
Tip: keep screenshots at the same zoom level and include the header/footer.
```

---

## Troubleshooting

### Upload fails / can’t connect

* Use a **data** USB cable (not charge-only)
* Confirm the port exists:

  ```bash
  ls /dev/tty* | head
  ```
* Confirm board detection:

  ```bash
  arduino-cli board list
  ```

### Audio crackle under heavy load

* This build uses **adaptive FX safety** (FX dries out briefly instead of crackling).
* If crackling persists: reduce FX intensity (Tape mix/feedback, delay mix), then re-test.

## Quick Keys 
- `Space`: play/stop
- `[` / `]`: previous/next page
- `Arrows`: move cursor / navigate lists
- `Enter`: confirm/apply/toggle focused item
- `Tab`: switch focus/section on many pages
- `Q..I`: choose pattern slot `1..8` in Pattern/Drum/Song contexts
- `B`: quick A/B bank toggle (Pattern/Drum) or bank flip in Song cell/selection
- `Alt+B`: edit song slot `A/B`
- `Ctrl+B`: play song slot `A/B`
- `X`: split compare (Song) or primary action on Tape page
- `Esc`: back (or clear selection in editors)

---

## Contributing

This is an **experimental instrument**. If you want to contribute:

* Keep PRs small and testable.
* Prefer changes that preserve the core rule: **GENRE ≠ FEEL ≠ GENERATOR ≠ TEXTURE**.
* If you’re unsure where a change belongs, open an issue first.

---

## Credits

* Original inspiration: [urtubia/miniacid](https://github.com/urtubia/miniacid)
* Hardware: M5Stack Cardputer
* References: TB-303 / TR-808 lineage 

---

## License

MIT (`LICENSE`)
