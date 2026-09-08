#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    spec_path = ROOT / "docs/contracts/0_9_10_LIFETIME_R1_RESOURCE_OWNERSHIP.md"
    diag_path = ROOT / "src/diag/lifetime_census.h"
    sketch_path = ROOT / "GroovePuter.ino"
    smf_path = ROOT / "src/platform/cardputer_smf_player.cpp"

    require(spec_path.exists(), "LIFETIME-R1 ownership contract must exist")
    spec = spec_path.read_text(encoding="utf-8")
    require("Optimize lifetime before capacity" in spec,
            "contract must freeze the lifetime-before-capacity invariant")
    require("SMF post-use residency" in spec,
            "L1 must record the first proven post-use residency defect")

    sketch = sketch_path.read_text(encoding="utf-8")
    smf = smf_path.read_text(encoding="utf-8")

    require(re.search(
        r'xTaskCreatePinnedToCore\(\s*audioTask\s*,\s*"AudioTask"\s*,\s*8192',
        sketch) is not None,
        "L1 baseline must keep the accepted 8192-byte AudioTask stack")
    require("constexpr uint32_t kPlayerTaskStack = 6144;" in smf,
            "L1 baseline must keep the measured 6144-byte SMF task stack")
    require("SMF runtime deferred" in sketch,
            "MEMORY-R1 boot-lazy SMF residency must remain inherited")

    require(diag_path.exists(),
            "RED: LIFETIME-R1 requires src/diag/lifetime_census.h")
    diag = diag_path.read_text(encoding="utf-8")

    require("#define GROOVEPUTER_DIAG_LIFETIME_CENSUS 0" in diag,
            "lifetime census must be default-off on production hardware")
    require("heap_caps_get_free_size" in diag and "MALLOC_CAP_INTERNAL" in diag,
            "census must report free internal heap")
    require("heap_caps_get_largest_free_block" in diag,
            "census must report largest internal block")
    require("heap_caps_get_minimum_free_size" in diag,
            "census must report minimum-ever internal heap")
    require("uxTaskGetStackHighWaterMark" in diag,
            "census must expose raw native FreeRTOS task HWM")
    require("taskHwmNative" in diag,
            "HWM label must remain unit-honest until target semantics are verified")
    require("allocationCount" not in diag and "allocCount" not in diag,
            "L1 must not invent an allocation-count proxy")
    require("LIFETIME_CENSUS_POINT" in diag,
            "diagnostic must expose one bounded compile-gated observation macro")
    require("do { } while (0)" in diag,
            "disabled census macro must compile to an explicit no-op")

    require('LIFETIME_CENSUS_POINT("BOOT", "product-ready")' in sketch,
            "L1 needs a product-ready heap plateau observation point")
    require('LIFETIME_CENSUS_POINT("SMF_PLAY", "worker-start")' in smf,
            "L1 needs an SMF worker-start observation point")
    require('LIFETIME_CENSUS_POINT("SMF_PLAY", "after-create")' in smf,
            "L1 needs an SMF post-create observation point")

    print("LIFETIME-R1 L1 contract PASS")


if __name__ == "__main__":
    main()
