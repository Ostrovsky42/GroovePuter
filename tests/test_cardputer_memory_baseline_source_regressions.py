#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

build = (ROOT / "scripts/build_cardputer_memory_baseline.sh").read_text()
instrument = (ROOT / "scripts/instrument_cardputer_memory_runtime.py").read_text()
report = (ROOT / "scripts/report_cardputer_memory_baseline.sh").read_text()
tinyusb_report = (ROOT / "scripts/report_cardputer_tinyusb_class_buffers.sh").read_text()
gate = (ROOT / "scripts/check_cardputer_dram_budget.sh").read_text()
workflow = (ROOT / ".github/workflows/cardputer-memory-baseline.yml").read_text()
baseline_doc = (ROOT / "docs/stages/CARDPUTER_MEMORY_BASELINE.md").read_text()
runtime_doc = (ROOT / "docs/stages/CARDPUTER_MEMORY_RUNTIME_TELEMETRY.md").read_text()
doc = baseline_doc + "\n" + runtime_doc
doc_lower = doc.lower()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


require('MAX_BYTES="${2:-191488}"' in gate,
        "the mandatory gate must use the provisional pre-122880 value")
require("not a universal hardware" in gate,
        "191488 must not be presented as a hardware safety limit")
require("policy: provisional exception" in gate and "items 5-7" in gate,
        "every gate run must expose the incomplete threshold-rule status")

for needle in (
    'PROVISIONAL_GATE_BYTES="${PROVISIONAL_GATE_BYTES:-191488}"',
    'UNSUPPORTED_GATE_BYTES="${UNSUPPORTED_GATE_BYTES:-122880}"',
    "MEMORY_BASELINE_IMAGE_KIND",
    "OBJDUMP_TOOL",
    '"${OBJDUMP_TOOL}" -t',
    '{".dram0.data", ".dram0.bss"}',
    "Full .dram0.bss symbol inventory",
    "bss_symbol_coverage",
    "bss_section_uncovered",
    "bss_alias_overlap",
    "candidate_bss",
    "candidate_outside",
    "provisional_status=exception_unvalidated",
    "deliberately non-gating",
    "exit 0",
):
    require(needle in report, f"static report contract missing: {needle}")

for script_name, script in (("gate", gate), ("report", report)):
    require("discover_arduino_tool" in script,
            f"{script_name} must use vendor-neutral tool discovery")
    require("ARDUINO_PACKAGES_ROOT" in script and "command -v" in script,
            f"{script_name} must support PATH and arbitrary package vendors")
    require(".arduino15/packages/esp32/tools" not in script,
            f"{script_name} must not assume the Espressif vendor directory")

for needle in (
    "mktemp -d /tmp/grooveputer-memory-baseline",
    "rsync -a --delete",
    "instrument_cardputer_memory_runtime.py",
    '"${SOURCE_ROOT}"',
    "SOURCE_COMMIT",
    "SOURCE_DIRTY",
    "ELF_SHA256",
    "sha256sum",
    'IMAGE_KIND="${1:-}"',
    "product|runtime",
    'if [[ "${IMAGE_KIND}" == "runtime" ]]',
    "Memory baseline image",
    "normal|midi-only",
):
    require(needle in build, f"build identity/isolation contract missing: {needle}")
require("git commit" not in build and "git push" not in build,
        "diagnostic builds must never mutate repository history")

for api in (
    "heap_caps_get_minimum_free_size",
    "heap_caps_get_largest_free_block",
    "heap_caps_check_integrity_all",
    "uxTaskGetStackHighWaterMark",
):
    require(api in instrument, f"runtime instrumentation missing {api}")
for field in (
    "free8",
    "largest8",
    "freeInternal8",
    "largestInternal8",
    "loopStackFreeBytes",
    "audioStackFreeBytes",
    "smfStackFreeBytes",
    "dispatchStackFreeBytes",
    "smfTaskPresent",
    "dispatchTaskPresent",
):
    require(field in instrument, f"runtime telemetry missing {field}")
require("loopStackWords" not in instrument and "audioStackWords" not in instrument,
        "ESP-IDF stack watermarks must not be labelled as words")
require("xTaskGetHandle" not in instrument,
        "task watermarks must not depend on optional task-name lookup")
require('#include "src/platform/cardputer_usb_midi_transport.h"' not in instrument,
        "runtime probe must not import TinyUSB transport headers into the sketch")
for accessor in (
    "cardputerSmfPlayerTaskHandleForMemoryBaseline",
    "cardputerMidiDispatchTaskHandleForMemoryBaseline",
    "memoryBaselineTaskHandle",
):
    require(accessor in instrument,
            f"runtime-only direct task accessor missing: {accessor}")
for source_path in (
    "cardputer_smf_player.h",
    "cardputer_smf_player_registry.h",
    "cardputer_smf_player_registry.cpp",
    "cardputer_usb_midi_transport.h",
    "cardputer_usb_midi_transport.cpp",
):
    require(source_path in instrument,
            f"runtime source patch target missing: {source_path}")

for symbol in ("ncm_epbuf", "_mscd_epbuf", "_dfu_epbuf"):
    require(symbol in tinyusb_report,
            f"TinyUSB provenance report missing {symbol}")
require("CFG_TUD_" in tinyusb_report and "CONFIG_TINYUSB_" in tinyusb_report,
        "TinyUSB report must inspect compile-time class controls")
require("Link-map evidence" in tinyusb_report,
        "TinyUSB report must identify contributing objects through linker maps")
require("observational" in tinyusb_report and "does not disable" in tinyusb_report,
        "PR #70 must not disable USB classes without evidence")
require("find \"${PACKAGE_ROOT}\" -type f -name '*.a'" not in tinyusb_report,
        "TinyUSB reporting must not scan every archive in the package tree")
require("report_cardputer_tinyusb_class_buffers.sh" in build,
        "product builds must emit TinyUSB provenance")
for evidence in (
    "libarduino_tinyusb.a(msc_device.c.obj)",
    "libarduino_tinyusb.a(dfu_device.c.obj)",
    "libarduino_tinyusb.a(ncm_device.c.obj)",
    "CONFIG_TINYUSB_MSC_ENABLED=y",
    "CONFIG_TINYUSB_DFU_ENABLED=y",
    "CONFIG_TINYUSB_NCM_ENABLED=y",
    "14608 B",
):
    require(evidence in runtime_doc,
            f"TinyUSB artifact evidence missing from documentation: {evidence}")

for needle in (
    "profile: [normal, midi-only]",
    "image-kind: [product, runtime]",
    "github.event.pull_request.head.sha || github.sha",
    "test_cardputer_memory_baseline_source_regressions.py",
    "set -o pipefail",
    "2>&1 | tee",
    "docs/stages/CARDPUTER_MEMORY_RUNTIME_TELEMETRY.md",
    "scripts/instrument_cardputer_memory_runtime.py",
    "scripts/report_cardputer_tinyusb_class_buffers.sh",
):
    require(needle in workflow, f"workflow contract missing: {needle}")
require("bash scripts/check_cardputer_dram_budget.sh" not in workflow,
        "baseline workflow must not duplicate the product gate")

for needle in (
    "81689b4",
    "one-line",
    "no new numeric threshold",
    "full `.dram0.bss` attribution",
    "59152",
    "59096",
    "provisional `191488` self-audit",
    "passes **4 of 7**",
    "boundary audit",
):
    require(needle.lower() in doc_lower,
            f"documentation contract missing: {needle}")
require(doc.count("**MISSING**") >= 3,
        "runtime, reserves, and derivation must remain visibly missing")
require("loopStackWords" in runtime_doc and "supersedes" in runtime_doc,
        "telemetry contract must explicitly retire misleading stack names")
require("direct runtime-only accessors" in runtime_doc,
        "telemetry contract must document direct task-handle access")
require("smfStackFreeBytes" in runtime_doc and "dispatchStackFreeBytes" in runtime_doc,
        "telemetry contract must cover dense-SMF task stacks")

for path in (
    ".github/workflows/cardputer-memory-baseline.yml",
    "docs/stages/CARDPUTER_MEMORY_BASELINE.md",
    "docs/stages/CARDPUTER_MEMORY_RUNTIME_TELEMETRY.md",
    "scripts/build_cardputer_memory_baseline.sh",
    "scripts/check_cardputer_dram_budget.sh",
    "scripts/instrument_cardputer_memory_runtime.py",
    "scripts/report_cardputer_memory_baseline.sh",
    "scripts/report_cardputer_tinyusb_class_buffers.sh",
    "tests/test_cardputer_memory_baseline_source_regressions.py",
):
    require(path in doc, f"boundary audit missing changed path: {path}")

print("Cardputer memory baseline source regressions: PASS")
