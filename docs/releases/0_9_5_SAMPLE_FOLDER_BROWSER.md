# GroovePuter 0.9.5-A — Sample folder browser

## Purpose

Add a practical folder browser for loose sampler WAV files without reintroducing the removed unsafe kit loader.

The browser operates only below the existing loose-sample root:

```text
/samples/
```

Cardputer mount alias remains:

```text
/sd/samples/
```

At boot, `SampleIndex` recursively discovers WAV files once and binds the complete catalog to `RamSampleStore`. The UI browser then uses only this in-memory catalog; opening/navigating the browser does not rescan SD and does not replace the global SampleIndex.

The recursive catalog does not keep the historical duplicate basename `std::map`; legacy basename lookup is computed from the catalog and fails closed when the same filename exists in more than one folder.

This is not the canonical `/kits/<kit>/kit.json` model and does not implement LOAD KIT or RELINK.

## Hardware list

- M5Stack Cardputer ADV / ESP32-S3
- microSD card
- USB-C data cable

No external module is required.

## Wiring

No external wiring.

Use the built-in microSD slot. PORT.A / I2C GPIO2(SDA) / GPIO1(SCL) is unchanged and unused by this test.

## SD layout

Example:

```text
/samples/
  loose.wav
  909/
    kick.wav
    snare.wav
    hats/
      closed.wav
  SP12/
    kick.wav
```

Rules:

- `.wav` extension is case-insensitive;
- non-WAV files are ignored;
- hidden files/directories whose name begins with `.` are ignored;
- recursive discovery is bounded to 8 directory levels below `/samples`;
- folders under `/kits` are not scanned by this browser;
- two folders may contain the same basename such as `909/kick.wav` and `SP12/kick.wav`; stable path identity keeps them distinct;
- basename-only legacy lookup returns no match when a basename is ambiguous.

## Build / Flash steps

Focused host gate:

```bash
bash tests/run_sampler_folder_browser_tests.sh
```

Normal release gates:

```bash
bash tests/run_host_tests.sh
bash scripts/install_arduino_deps.sh
bash scripts/build.sh --warnings all
bash scripts/check_cardputer_dram_budget.sh build/cardputer-adv-current/GroovePuter.ino.elf
bash scripts/build_seqtrak_midi_only.sh --warnings all
```

Flash the exact PR candidate:

```bash
git rev-parse HEAD
bash scripts/upload.sh /dev/ttyACM0
arduino-cli monitor -p /dev/ttyACM0 -c baudrate=115200
```

## Controls

Open `DRUMS -> SAMPLES`, focus the `SAMPLE:` row, then:

```text
Enter       open SAMPLE BROWSER
Up/Down     move selection
Enter/Right enter selected folder or assign selected WAV
Backspace   go to parent; at root closes browser
Left        go to parent
Esc         close browser without changing the pad
```

Outside the browser, Left/Right on `SAMPLE:` remains the quick selector, now scoped to WAV files in the current sample's folder.

Direct Q/W/E/R/T/Y/U/I pad triggering remains unchanged when the browser is closed.

## Expected behavior

### Screen

For the example SD layout the root browser shows approximately:

```text
SAMPLE BROWSER
/
> 909/
> SP12/
  loose.wav
```

Opening `909/` shows its immediate WAV files and `hats/`. Opening `hats/` shows `closed.wav` and a `..` parent entry.

Selecting a WAV performs the normal hardened preload first. Only after preload succeeds is the current pad assignment published.

The assigned `SAMPLE:` value includes the relative folder path when needed, for example:

```text
909/kick.wav
```

### Serial

Boot reports both catalog ownership and dynamic internal-heap cost:

```text
[SAMPLER-REGISTRY] heap phase=before-scan free8=... largest8=...
[SAMPLER-REGISTRY] heap phase=after-scan free8=... largest8=...
[SAMPLER-REGISTRY] heap phase=after-bind free8=... largest8=...
[SAMPLER-REGISTRY] ready discovered=... registered=... stableReject=... legacyReject=... storeReject=...
```

For production acceptance, capture these four lines with the small smoke library and again with a realistic library (preferably 20–50 WAV if that matches actual use). Do not infer a threshold from file count alone: reject the candidate if registry construction causes reset/WDT, heap corruption, systematic audio underruns, or leaves insufficient heap for the normal sampler/Scene smoke.

Browsing itself should not print repeated directory scans because the browser uses the boot-built in-memory catalog.

A failed WAV preload must leave the previous pad assignment unchanged.

## Troubleshooting

### Folder does not appear

A folder is shown only when the recursively indexed catalog contains at least one visible WAV somewhere below it. Hidden directories and non-WAV-only directories are intentionally omitted.

### Nested WAV does not appear

Check that it is no deeper than 8 directory levels below `/samples` and that the extension is `.wav` (case-insensitive).

### Two `kick.wav` files behave strangely in old Scenes

Duplicate basenames are ambiguous to the historical 32-bit basename ID. The current stable path identity/runtime mapping keeps newly indexed files distinct, while basename-only legacy lookup and an old Scene containing only the ambiguous legacy ID fail closed rather than selecting one arbitrarily.

### Large library reduces free heap

Compare `before-scan`, `after-scan`, and `after-bind`. `after-scan` is the recursive `SampleIndex` cost; the additional drop to `after-bind` is the Store path registry cost. Capture those values with the exact file count and average path shape. Allocator-policy redesign remains outside 0.9.5-A; do not increase the 32 KiB PCM pool to compensate.

### Browser navigation causes reset/WDT

Reject the candidate. Browsing is memory-only after boot and must not perform SD scans, WAV decode or audio-thread filesystem work.

## Acceptance checklist

- [ ] exact candidate SHA flashed;
- [ ] `/samples/root.wav` is visible at browser root;
- [ ] `/samples/909/kick.wav` is visible after entering `909/`;
- [ ] `/samples/909/hats/closed.wav` is visible after entering nested `hats/`;
- [ ] Backspace/Left returns to parent directory;
- [ ] Esc closes browser without changing current pad;
- [ ] selecting a valid nested WAV assigns and plays it;
- [ ] selecting a malformed nested WAV leaves previous pad playable;
- [ ] `909/kick.wav` and `SP12/kick.wav` are independently selectable;
- [ ] duplicate basename legacy lookup fails closed rather than last-write-wins;
- [ ] quick Left/Right on `SAMPLE:` cycles only files in that folder;
- [ ] Q/W/E/R/T/Y/U/I triggers still work after browser use;
- [ ] Save/reboot/Load restores a nested selected loose sample through stable path identity;
- [ ] registry `before-scan/after-scan/after-bind` heap lines captured;
- [ ] realistic-library boot smoke passes without WDT/reset/heap corruption;
- [ ] no browser navigation causes WDT/reset;
- [ ] fixed DRAM gate remains green;
- [ ] sampler pool remains 32 KiB;
- [ ] no `/kits` canonical model, LOAD KIT or RELINK implementation is introduced by this change.
