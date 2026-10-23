from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/diag/melody_pending_census.h").read_text(encoding="utf-8")

required = (
    "#if defined(GROOVEPUTER_MELODY_CENSUS)",
    "sizeof(PhraseRuntime::RuntimeSynthEventBuffer)",
    'report(c.active[0] ? "A-active" : "A-active-alloc-fail")',
    'report("A-active+pending-A")',
    'report(c.active[1] ? "A+B-active" : "B-active-alloc-fail")',
    'report("A+B-active+pending-A+B")',
    "kChurnCycles = 40",
    "allocationFailures",
    "loadFailures",
    'constexpr const char* kPayloadDirectory = "/__gp_diag"',
    "SD.exists(c.payloadPath)",
    "SD.remove(c.payloadPath)",
    "GroovePuterUndo::UndoOwner::payloadCapacity()",
    "sample.lengthTicks = 8u * PhraseRuntime::kTicksPerBar",
    "sample.events[i].startTick = static_cast<uint16_t>(i * 24u)",
    "if (!c.active[0]) { finish(false); return; }",
    "if (!c.active[1]) { finish(false); return; }",
    'report("churn-load-fail")',
    'c.completed = completed && c.cleanupSucceeded',
)
for token in required:
    assert token in SOURCE, f"Long Melody diagnostic contract missing: {token}"

assert "new " not in SOURCE, "census must use fixed-size heap_caps allocations only"
assert "MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT" in SOURCE
assert "MELODY_CENSUS_TICK(playing) ((void)0)" in SOURCE

print("Long Melody pending-residency census source: PASS")
