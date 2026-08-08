from pathlib import Path

path = Path("src/generation/rhythm/rhythm_realizer.cpp")
text = path.read_text(encoding="utf-8")
old = """  if (!isOnsetLegal(archetype, lane, step) ||
      structuralCount(occupancy, bar, lane.role) >= lane.structuralMax ||
      !hardCandidateAdditionAllowed(archetype, occupancy, bar, lane.role, step)) {
    return false;
  }
"""
new = """  if (!isOnsetLegal(archetype, lane, step) ||
      structuralCount(occupancy, bar, lane.role) >= lane.structuralMax ||
      totalStructural(occupancy, bar) >= archetype.density.structuralMax ||
      !hardCandidateAdditionAllowed(archetype, occupancy, bar, lane.role, step)) {
    return false;
  }
"""
if text.count(old) != 1:
    raise SystemExit(f"expected one addPlanSecondary target, found {text.count(old)}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
