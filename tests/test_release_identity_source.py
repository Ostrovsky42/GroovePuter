#!/usr/bin/env python3
"""Keep diagnostic and product release images on the accepted USB profile."""

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RELEASE = (ROOT / ".github/workflows/release.yml").read_text(encoding="utf-8")
ACCEPTANCE = (ROOT / "scripts/ci/build_usb_acceptance_package.sh").read_text(
    encoding="utf-8"
)

EXPECTED_FQBN = (
    "m5stack:esp32:m5stack_cardputer:PSRAM=disabled,PartitionScheme=huge_app,"
    "USBMode=default,CDCOnBoot=default,UploadMode=cdc"
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise SystemExit(message)


release_match = re.search(r"(?m)^\s*FQBN:\s*['\"]?([^'\"\n]+)", RELEASE)
if release_match is None:
    # Also accept the workflow's environment assignment form.
    release_match = re.search(r"(?m)^\s*FQBN:\s*([^\n]+)", RELEASE)
require(release_match is not None, "Release workflow must declare its product FQBN")
require(
    EXPECTED_FQBN in release_match.group(1),
    "Release product must use the CDCOnBoot=default USB-role profile",
)
require(
    "CDCOnBoot=default" in ACCEPTANCE and EXPECTED_FQBN in ACCEPTANCE,
    "USB diagnostic image and product image must use the same FQBN",
)
require(
    "GROOVEPUTER_USB_ACCEPT_DIAG" not in RELEASE,
    "Release product build must not enable USB acceptance diagnostics",
)
require(
    'GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS: ""' in RELEASE,
    "Release product build must explicitly clear diagnostic compiler flags",
)
require(
    'GROOVEPUTER_BUILD_EXTRA_CPP_FLAGS="-DGROOVEPUTER_USB_ACCEPT_DIAG"'
    in ACCEPTANCE,
    "USB acceptance image must enable only the USB diagnostics flag",
)
require(
    "USB acceptance diagnostic marker leaked into product image" in RELEASE,
    "Release workflow must prove diagnostic markers are absent from product ELF",
)
for field in ("fqbn", "extra_cpp_flags", "fatfs", "diagnostics"):
    require(
        f"{field}=" in RELEASE,
        f"Release provenance must record {field}",
    )

print("Release product identity and USB profile: PASS")
