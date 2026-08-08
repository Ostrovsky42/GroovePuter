from pathlib import Path

path = Path("tests/test_rhythm_stage2.cpp")
text = path.read_text(encoding="utf-8")
needle = "namespace {\n\nstruct Stage2Fixture {\n"
replacement = """namespace {

uint8_t bitCount16(StepMask value) {
  uint8_t count = 0;
  while (value) {
    value = static_cast<StepMask>(value & (value - 1u));
    ++count;
  }
  return count;
}

struct Stage2Fixture {
"""
if text.count(needle) != 1:
    raise SystemExit(f"expected one helper insertion point, found {text.count(needle)}")
path.write_text(text.replace(needle, replacement, 1), encoding="utf-8")
