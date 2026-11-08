# GroovePuter 0.9.20 installation draft

**Status: NOT A RELEASE.** This route has not been tried with the candidate
image on physical hardware. It is written for **M5Stack Cardputer ADV** only;
the current image is not accepted for the original Cardputer.

## Candidate image

- App image for M5Launcher: `GroovePuter.ino.bin`
- Profile: Cardputer ADV, PSRAM disabled, `huge_app`, USB CDC on boot disabled,
  dynamic FatFs FS1B
- SHA-256: use the value for `GroovePuter.ino.bin` in the adjacent
  `SHA256SUMS.txt`. This draft does not identify the earlier dirty-tree image.

## Install with M5Launcher, without Arduino IDE

M5Launcher currently lists Cardputer and Cardputer ADV as supported devices and
can install an app `.bin` from SD or OTA. This provides a no-IDE candidate route;
compatibility with this exact GroovePuter image remains unverified.

1. If M5Launcher is not already installed, use its [official browser flasher](https://bmorcelli.github.io/Launcher/) or M5Burner to install it for
   Cardputer/ADV. The browser flasher requires a data USB cable and a supported
   browser.
2. Back up any device data you need before replacing firmware. Do not assume an
   app install preserves GroovePuter projects until that has been checked on the
   device.
3. Copy `GroovePuter.ino.bin` to the root of a FAT32 microSD card. Do not select
   the merged image in M5Launcher.
4. Insert the card, open **SD** in M5Launcher, select the GroovePuter app image,
   choose **Install**, and confirm the target.
5. Boot GroovePuter. Complete the first-session steps in `QUICKSTART.md`, then
   test Save, reboot, Load, and resume work.

M5Launcher also supports installing a direct firmware URL through OTA. Once a
GitHub Release exists, a release asset can be installed that way; this draft
does not point at a live release URL.

## Restore firmware

Use M5Stack's [Cardputer-Adv factory restore guide](https://docs.m5stack.com/en/guide/cardputer_adv/restore_factory) and its M5Burner
flow to return to factory firmware. This replaces the current app and has not
been exercised with the GroovePuter candidate. The repository's
`scripts/restore_m5launcher.sh` can download and verify the official Launcher
image; do not run its `--flash` path until you intend to replace the current
firmware.

## Acceptance evidence required

Record the device model/revision, image SHA-256, install route, first boot,
project create/save/reboot/load, USB MIDI, SD access, recovery result, and any
errors. Until this is done on a physical Cardputer ADV, the install and recovery
journey remains **UNVERIFIED** and this file must remain a draft.
