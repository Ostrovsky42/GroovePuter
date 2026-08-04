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
require("exit 0" in report and "deliberately non-gating" in report,
        "the baseline report must not replace the product gate")

require("mktemp -d /tmp/grooveputer-memory-baseline" in build,
        "the diagnostic build must use a temporary source tree")
require("rsync -a --delete" in build,
        "the diagnostic build must copy the checkout before instrumentation")
require("SOURCE_ROOT}/GroovePuter.ino" in build,
        "instrumentation must target only the temporary sketch")
require("git commit" not in build and "git push" not in build,
        "the diagnostic build must never mutate repository history")
require("SOURCE_COMMIT" in build and "git -C" in build,
        "every measurement must identify the immutable source commit")
require("ELF_SHA256" in build and "sha256sum" in build,
        "every measurement must identify the exact ELF bytes")
require("heap_caps_get_minimum_free_size" in build,
        "the diagnostic firmware must report the IDF boot-time heap floor")
require("heap_caps_get_largest_free_block" in build,
        "the diagnostic firmware must sample contiguous internal heap")
require("heap_caps_check_integrity_all" in build,
        "the diagnostic firmware must report heap integrity")
require("uxTaskGetStackHighWaterMark" in build,
        "the diagnostic firmware must report loop/audio stack watermarks")
require("normal|midi-only" in build,
        "normal and MIDI-only profiles must be measurable separately")

require("build_cardputer_memory_baseline.sh" in workflow,
        "the workflow must compile the instrumented diagnostic profile")
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
