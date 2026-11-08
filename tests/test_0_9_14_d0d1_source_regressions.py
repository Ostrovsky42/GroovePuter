#!/usr/bin/env python3
"""D0-D1 source firewall: downbeat presence must not become musical authority."""

from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
SEM = (ROOT / "src/dsp/development_semantics.h").read_text()
ADAPTER = (ROOT / "src/dsp/development_semantic_adapter.h").read_text()
LEGACY = (ROOT / "src/dsp/musical_development.h").read_text()

failures: list[str] = []

def require(condition: bool, message: str) -> None:
    if not condition:
        failures.append(message)

require("PrimaryDownbeatOnsetPresence" in SEM,
        "D0-D1 truthful claim name missing from semantic carrier")
require("PrimaryDownbeatOnsetPresence" in ADAPTER,
        "D0-D1 adapter does not publish truthful downbeat capability")
require("MetricAnchorTheOne" not in SEM and "MetricAnchorTheOne" not in ADAPTER,
        "D0-D1 overnamed MetricAnchorTheOne claim still present")
require("hasPrimaryDownbeatOnset" in ADAPTER,
        "D0-D1 narrow observable provider missing")

require("hasEventOnTheOne" not in ADAPTER,
        "D0-D1 adapter still imports legacy The One helper")
require("genreRequiresTheOne" not in ADAPTER,
        "D0-D1 adapter still uses downbeat presence as genre shortcut")
require("sourceHadTheOne" not in ADAPTER and "candidateHasTheOne" not in ADAPTER,
        "D0-D1 adapter still carries overclaimed The One variables")

require("case ClaimEvidenceStatus::Fail:" in ADAPTER and
        "return GenreStatus::Violation;" in ADAPTER,
        "D0-D1 accidentally removed explicit genre-requirements failure semantics")

require("hasEventOnTheOne" in LEGACY,
        "D0-D1 unexpectedly rewrote legacy downbeat helper")
require("theOnePreserved" in LEGACY,
        "D0-D1 unexpectedly rewrote legacy evidence contract")
require("requireTheOne" in LEGACY,
        "D0-D1 unexpectedly rewrote legacy request contract")

cap_pos = ADAPTER.find(
    "setCapability(facts, CapabilityClaim::PrimaryDownbeatOnsetPresence")
require(cap_pos >= 0, "D0-D1 capability assignment missing")
if cap_pos >= 0:
    tail = ADAPTER[cap_pos:cap_pos + 900]
    require("facts.genre =" not in tail,
            "D0-D1 downbeat capability still writes genre")
    require("facts.lineage =" not in tail,
            "D0-D1 downbeat capability still writes lineage")
    require("facts.trajectory =" not in tail,
            "D0-D1 downbeat capability still writes trajectory")

if failures:
    for failure in failures:
        print(failure)
    print(f"D0-D1 source firewall: FAIL ({len(failures)} issues)")
    sys.exit(1)

print("D0-D1 source firewall: PASS")
