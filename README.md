# MiniAcid

MiniAcid is a portable groovebox for **M5Stack Cardputer (ESP32-S3)** with dual 303 voices, drum sequencing, song arrangement, tape workflow and genre-driven generation.

## Current Highlights
- Dual TB-303 engine (`303A` / `303B`)
- Drum sequencer with A/B pattern banks
- Song arranger with dual slots (`A`/`B`), split compare and live mix controls
- Genre + Texture workflow with apply modes (`SND`, `S+P`, `S+T`)
- New **GROOVE LAB** page (Mode/Flavor/macros/preview + corridor visibility)
- Tape looper workflow (`CAPTURE`, `THICKEN`, `WASH`, smart `REC/PLAY/DUB`)
- Scene save/load with SD persistence

## Architecture (practical split)
- `GenreManager`: generation behavior + musical policy
- `ModeManager`: mode/flavor routing and preset application
- `GrooveProfile` (`src/dsp/groove_profile.*`): corridor source-of-truth + budget rules
- UI pages are thin controllers over engine state

## Core Docs
- Keymap: `keys_sheet.md`
- Song page quickstart: `docs/SONG_PAGE_QUICKSTART.md`
- Song styles: `docs/SONG_PAGE_STYLES.md`
- Tape workflow: `docs/TAPE_WORKFLOW.md`
- Groove Lab (Mode/Flavor/corridors): `docs/GROOVE_LAB.md`

## Build

### Quick
```bash
./build.sh
./upload.sh /dev/ttyACM0
```

### Release profile
```bash
./release.sh
```

### Manual (Arduino CLI)
```bash
arduino-cli compile \
  --fqbn m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,FlashMode=dio

arduino-cli upload \
  --fqbn m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,FlashMode=dio \
  -p /dev/ttyACM0
```

## Audio Safety Note
- Master high-cut is currently **hardcoded in DSP** (not exposed in Project UI).
- Default project value: `16000 Hz` (`kMasterHighCutHz` in `src/dsp/miniacid_engine.h`).
- Typical safe ranges for compact speakers:
  - `12000 Hz`: stronger ear protection
  - `16000 Hz`: balanced default
  - `18000 Hz`: brighter, less protection

## License
MIT (`LICENSE`)
