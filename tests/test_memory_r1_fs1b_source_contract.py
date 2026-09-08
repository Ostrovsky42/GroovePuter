#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
CANDIDATE = ROOT / "scripts" / "build_fatfs_dynbuffers_candidate.sh"
PRODUCT = ROOT / "scripts" / "build_cardputer_memory_r1_fs1b.sh"


def require(text: str, pattern: str, owner: str) -> None:
    if not re.search(pattern, text, re.M):
        raise AssertionError(f"{owner}: missing required contract pattern: {pattern!r}")


def reject(text: str, pattern: str, owner: str) -> None:
    if re.search(pattern, text, re.M):
        raise AssertionError(f"{owner}: forbidden contract pattern present: {pattern!r}")


def main() -> None:
    assert CANDIDATE.is_file(), "FS1B candidate builder is missing"
    assert PRODUCT.is_file(), "FS1B product build wrapper is missing"

    candidate = CANDIDATE.read_text(encoding="utf-8")
    product = PRODUCT.read_text(encoding="utf-8")

    # Framework/source identity is pinned to the exact IDF commit and the
    # production Arduino core's IDF archive family.
    require(
        candidate,
        r'COMMIT="858a988d6eb90f54661abe282c523879c6ad0116"',
        CANDIDATE.name,
    )
    require(candidate, r"idf-release_v5\.4-858a988d-v1/esp32s3", CANDIDATE.name)

    # FS1B changes only dynamic FatFs buffering. WL sector size and private
    # per-file cache policy are guards, not optimization levers in this checkpoint.
    require(candidate, r"CONFIG_FATFS_USE_DYN_BUFFERS", CANDIDATE.name)
    require(candidate, r"CONFIG_WL_SECTOR_SIZE", CANDIDATE.name)
    require(candidate, r"CONFIG_FATFS_PER_FILE_CACHE", CANDIDATE.name)
    reject(candidate, r"CONFIG_WL_SECTOR_SIZE\s*=\s*512", CANDIDATE.name)
    reject(candidate, r"CONFIG_FATFS_PER_FILE_CACHE\s*=\s*0", CANDIDATE.name)
    reject(candidate, r"FF_FS_TINY\s*=", CANDIDATE.name)

    # The product link must replace exactly the stock -lfatfs entry in a copied
    # ld_libs response file. A build.extra_libs property would add a second
    # archive and permit mixed translation units with incompatible FIL/FATFS layouts.
    require(product, r"flags/ld_libs", PRODUCT.name)
    require(product, r"-lfatfs", PRODUCT.name)
    require(product, r"compiler\.c\.elf\.libs", PRODUCT.name)
    require(product, r"libfatfs\.a", PRODUCT.name)
    reject(product, r"--build-property[^\n]*build\.extra_libs", PRODUCT.name)

    # The wrapper must retain the normal authoritative Cardputer build owner.
    require(product, r'\$\{SCRIPT_DIR\}/build\.sh', PRODUCT.name)

    print("MEMORY-R1 FS1B source contract: PASS")


if __name__ == "__main__":
    main()
