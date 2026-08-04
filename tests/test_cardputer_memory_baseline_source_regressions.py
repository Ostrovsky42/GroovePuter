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


require('MAX_BYTES="${2:-122880}"' in gate,
        "the mandatory fixed-DRAM gate must remain at 122880 bytes")
require("PREVIOUS_GATE_BYTES=\"${PREVIOUS_GATE_BYTES:-191488}\"" in report,
        "the report must retain the previous 191488-byte reference")
require("CURRENT_GATE_BYTES=\"${CURRENT_GATE_BYTES:-122880}\"" in report,
        "the report must retain the current 122880-byte gate")
require("exit 0" in report and "deliberately non-gating" in report,
        "the baseline report must not replace or weaken the product gate")

require("mktemp -d /tmp/grooveputer-memory-baseline" in build,
        "the diagnostic build must use a temporary source tree")
require("rsync -a --delete" in build,
        "the diagnostic build must copy the checkout before instrumentation")
require("SOURCE_ROOT}/GroovePuter.ino" in build,
        "instrumentation must target only the temporary sketch")
require("git commit" not in build and "git push" not in build,
        "the diagnostic build must never mutate repository history")
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
require("check_cardputer_dram_budget.sh" not in workflow,
        "the baseline workflow must not duplicate or bypass the product gate")

for candidate in (
    "s_tempLoadScene",
    "Wavetable static arrays",
    "g_mainScene",
    "g_smfPlayer",
    "g_miniAcidInstance",
):
    require(candidate in report or candidate in doc,
            f"missing documented candidate: {candidate}")

require("122676" in doc and "204" in doc,
        "the provenance note must record the reviewed PR #63 measurement")
require("No gate change" in doc,
        "the baseline stage must explicitly prohibit a premature gate change")

print("Cardputer memory baseline source regressions: PASS")
