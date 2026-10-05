#!/usr/bin/env python3
"""Keep diagnostic and product release images on the accepted USB profile."""

import re
import shlex
import subprocess
import tempfile
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
    'grep -F "$marker" >/dev/null' in ACCEPTANCE and 'grep -Fq "$marker"' not in ACCEPTANCE,
    "USB acceptance marker scan must drain strings output under pipefail",
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

# Execute the generated upload example with a stub: shell expansion must pass
# the complete FQBN even when the user's environment has no FQBN variable.
flash_template = ACCEPTANCE.split('cat > "$PACKAGE_DIR/FLASH.txt" <<EOF\n', 1)[1].split('\nEOF', 1)[0]
rendered = subprocess.check_output(
    ["bash", "-c", f"FQBN_ACCEPT={shlex.quote(EXPECTED_FQBN)}\ncat <<EOF\n{flash_template}\nEOF"],
    text=True,
)
upload_line = next(line.strip() for line in rendered.splitlines() if "arduino-cli upload" in line)
arguments = subprocess.check_output(
    ["bash", "-c", 'unset FQBN; arduino-cli() { printf "%s\\n" "$@"; }; ' + upload_line],
    text=True,
).splitlines()
require(
    arguments == ["upload", "--fqbn", EXPECTED_FQBN, "-p", "/dev/ttyACM0", "--input-dir", "."],
    "Packaged flash instruction must pass the correct FQBN without relying on shell environment",
)

# Run the workflow's checksum command, then move its output as artifact download
# does. Verification must work without the original build-directory hierarchy.
hash_command = next(line.strip() for line in RELEASE.splitlines() if "sha256sum " in line)
with tempfile.TemporaryDirectory() as temporary:
    root = Path(temporary)
    build = root / "build" / "firmware"
    build.mkdir(parents=True)
    for name in ("GroovePuter.ino.elf", "GroovePuter.ino.bin", "GroovePuter.ino.merged.bin"):
        (build / name).write_bytes(name.encode())
    subprocess.run(["bash", "-ec", f"BUILD_DIR=build/firmware\n{hash_command}"], cwd=root, check=True)
    downloaded = root / "downloaded"
    build.rename(downloaded)
    subprocess.run(["sha256sum", "-c", "SHA256SUMS.txt"], cwd=downloaded, check=True)

print("Release product identity, USB profile, and portable package instructions: PASS")
