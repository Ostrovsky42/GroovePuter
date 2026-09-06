# Cardputer recovery diagnostics review — 2026-09-05

Companion to
[SD_MOUNT_UNAVAILABLE_2026-09-05.md](SD_MOUNT_UNAVAILABLE_2026-09-05.md). That
document's "Source review and controlled USB capture" section pointed here;
this session ran out of budget before writing it. Reviewed and corrected in a
second session, against the actual installed library sources rather than
inference.

## One claim disproven

The working theory behind the early-return added to
`ensureCardputerSdMounted()` was that `SDFS::begin()` (`SD.cpp`) calls
`spi.begin()` internally, and that this second call would reset the SPI bus to
ESP32 default pins, discarding the explicit Cardputer ADV pin assignment
(`kSdClockPin`/`kSdMisoPin`/`kSdMosiPin`/`kSdChipSelectPin`) made just before
it. That would have explained every `CARD_NONE` this session as a real,
fixable bug.

It does not hold. `SPIClass::begin()`
(`libraries/SPI/src/SPI.cpp:66`) opens with:

```cpp
bool SPIClass::begin(int8_t sck, int8_t miso, int8_t mosi, int8_t ss) {
  if (_spi) {
    return true;
  }
  ...
```

Once the bus has been started once — which the explicit custom-pin call does
— every later call, args or not, hits this guard and returns immediately
without touching pin state. `SDFS::begin()`'s internal `spi.begin()` is a
no-op here. The early-return code that resulted from this theory is harmless
(it only distinguishes, in the log, "our explicit init failed" from "SD.begin()
itself failed," one stage sooner) but its comment claimed a fix that isn't
one; corrected in `cardputer_sd.cpp` to say what the change actually does.

## One claim confirmed

Separately, and independent of the disproven theory:
`SDFS::begin()` (`SD.cpp:29`) resets `_pdrv` to `0xFF` — the same state as
"never mounted" — whenever `sdcard_mount()` fails, not only when
`sdcard_init()` (the hardware/protocol layer) fails:

```cpp
_pdrv = sdcard_init(ssPin, &spi, frequency);
if (_pdrv == 0xFF) return false;
if (!sdcard_mount(_pdrv, mountpoint, max_files, format_if_empty)) {
    sdcard_unmount(_pdrv);
    sdcard_uninit(_pdrv);
    _pdrv = 0xFF;              // same value as "card never talked at all"
    return false;
}
```

`SD.cardType()` reads `_pdrv == 0xFF ? CARD_NONE : ...`. So `CARD_NONE` in
every capture this session could mean either "the card never answered on
SPI" or "the card answered fine, but its filesystem failed to mount"
(unformatted, wrong FAT type, corrupted table) — the two are indistinguishable
from this log line alone. This reframes the open SD question: it may not be a
detection problem at all.

## What is still open

Neither theory identifies why every mount attempt this session returned
`CARD_NONE`, including the one genuine `ESP_RST_POWERON` boot. The recovery
diagnostic build (`scripts/build_cardputer_recovery_diagnostics.sh`) exists to
get past this: a normal CDC image with `CORE_DEBUG_LEVEL` raised, so the
underlying FatFs/`sdcard_mount()` driver logs its actual error rather than the
caller only seeing a boolean. That capture has not been taken.

## What this review changed

- The false comment in `cardputer_sd.cpp` is corrected; the (harmless)
  early-return diagnostic stays.
- The true `_pdrv`/`CARD_NONE` conflation is kept in
  `SD_MOUNT_UNAVAILABLE_2026-09-05.md` as originally written — verified, not
  retracted.
- This file replaces the dangling link with what was actually checked, so the
  next SD session starts from the recovery-diagnostic build rather than
  re-deriving these two facts.

## Unrelated to SD, verified in the same review

The same working tree carried a compile-time move of the Stage 12 phrase
evolution catalog out of a first-use DRAM copy
(`src/generation/rhythm/reference_phrase_catalog_data.h`,
`reference_vocabulary.cpp`). Measured directly, not taken from the originating
session's own account:

```
before: 192904 B fixed DRAM, ceiling 191488, over by 1416
after:  190776 B fixed DRAM, ceiling 191488, under by 712
```

`scripts/check_cardputer_dram_budget.sh` now exits 0 on `scripts/build.sh`'s
product image. `bash tests/run_phrase_stage12_tests.sh` (the Stage 12 host
matrix, including the canonical-vs-production byte-for-byte comparison) exits
0 unchanged. This is the first time this session's P3 line has been under the
static ceiling; see `0_9_10_PATTERN_PHRASE_P3_DRAM_CHARACTERIZATION.md` for the
prior state this resolves.
