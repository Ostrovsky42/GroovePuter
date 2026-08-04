#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

build = (ROOT / "scripts/build_cardputer_memory_baseline.sh").read_text()
report = (ROOT / "scripts/report_cardputer_memory_baseline.sh").read_text()
gate = (ROOT / "scripts/check_cardputer_dram_budget.sh").read_text()
workflow = (ROOT / ".github/workflows/cardputer-memory-baseline.yml").read_text()
doc = (ROOT / "docs/stages/CARDPUTER_MEMORY_BASELINE.md").read_text()


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


require('MAX_BYTES="${2:-191488}"' in gate,
        "the mandatory gate must use the provisional pre-122880 ceiling")
require("not a universal hardware" in gate,
        "the provisional ceiling must not be presented as a hardware limit")
require("PROVISIONAL_GATE_BYTES=\"${PROVISIONAL_GATE_BYTES:-191488}\"" in report,
        "the report must retain the provisional 191488-byte reference")
require("UNSUPPORTED_GATE_BYTES=\"${UNSUPPORTED_GATE_BYTES:-122880}\"" in report,
        "the report must preserve 122880 as an unsupported historical reference")
require("MEMORY_BASELINE_IMAGE_KIND" in report and "kind=%s" in report,
        "every section report must identify product or runtime image kind")
require("10#${size}" in report,
        "zero-padded decimal nm sizes must not be interpreted as octal")
require("exit 0" in report and "deliberately non-gating" in report,
        "the baseline report must not replace the product gate")

for script_name, script in (("gate", gate), ("report", report)):
    require("discover_arduino_tool" in script,
            f"{script_name} must use the shared vendor-neutral discovery pattern")
    require("ARDUINO_PACKAGES_ROOT" in script and "command -v" in script,
            f"{script_name} must support PATH and arbitrary Arduino package vendors")
    require(".arduino15/packages/esp32/tools" not in script,
            f"{script_name} must not assume the Espressif vendor directory")
    require("find \"${package_root}\"" in script and "|| true" in script,
            f"{script_name} tool discovery must fail explicitly, not through set -e")

require("mktemp -d /tmp/grooveputer-memory-baseline" in build,
        "the diagnostic build must use a temporary source tree")
require("rsync -a --delete" in build,
        "the diagnostic build must copy the checkout before instrumentation")
require("SOURCE_ROOT}/GroovePuter.ino" in build,
        "instrumentation must target only the temporary sketch")
require("git commit" not in build and "git push" not in build,
        "the diagnostic build must never mutate repository history")
require("SOURCE_COMMIT" in build and "SOURCE_DIRTY" in build,
        "every measurement must identify source commit and clean-tree state")
require("ELF_SHA256" in build and "sha256sum" in build,
        "every measurement must identify the exact ELF bytes")
require('IMAGE_KIND="${1:-}"' in build and "product|runtime" in build,
        "product and runtime images must be explicit build modes")
require('if [[ "${IMAGE_KIND}" == "runtime" ]]' in build,
        "only runtime images may receive heap instrumentation")
require("Memory baseline image" in build,
        "the build log must label product versus runtime")
require("heap_caps_get_minimum_free_size" in build,
        "the runtime diagnostic firmware must report the IDF boot-time heap floor")
require("heap_caps_get_largest_free_block" in build,
        "the runtime diagnostic firmware must sample contiguous internal heap")
require("heap_caps_check_integrity_all" in build,
        "the runtime diagnostic firmware must report heap integrity")
require("uxTaskGetStackHighWaterMark" in build,
        "the runtime diagnostic firmware must report loop/audio stack watermarks")
require("normal|midi-only" in build,
        "normal and MIDI-only profiles must be measurable separately")

require("build_cardputer_memory_baseline.sh" in workflow,
        "the workflow must compile both baseline image kinds")
require("profile: [normal, midi-only]" in workflow,
        "the workflow must build both USB profiles")
require("image-kind: [product, runtime]" in workflow,
        "the workflow must separate exact product ELF from runtime instrumentation")
require("github.event.pull_request.head.sha || github.sha" in workflow,
        "PR measurements must checkout the immutable head instead of a synthetic merge commit")
require("test_cardputer_memory_baseline_source_regressions.py" in workflow,
        "the workflow must run the source-boundary regression")
require("set -o pipefail" in workflow and "2>&1 | tee" in workflow,
        "the workflow must expose build or report failures behind tee")
require("bash scripts/check_cardputer_dram_budget.sh" not in workflow,
        "the baseline workflow must not duplicate the product gate")

for candidate in (
    "s_tempLoadScene",
    "Wavetable static arrays",
    "g_mainScene",
    "g_smfPlayer",
    "g_miniAcidInstance",
):
    require(candidate in report or candidate in doc,
            f"missing documented candidate: {candidate}")

require("81689b4" in doc and "one-line" in doc,
        "the provenance note must identify the unsupported threshold commit")
require("Source commit" in doc and "ELF SHA-256" in doc and "full FQBN" in doc,
        "the threshold policy must require reproducible build identity")
require("No new numeric threshold" in doc,
        "the baseline stage must prohibit unexplained threshold changes")

print("Cardputer memory baseline source regressions: PASS")
