# MiniAcid

MiniAcid is a tiny acid groovebox for the M5Stack Cardputer. It runs two squelchy TB-303 style voices plus a punchy TR-808 inspired drum section on the Cardputer's built-in keyboard and screen, so you can noodle basslines and beats anywhere.

> Go play with it: https://miniacid.mrbook.org

## What it does
- Two independent 303 voices with filter/env controls and optional tempo-synced delay
- 16-step sequencers for both acid lines and drums, with quick randomize actions
- Live mutes for every part (two synths + eight drum lanes)
- Pattern and song arrangement system
- **NEW**: 44.1 kHz Hi-Fi audio optimized for DRAM

## Building & Uploading

**NOTE**: M5Stack Cardputer (StampS3) does NOT have PSRAM.  
The project is optimized to run efficiently in DRAM only.

```bash
# Build for Cardputer
./build-psram.sh

# Upload to device
./upload-psram.sh /dev/ttyACM0
```

Or with arduino-cli manually:
```bash
./arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=enabled
./arduino-cli upload --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,PSRAM=enabled -p /dev/ttyACM0
```

## Using it

On the M5Stack Cardputer:

1) Flash using the build scripts above
2) Use the keyboard shortcuts below to play, navigate pages, and tweak sounds.  
3) Jam, tweak synths, sequence, randomize, and mute on the fly

On the web:
1) Go to https://miniacid.mrbook.org

For more detailed instructions, see the [Manual](MANUAL.md).
