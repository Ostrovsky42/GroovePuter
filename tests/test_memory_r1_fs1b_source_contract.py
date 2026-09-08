#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CANDIDATE = ROOT / "scripts" / "build_fatfs_dynbuffers_candidate.sh"
PRODUCT = ROOT / "scripts" / "build_cardputer_memory_r1_fs1b.sh"


def require(text: str, needle: str, owner: str) -> None:
    if needle not in text:
        raise AssertionError(f"{owner}: missing required contract fragment: {needle!r}")


def reject(text: str, needle: str, owner: str) -> None:
    if needle in text:
        raise AssertionError(f"{owner}: forbidden contract fragment present: {needle!r}")


def main() -> None:
    assert CANDIDATE.is_file(), "FS1B candidate builder is missing"
    assert PRODUCT.is_file(), "FS1B product build wrapper is missing"

    candidate = CANDIDATE.read_text(encoding="utf-8")
    product = PRODUCT.read_text(encoding="utf-8")

    # Framework/source identity is pinned to the production core provenance.
    require(candidate, "COMMIT=858a988d", CANDIDATE.name)
    require(candidate, "idf-release_v5.4-858a988d-v1/esp32s3", CANDIDATE.name)

    # FS1B changes only dynamic FatFs buffering. WL sector size and private
    # per-file cache policy are guards, not optimization levers in this checkpoint.
    require(candidate, "CONFIG_FATFS_USE_DYN_BUFFERS", CANDIDATE.name)
    require(candidate, "CONFIG_WL_SECTOR_SIZE", CANDIDATE.name)
    require(candidate, "CONFIG_FATFS_PER_FILE_CACHE", CANDIDATE.name)
    reject(candidate, "CONFIG_WL_SECTOR_SIZE=512", CANDIDATE.name)
    reject(candidate, "CONFIG_FATFS_PER_FILE_CACHE=0", CANDIDATE.name)
    reject(candidate, "FF_FS_TINY", CANDIDATE.name)

    # The product link must replace exactly the stock -lfatfs entry in a copied
    # ld_libs response file. Adding a second archive to build.extra_libs can mix
    # translation units built against incompatible FIL/FATFS layouts.
    require(product, "flags/ld_libs", PRODUCT.name)
    require(product, "-lfatfs", PRODUCT.name)
    require(product, "compiler.c.elf.libs", PRODUCT.name)
    require(product, "libfatfs.a", PRODUCT.name)
    reject(product, "build.extra_libs", PRODUCT.name)

    # The wrapper must retain the normal authoritative Cardputer build owner.
    require(product, "scripts/build.sh", PRODUCT.name)

    print("MEMORY-R1 FS1B source contract: PASS")


if __name__ == "__main__":
    main()
