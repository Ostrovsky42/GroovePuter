#!/usr/bin/env python3
"""Build a deterministic, unpublished Cardputer ADV firmware handoff bundle."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import mmap
import re
import shutil
import subprocess
import zipfile
from zipfile import BadZipFile
from pathlib import Path


APP_NAME = "GroovePuter.ino.bin"
MERGED_NAME = "GroovePuter.ino.merged.bin"
DRAM_BUDGET_DEFAULT = 191_488
REPOSITORY = "https://github.com/Ostrovsky42/GroovePuter"
ROLLBACK_FILES = (
    "GroovePuter.ino.bin",
    "GroovePuter.ino.merged.bin",
    "GroovePuter.ino.bootloader.bin",
    "GroovePuter.ino.partitions.bin",
)
ROLLBACK_OFFSETS = {
    "GroovePuter.ino.bin": "0x10000",
    "GroovePuter.ino.merged.bin": "0x0",
    "GroovePuter.ino.bootloader.bin": "0x1000",
    "GroovePuter.ino.partitions.bin": "0x8000",
}


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def file_contains(path: Path, value: bytes) -> bool:
    with path.open("rb") as source:
        with mmap.mmap(source.fileno(), 0, access=mmap.ACCESS_READ) as contents:
            return contents.find(value) >= 0


def validate_clean_source(repository_root: Path, source_sha: str) -> None:
    repository_root = repository_root.resolve()
    head = subprocess.run(
        ["git", "rev-parse", "--verify", "HEAD"],
        cwd=repository_root,
        capture_output=True,
        text=True,
        check=False,
    )
    if head.returncode != 0:
        raise ValueError(f"cannot read source HEAD in {repository_root}")
    if head.stdout.strip().lower() != source_sha.lower():
        raise ValueError("supplied source SHA does not match HEAD")

    status = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=normal"],
        cwd=repository_root,
        capture_output=True,
        text=True,
        check=False,
    )
    if status.returncode != 0:
        raise ValueError(f"cannot read source status in {repository_root}")
    if status.stdout.strip():
        raise ValueError("source tree is dirty")


def add_rollback_bundle(
    archive_path: Path, expected_sha256: str, output_dir: Path, fqbn: str
) -> dict:
    actual_sha256 = sha256(archive_path)
    if not re.fullmatch(r"[0-9a-fA-F]{64}", expected_sha256):
        raise ValueError("rollback archive SHA-256 must be 64 hex digits")
    if actual_sha256.lower() != expected_sha256.lower():
        raise ValueError("rollback archive SHA-256 does not match supplied value")

    output = output_dir / "rollback"
    output.mkdir()
    try:
        with zipfile.ZipFile(archive_path) as source:
            names = source.namelist()
            acceptance_names = [
                name for name in names
                if name.endswith("/HARDWARE-ACCEPTANCE.txt")
            ]
            provenance_names = [
                name for name in names if name.endswith("/product/provenance.txt")
            ]
            checksum_names = [
                name for name in names if name.endswith("/product/SHA256SUMS.txt")
            ]
            if not (len(acceptance_names) == len(provenance_names) ==
                    len(checksum_names) == 1):
                raise ValueError("rollback archive lacks unique acceptance/provenance/checksum records")

            acceptance = source.read(acceptance_names[0]).decode("utf-8")
            if not re.search(r"(?i)owner.{0,80}(?:PASS|ACCEPTED)|PASS.{0,80}owner", acceptance):
                raise ValueError("rollback archive has no owner hardware acceptance PASS")
            provenance = source.read(provenance_names[0]).decode("utf-8")
            provenance_values = dict(
                line.split("=", 1) for line in provenance.splitlines() if "=" in line
            )
            source_commit = provenance_values.get("source_commit", "")
            if not re.fullmatch(r"[0-9a-fA-F]{40,64}", source_commit):
                raise ValueError("rollback provenance has no full source commit SHA")
            if provenance_values.get("fqbn") != fqbn:
                raise ValueError("rollback FQBN does not match candidate FQBN")

            checksum_text = source.read(checksum_names[0]).decode("ascii")
            checksums = {}
            for line in checksum_text.splitlines():
                match = re.fullmatch(r"([0-9a-fA-F]{64})\s+\*?(.+)", line.strip())
                if not match:
                    raise ValueError("rollback checksum manifest has an invalid line")
                digest, filename = match.groups()
                if filename in checksums:
                    raise ValueError("rollback checksum manifest has duplicate paths")
                checksums[filename] = digest.lower()

            selected = {}
            for filename in ROLLBACK_FILES:
                if filename not in checksums:
                    raise ValueError(f"rollback checksum manifest omits {filename}")
                matches = [
                    name for name in names
                    if name.endswith("/product/" + filename)
                ]
                if len(matches) != 1:
                    raise ValueError(f"rollback archive must contain one product/{filename}")
                info = source.getinfo(matches[0])
                if info.file_size <= 0:
                    raise ValueError(f"rollback artifact is empty: {filename}")
                digest = hashlib.sha256()
                destination = output / filename
                with source.open(info) as stream, destination.open("wb") as target:
                    for block in iter(lambda: stream.read(1024 * 1024), b""):
                        digest.update(block)
                        target.write(block)
                if digest.hexdigest() != checksums[filename]:
                    raise ValueError(f"rollback artifact checksum mismatch: {filename}")

            (output / "provenance.txt").write_text(provenance, encoding="utf-8")
            (output / "HARDWARE-ACCEPTANCE.txt").write_text(
                acceptance, encoding="utf-8"
            )
    except (BadZipFile, KeyError, UnicodeDecodeError) as error:
        raise ValueError(f"invalid rollback archive: {error}") from error

    instructions = (
        "# Restore the accepted 0.9.16 baseline\n\n"
        "This bundle contains the product image from source commit "
        f"`{source_commit.lower()}`. The recorded owner PASS was for its companion "
        "USB diagnostic image. The same-source, same-FQBN product image was later "
        "flashed to that Cardputer and the board re-enumerated; no per-step product "
        "telemetry was supplied.\n\n"
        "For a full restore, power off the Cardputer ADV, hold G0 while powering "
        "on, then release G0 to enter download mode. Run `python -m esptool "
        "--chip esp32s3 --port <PORT> write_flash 0x0 "
        "GroovePuter.ino.merged.bin` with the serial port substituted for `<PORT>`. "
        "This overwrites the currently installed firmware and flash layout. Back "
        "up any data stored in flash first. The app-only file may be written at "
        "`0x10000` only when the existing bootloader and partition table match "
        "this image.\n\n"
        "Before flashing, verify `SHA256SUMS.txt` in this directory. No rollback "
        "image has been flashed as part of candidate preparation.\n"
    )
    (output / "ROLLBACK.md").write_text(instructions, encoding="utf-8")
    checksum_text = "".join(
        f"{sha256(path)}  {path.name}\n"
        for path in sorted(output.iterdir())
        if path.is_file() and path.name != "SHA256SUMS.txt"
    )
    (output / "SHA256SUMS.txt").write_text(checksum_text, encoding="utf-8")
    return {
        "source_commit": source_commit.lower(),
        "archive_sha256": actual_sha256,
        "owner_hardware_acceptance": (
            "DIAGNOSTIC_IMAGE_OWNER_PASS; PRODUCT_IMAGE_SAME_SOURCE_FQBN_"
            "SUBSEQUENTLY_FLASHED_AND_REENUMERATED; NO_PER_STEP_TELEMETRY"
        ),
        "artifacts": list(ROLLBACK_FILES),
    }


def parse_build_report(path: Path) -> dict[str, int]:
    text = path.read_text(encoding="utf-8", errors="replace")
    program = re.search(r"Sketch uses ([\d,]+) bytes", text)
    memory = re.search(
        r"Global variables use ([\d,]+) bytes.*?leaving ([\d,]+) bytes",
        text,
    )
    if not program or not memory:
        raise ValueError(f"build log lacks program/DRAM summary: {path}")
    return {
        "program_storage_bytes": int(program.group(1).replace(",", "")),
        "static_dram_bytes": int(memory.group(1).replace(",", "")),
        "dynamic_memory_available_bytes": int(memory.group(2).replace(",", "")),
    }


def parse_partition(path: Path, name: str) -> tuple[int, int]:
    with path.open("r", encoding="utf-8", newline="") as source:
        for row in csv.reader(source):
            if not row or row[0].lstrip().startswith("#"):
                continue
            fields = [field.strip() for field in row]
            if fields[0] == name:
                if len(fields) < 5:
                    break
                return int(fields[3], 0), int(fields[4], 0)
    raise ValueError(f"partition {name!r} missing from {path}")


def property_value(text: str, key: str) -> str:
    match = re.search(rf"(?m)^{re.escape(key)}\s*=\s*(\S+)\s*$", text)
    if not match:
        raise ValueError(f"platform properties missing {key}")
    return match.group(1)


def flash_layout(build_dir: Path, platform_txt: Path) -> dict:
    platform = platform_txt.read_text(encoding="utf-8", errors="replace")
    sdkconfig = (build_dir / ".arduino-build/sdkconfig").read_text(
        encoding="utf-8", errors="replace"
    )
    recipe = next(
        (line for line in platform.splitlines() if "merge-bin" in line), None
    )
    if recipe is None:
        raise ValueError("platform recipe has no merged-image flash layout")

    bootloader_offset = int(property_value(platform, "build.bootloader_addr"), 0)
    partition_match = re.search(
        r"(?m)^CONFIG_PARTITION_TABLE_OFFSET=(0x[0-9a-fA-F]+)", sdkconfig
    )
    if not partition_match:
        raise ValueError("generated sdkconfig has no partition-table offset")
    partition_offset = int(partition_match.group(1), 0)

    boot_app_match = re.search(
        r"\s+(0x[0-9a-fA-F]+)\s+\S*boot_app0\.bin", recipe
    )
    partition_recipe_match = re.search(
        r'\s+(0x[0-9a-fA-F]+)\s+"\{build\.path\}/'
        r'\{build\.project_name\}\.partitions\.bin"', recipe
    )
    app_match = re.search(
        r'\s+(0x[0-9a-fA-F]+)\s+"\{build\.path\}/\{build\.project_name\}\.bin"',
        recipe,
    )
    if not boot_app_match or not partition_recipe_match or not app_match:
        raise ValueError("could not read component offsets from merge recipe")
    boot_app_offset = int(boot_app_match.group(1), 0)
    recipe_partition_offset = int(partition_recipe_match.group(1), 0)
    recipe_app_offset = int(app_match.group(1), 0)
    if partition_offset != recipe_partition_offset:
        raise ValueError(
            f"partition-table offset {partition_offset:#x} disagrees with flash "
            f"recipe {recipe_partition_offset:#x}"
        )
    app_offset, app_size = parse_partition(
        build_dir / ".arduino-build/partitions.csv", "app0"
    )
    if recipe_app_offset != app_offset:
        raise ValueError(
            f"app partition offset {app_offset:#x} disagrees with flash recipe "
            f"{recipe_app_offset:#x}"
        )
    flash_size = property_value(platform, "build.flash_size")
    size_match = re.fullmatch(r"(\d+)([KMG]B)", flash_size, re.IGNORECASE)
    if not size_match:
        raise ValueError(f"unsupported build.flash_size value: {flash_size}")
    scale = {"KB": 1024, "MB": 1024**2, "GB": 1024**3}
    flash_bytes = int(size_match.group(1)) * scale[size_match.group(2).upper()]
    return {
        "device_flash_bytes": flash_bytes,
        "app_partition": {"offset": f"0x{app_offset:x}", "size_bytes": app_size},
        "merged_image": {"offset": "0x0", "size_bytes": flash_bytes},
        "components": [
            {"role": "bootloader", "offset": f"0x{bootloader_offset:x}"},
            {"role": "partition_table", "offset": f"0x{partition_offset:x}"},
            {"role": "boot_app0", "offset": f"0x{boot_app_offset:x}"},
            {"role": "application", "offset": f"0x{app_offset:x}"},
        ],
    }


def record_artifact(root: Path, path: Path, role: str,
                    address: str | None = None) -> dict:
    record = {
        "path": path.relative_to(root).as_posix(),
        "role": role,
        "size_bytes": path.stat().st_size,
        "sha256": sha256(path),
    }
    if address is not None:
        record["flash_address"] = address
    return record


def make_zip(root: Path, zip_path: Path) -> None:
    with zipfile.ZipFile(
        zip_path, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9
    ) as archive:
        for path in sorted(p for p in root.rglob("*") if p.is_file()):
            info = zipfile.ZipInfo(path.relative_to(root).as_posix())
            info.date_time = (1980, 1, 1, 0, 0, 0)
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = 0o100644 << 16
            archive.writestr(info, path.read_bytes(), compress_type=zipfile.ZIP_DEFLATED,
                             compresslevel=9)


def prepare_candidate_package(
    *, build_dir: Path, output_dir: Path, version: str, source_sha: str,
    fqbn: str, core_version: str, arduino_cli_version: str,
    esp_idf_commit: str, platform_txt: Path, boot_app0: Path,
    build_log: Path, dram_budget: int = DRAM_BUDGET_DEFAULT,
    require_clean_source: bool = False, repository_root: Path | None = None,
    rollback_archive: Path | None = None, rollback_sha256: str | None = None,
) -> tuple[Path, Path]:
    build_dir = build_dir.resolve()
    platform_txt = platform_txt.resolve()
    boot_app0 = boot_app0.resolve()
    build_log = build_log.resolve()
    output_dir = output_dir.resolve()
    if rollback_archive is not None:
        rollback_archive = rollback_archive.resolve()

    if not re.fullmatch(
        r"\d+\.\d+\.\d+(?:-[A-Za-z0-9.-]+)?(?:\+[A-Za-z0-9.-]+)?",
        version,
    ):
        raise ValueError(f"invalid semantic version: {version}")
    if not re.fullmatch(r"(?:[0-9a-fA-F]{40}|[0-9a-fA-F]{64})", source_sha):
        raise ValueError("source SHA must be a full 40- or 64-digit hex SHA")
    if dram_budget <= 0:
        raise ValueError("DRAM budget must be positive")
    if output_dir.exists():
        raise ValueError(f"output directory already exists: {output_dir}")
    if require_clean_source and rollback_archive is None:
        raise ValueError("clean-source candidate package requires a verified rollback archive")
    if (rollback_archive is None) != (rollback_sha256 is None):
        raise ValueError("rollback archive and expected SHA-256 must be supplied together")
    source_state = "NOT_CHECKED"
    if require_clean_source:
        validate_clean_source(
            repository_root or Path(__file__).resolve().parents[1], source_sha
        )
        source_state = "CLEAN"

    inputs = {
        "app": build_dir / APP_NAME,
        "merged": build_dir / MERGED_NAME,
        "bootloader": build_dir / "GroovePuter.ino.bootloader.bin",
        "partitions": build_dir / "GroovePuter.ino.partitions.bin",
        "elf": build_dir / "GroovePuter.ino.elf",
        "map": build_dir / "GroovePuter.ino.map",
    }
    for label, path in {**inputs, "boot_app0": boot_app0,
                        "platform": platform_txt, "build_log": build_log}.items():
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"missing/empty {label} input: {path}")

    if (not file_contains(inputs["elf"], version.encode("utf-8"))
            or not file_contains(inputs["elf"], source_sha.lower().encode("ascii"))):
        raise ValueError("ELF build identity does not match manifest version/source SHA")

    layout = flash_layout(build_dir, platform_txt)
    if inputs["app"].stat().st_size > layout["app_partition"]["size_bytes"]:
        raise ValueError("application image exceeds the generated app partition")
    if inputs["merged"].stat().st_size != layout["merged_image"]["size_bytes"]:
        raise ValueError("merged image size disagrees with platform flash size")
    component_offsets = [int(item["offset"], 16) for item in layout["components"]]
    component_sizes = [
        inputs["bootloader"].stat().st_size,
        inputs["partitions"].stat().st_size,
        boot_app0.stat().st_size,
        inputs["app"].stat().st_size,
    ]
    for index in range(len(component_offsets) - 1):
        if component_offsets[index] + component_sizes[index] > component_offsets[index + 1]:
            raise ValueError("firmware components overlap in the flash recipe")
    if inputs["partitions"].stat().st_size > 0x1000:
        raise ValueError("partition table image exceeds its flash sector")

    report = parse_build_report(build_log)
    if report["static_dram_bytes"] > dram_budget:
        raise ValueError("static DRAM exceeds the configured candidate budget")

    output_dir.mkdir(parents=True)
    artifacts_dir = output_dir / "artifacts"
    launcher_dir = output_dir / "m5launcher"
    burner_root = output_dir / "m5burner-dry-run"
    burner_firmware = burner_root / "firmware"
    for directory in (artifacts_dir, launcher_dir, burner_firmware):
        directory.mkdir(parents=True, exist_ok=True)

    rollback_manifest = None
    if rollback_archive is not None and rollback_sha256 is not None:
        try:
            rollback_manifest = add_rollback_bundle(
                rollback_archive, rollback_sha256, output_dir, fqbn
            )
        except Exception:
            shutil.rmtree(output_dir, ignore_errors=True)
            raise

    shutil.copyfile(inputs["app"], artifacts_dir / APP_NAME)
    shutil.copyfile(inputs["merged"], artifacts_dir / MERGED_NAME)
    shutil.copyfile(inputs["app"], launcher_dir / APP_NAME)
    component_sources = [
        (inputs["bootloader"], "bootloader_" + layout["components"][0]["offset"] + ".bin"),
        (inputs["partitions"], "partitions_" + layout["components"][1]["offset"] + ".bin"),
        (boot_app0, "boot_app0_" + layout["components"][2]["offset"] + ".bin"),
        (inputs["app"], "GroovePuter_" + layout["components"][3]["offset"] + ".bin"),
    ]
    for source, filename in component_sources:
        shutil.copyfile(source, burner_firmware / filename)

    burner_metadata = {
        "name": "GroovePuter",
        "description": (
            f"GroovePuter {version} Cardputer ADV candidate; software build only."
        ),
        "keywords": "GroovePuter, music, MIDI, Cardputer ADV",
        "author": "Ostrovsky42",
        "repository": REPOSITORY,
        "firmware_category": [{
            "Cardputer ADV": {
                "path": "firmware",
                "device": ["M5Stack Cardputer ADV"],
                "default_baud": 921600,
            }
        }],
        "version": version,
        "framework": "Arduino",
    }
    (burner_root / "m5burner.json").write_text(
        json.dumps(burner_metadata, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    (burner_root / "README.md").write_text(
        "# M5Burner dry-run package\n\n"
        "This directory uses the documented legacy M5Stack firmware repository "
        "layout: `m5burner.json` plus a `firmware/` directory. Its filename/address "
        "pairs come from the installed M5Stack ESP32 core flash recipe. The package "
        "was structurally checked only; it has not been opened in M5Burner, submitted, "
        "or tested in the newer Web Burner. Do not publish it as compatible.\n\n"
        "The device string in the category is a proposed label, not a verified catalog ID.\n",
        encoding="utf-8",
    )

    report["dram_budget_bytes"] = dram_budget
    report["dram_headroom_bytes"] = dram_budget - report["static_dram_bytes"]
    (output_dir / "resource-report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    manifest = {
        "schema_version": 1,
        "status": "UNPUBLISHED_CANDIDATE; HARDWARE_UNVERIFIED",
        "version": version,
        "source": {"commit": source_sha.lower(), "working_tree": source_state},
        "build": {
            "profile": "Cardputer ADV FS1B dynamic FatFs",
            "fqbn": fqbn,
            "arduino_cli": arduino_cli_version,
            "m5stack_esp32_core": core_version,
            "esp_idf_commit": esp_idf_commit,
        },
        "memory": report,
        "flash": layout,
        "compatibility": {
            "m5launcher": "HARDWARE_UNVERIFIED",
            "m5burner_legacy": "STRUCTURE_ONLY_DRY_RUN",
            "m5burner_web": "NOT_TESTED",
        },
        "rollback": rollback_manifest,
        "artifacts": [],
    }
    manifest["artifacts"].append(record_artifact(
        output_dir, artifacts_dir / APP_NAME, "app-only; requires matching partition table",
        layout["app_partition"]["offset"],
    ))
    manifest["artifacts"].append(record_artifact(
        output_dir, artifacts_dir / MERGED_NAME, "merged full image",
        layout["merged_image"]["offset"],
    ))
    manifest["artifacts"].append(record_artifact(
        output_dir, launcher_dir / APP_NAME,
        "M5Launcher SD app payload; install compatibility unverified",
        layout["app_partition"]["offset"],
    ))
    for _, filename in component_sources:
        component = next(
            item for item in layout["components"]
            if filename.endswith(item["offset"] + ".bin")
        )
        manifest["artifacts"].append(record_artifact(
            output_dir, burner_firmware / filename,
            "M5Burner dry-run " + component["role"], component["offset"],
        ))
    if rollback_manifest is not None:
        for filename in ROLLBACK_FILES:
            manifest["artifacts"].append(record_artifact(
                output_dir,
                output_dir / "rollback" / filename,
                "verified rollback image",
                ROLLBACK_OFFSETS[filename],
            ))
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    (output_dir / "README.md").write_text(
        f"# GroovePuter {version} candidate package\n\n"
        f"Source commit: `{source_sha.lower()}`. This is an unpublished, "
        "software-validated package. Hardware, physical SD behavior, M5Launcher "
        "installation and external MIDI remain unverified.\n\n"
        "- `artifacts/GroovePuter.ino.bin`: app-only image for app offset "
        f"`{layout['app_partition']['offset']}`; bootloader and partition table "
        "must already match this build.\n"
        "- `artifacts/GroovePuter.ino.merged.bin`: full merged image for offset `0x0`; "
        "contains bootloader, partition table, boot_app0 and app.\n"
        "- `m5launcher/`: app-only payload. Compatibility is not hardware-accepted.\n"
        "- `m5burner-dry-run/`: legacy manifest/firmware structure only; not submitted "
        "or tested in M5Burner.\n\n"
        "- `rollback/`: checksum-verified 0.9.16 product images and owner acceptance "
        "record for recovery; this image has not been flashed during packaging.\n\n"
        "See `manifest.json`, `resource-report.json` and `SHA256SUMS.txt` for "
        "source, profile, addresses, resource numbers and checksums.\n",
        encoding="utf-8",
    )

    files = sorted(path for path in output_dir.rglob("*") if path.is_file())
    checksum_text = "".join(
        f"{sha256(path)}  {path.relative_to(output_dir).as_posix()}\n"
        for path in files
    )
    (output_dir / "SHA256SUMS.txt").write_text(checksum_text, encoding="utf-8")

    zip_path = output_dir.parent / f"{output_dir.name}.zip"
    make_zip(output_dir, zip_path)
    (zip_path.parent / f"{zip_path.name}.sha256").write_text(
        f"{sha256(zip_path)}  {zip_path.name}\n", encoding="utf-8"
    )
    return output_dir, zip_path


def cli() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--build-log", type=Path, required=True)
    parser.add_argument("--platform-txt", type=Path, required=True)
    parser.add_argument("--boot-app0", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--source-sha", required=True)
    parser.add_argument("--fqbn", required=True)
    parser.add_argument("--core-version", required=True)
    parser.add_argument("--arduino-cli-version", required=True)
    parser.add_argument("--esp-idf-commit", required=True)
    parser.add_argument("--dram-budget", type=int, default=DRAM_BUDGET_DEFAULT)
    parser.add_argument("--require-clean-source", action="store_true")
    parser.add_argument("--repository-root", type=Path)
    parser.add_argument("--rollback-archive", type=Path)
    parser.add_argument("--rollback-sha256")
    args = parser.parse_args()

    output, archive = prepare_candidate_package(
        build_dir=args.build_dir,
        output_dir=args.output_dir,
        version=args.version,
        source_sha=args.source_sha,
        fqbn=args.fqbn,
        core_version=args.core_version,
        arduino_cli_version=args.arduino_cli_version,
        esp_idf_commit=args.esp_idf_commit,
        platform_txt=args.platform_txt,
        boot_app0=args.boot_app0,
        build_log=args.build_log,
        dram_budget=args.dram_budget,
        require_clean_source=args.require_clean_source,
        repository_root=args.repository_root,
        rollback_archive=args.rollback_archive,
        rollback_sha256=args.rollback_sha256,
    )
    print(f"Candidate package directory: {output}")
    print(f"Reproducible archive: {archive}")
    print("No release, upload, M5Burner submission, or hardware action was performed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(cli())
