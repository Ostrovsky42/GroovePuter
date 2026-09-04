#!/usr/bin/env python3
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
CPP = ROOT / "src/dsp/miniacid_engine.cpp"
HEADER = ROOT / "src/dsp/miniacid_engine.h"
RUNNER = ROOT / "tests/run_pattern_phrase_p2_tests.sh"
CORE = ROOT / "tests/test_source_regressions.py"
GF2 = ROOT / "tests/test_gf2_i1_tempo_corridor_arbitration.cpp"
DEAD = ROOT / "tests/test_pattern_phrase_p2_dead_legacy_release_contract.py"


def replace_once(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected exactly one match, found {count}: {old[:100]!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


# 1. Materialize the already-reviewed bounded Task-7 candidate into production.
subprocess.run(["python3", "tests/apply_p2_task7_lifecycle_green.py"], cwd=ROOT, check=True)

# 2. Retire the compatibility countdowns only after proving every remaining cpp use
# is a plain assignment/reset. No reads, branches, decrements or scheduling logic may
# be silently removed by this closure.
cpp = CPP.read_text(encoding="utf-8")
for name in ("gateCountdownA_", "gateCountdownB_"):
    occurrences = [line for line in cpp.splitlines() if name in line]
    if not occurrences:
        raise SystemExit(f"expected remaining compatibility references for {name}")
    for line in occurrences:
        if re.fullmatch(r"\s*" + re.escape(name) + r"\s*=\s*[^;]+;\s*", line) is None:
            raise SystemExit(f"refusing to remove semantic {name} use: {line!r}")
    cpp = "\n".join(line for line in cpp.splitlines() if name not in line) + "\n"
CPP.write_text(cpp, encoding="utf-8")

header = HEADER.read_text(encoding="utf-8")
for declaration in ("  long gateCountdownA_ = 0;\n", "  long gateCountdownB_ = 0;\n"):
    if header.count(declaration) != 1:
        raise SystemExit(f"unexpected countdown declaration count: {declaration!r}")
    header = header.replace(declaration, "", 1)
header = header.replace(
    "  // P2 compatibility fields remain physically present for this cutover\n"
    "  // commit, but they no longer own Pattern backend lifetime decisions.\n",
    "",
    1,
)
HEADER.write_text(header, encoding="utf-8")

# 3. Make the dead-owner proof permanent and part of the canonical P2 gate.
DEAD.write_text('''#!/usr/bin/env python3\nfrom pathlib import Path\n\nROOT = Path(__file__).resolve().parents[1]\nfor rel in ("src/dsp/miniacid_engine.cpp", "src/dsp/miniacid_engine.h"):\n    text = (ROOT / rel).read_text(encoding="utf-8")\n    for legacy in ("gateCountdownA_", "gateCountdownB_"):\n        if legacy in text:\n            raise AssertionError(f"P2 dead legacy release owner remains in {rel}: {legacy}")\nprint("P2 dead legacy release-owner contract: OK")\n''', encoding="utf-8")
replace_once(
    RUNNER,
    "python3 tests/test_pattern_phrase_p2_lifecycle_barrier_contract.py || lifecycle_contract_status=$?\n"
    "if (( lifecycle_behavior_status != 0 || lifecycle_contract_status != 0 )); then\n"
    "  echo \"P2 lifecycle/source barrier closure is not GREEN\" >&2\n"
    "  exit 1\n"
    "fi\n",
    "python3 tests/test_pattern_phrase_p2_lifecycle_barrier_contract.py || lifecycle_contract_status=$?\n"
    "if (( lifecycle_behavior_status != 0 || lifecycle_contract_status != 0 )); then\n"
    "  echo \"P2 lifecycle/source barrier closure is not GREEN\" >&2\n"
    "  exit 1\n"
    "fi\n\n"
    "python3 tests/test_pattern_phrase_p2_dead_legacy_release_contract.py\n",
)

# 4. Core HOST fixture used the retired countdown branch merely as a textual end
# marker. Anchor the PPQN dispatch characterization to the actual next production
# statement after the tick-advance block instead.
replace_once(
    CORE,
    '    loop_end = source.index("if (gateCountdownA_", loop_start)\n',
    '    loop_end = source.index("const uint32_t absoluteSubtick", loop_start)\n',
)

# 5. GF2-I1's intentionally narrow MiniAcid fixture predates P1C/P2. Teach only the
# fixture about the runtime-event surface now referenced by migration headers; keep
# all GF2 tempo/corridor semantics unchanged.
replace_once(
    GF2,
    '#include "../src/generation/migration/strong_rhythm_migration.h"\n',
    '#include "../src/generation/migration/strong_rhythm_migration.h"\n'
    '#include "../src/phrase/runtime_synth_events.h"\n',
)
replace_once(
    GF2,
    '  void regeneratePatternsWithGenre() {}\n\n private:\n',
    '  void regeneratePatternsWithGenre() {}\n\n'
    '  bool rebuildPatternRuntimeEventBank() { return true; }\n'
    '  bool refreshPatternRuntimeEvents(int, int, int) { return true; }\n'
    '  const PhraseRuntime::RuntimePatternEventBuffer& activePatternRuntimeEvents(int) const {\n'
    '    static const PhraseRuntime::RuntimePatternEventBuffer empty{};\n'
    '    return empty;\n'
    '  }\n'
    '  void barrierPatternRuntimeSourceTransition() {}\n\n'
    ' private:\n',
)

# Candidate tooling must not survive the closure commit.
(ROOT / "tests/apply_p2_task7_lifecycle_green.py").unlink()
print("P2 closure materialized")
