from pathlib import Path

path = Path("tests/test_rhythm_stage2_atlas_realization.cpp")
text = path.read_text(encoding="utf-8")
old = """  relation.op = RelationshipOp::Respond;
  relation.strength = ConstraintStrength::Hard;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  relation.minOffset = 2;
  relation.maxOffset = 3;
  relation.minResponsesPerWindow = 1;
  relation.maxResponsesPerWindow = 2;
"""
new = """  relation.op = RelationshipOp::Respond;
  relation.strength = ConstraintStrength::Soft;
  relation.scope = RelationshipScope::BarLocal;
  relation.zoneMask = kAllSteps;
  relation.minOffset = 2;
  relation.maxOffset = 3;
  relation.weight = 60;
"""
if text.count(old) != 1:
    raise SystemExit(f"expected one Deep Chord relation target, found {text.count(old)}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
