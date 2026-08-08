from pathlib import Path

path = Path("src/generation/rhythm/rhythm_realizer.cpp")
text = path.read_text(encoding="utf-8")

old = """bool addPlanGhost(const RhythmArchetype& archetype,
                  RhythmPhrasePlan& plan,
                  uint8_t bar,
                  const LaneGrammar& lane,
                  uint8_t step) {
"""
new = """uint16_t totalOrnaments(const RhythmPhrasePlan& plan, uint8_t bar) {
  uint16_t total = 0;
  for (uint8_t role = 0; role < kRhythmRoleCount; ++role) {
    total += bitCount16(plan.bars[bar].roles[role].ghosts);
  }
  return total;
}

bool addPlanGhost(const RhythmArchetype& archetype,
                  RhythmPhrasePlan& plan,
                  uint8_t bar,
                  const LaneGrammar& lane,
                  uint8_t step) {
"""
if text.count(old) != 1:
    raise SystemExit(f"expected one addPlanGhost function target, found {text.count(old)}")
text = text.replace(old, new, 1)

old = """  if ((rolePlan.structural | rolePlan.secondary | rolePlan.ghosts) & bit ||
      !isOnsetLegal(archetype, lane, step) ||
      bitCount16(rolePlan.ghosts) >= lane.ornamentMax) {
"""
new = """  if ((rolePlan.structural | rolePlan.secondary | rolePlan.ghosts) & bit ||
      !isOnsetLegal(archetype, lane, step) ||
      bitCount16(rolePlan.ghosts) >= lane.ornamentMax ||
      totalOrnaments(plan, bar) >= archetype.density.ornamentMax) {
"""
if text.count(old) != 1:
    raise SystemExit(f"expected one ornament guard target, found {text.count(old)}")
text = text.replace(old, new, 1)

path.write_text(text, encoding="utf-8")
