#!/usr/bin/env bash
set -euo pipefail
# FS1 candidate B: rebuild libfatfs.a from the exact ESP-IDF commit the core was
# built from, with dynamic FatFs buffers enabled. Nothing else changes, and the
# shared Arduino install is never modified -- the candidate is put ahead of the
# stock library by search path at link time only.
#
# Requires: a local esp-idf git repo containing commit 858a988d.
IDF="${IDF:-$HOME/esp-idf}"
COMMIT=858a988d
AL="$HOME/.arduino15/packages/m5stack/tools/esp32-arduino-libs/idf-release_v5.4-858a988d-v1/esp32s3"
CC="$HOME/.arduino15/packages/m5stack/tools/esp-x32/2411/bin/xtensa-esp32s3-elf-gcc"
AR="$HOME/.arduino15/packages/m5stack/tools/esp-x32/2411/bin/xtensa-esp32s3-elf-ar"
OUT="${OUT:-/tmp/fatfs-build}"

rm -rf "$OUT"; mkdir -p "$OUT/src" "$OUT/obj" "$OUT/inc"
git -C "$IDF" archive "$COMMIT" components/fatfs | tar -x -C "$OUT/src"
F="$OUT/src/components/fatfs"

cp "$AL/dio_qspi/include/sdkconfig.h" "$OUT/inc/"
echo "#define CONFIG_FATFS_USE_DYN_BUFFERS 1" >> "$OUT/inc/sdkconfig.h"

CFLAGS=$(cat "$AL/flags/c_flags")
DEFS=$(cat "$AL/flags/defines")
INC=$(cat "$AL/flags/includes")
LOCAL="-I$F/diskio -I$F/src -I$F/vfs"

# Exactly the member set of the stock archive, in the same order.
SOURCES="diskio/diskio.c diskio/diskio_rawflash.c diskio/diskio_wl.c src/ff.c
         src/ffunicode.c port/freertos/ffsystem.c diskio/diskio_sdmmc.c
         vfs/vfs_fat.c vfs/vfs_fat_sdmmc.c vfs/vfs_fat_spiflash.c"
for f in $SOURCES; do
  $CC $CFLAGS $DEFS -iprefix "$AL/include/" $INC -I"$OUT/inc" $LOCAL \
      -c "$F/$f" -o "$OUT/obj/$(basename "$f").obj"
done
( cd "$OUT/obj" && $AR rcs "$OUT/libfatfs.a" \
    diskio.c.obj diskio_rawflash.c.obj diskio_wl.c.obj ff.c.obj ffunicode.c.obj \
    ffsystem.c.obj diskio_sdmmc.c.obj vfs_fat.c.obj vfs_fat_sdmmc.c.obj \
    vfs_fat_spiflash.c.obj )

echo "candidate: $OUT/libfatfs.a"
sha256sum "$OUT/libfatfs.a"
echo "stock:"; sha256sum "$AL/lib/libfatfs.a"
echo
echo "ABI gate -- defined globals must be identical:"
diff <($AR t "$AL/lib/libfatfs.a" | sort) <($AR t "$OUT/libfatfs.a" | sort) \
  && echo "  member set: identical"
