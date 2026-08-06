#!/usr/bin/env python3
from pathlib import Path

path = Path("src/ui/global_help_content.h")
text = path.read_text(encoding="utf-8")
old = '''constexpr const char* kGenerationLines[] = {
    "=== GENERATION 3/4 ===",
    "Generation = material/development",
    "Enter/G     Materialize current bar",
    "SCOPE       Current Song row",
    "PLAN        Single bar / base",
    "A/S/FILL    Generation probabilities",
    "Phrase len  Owned by PHRASE CORE",
    "Linear constructive pass",
    "No scoring or retry loop",
    "No texture or microtiming changes",
};
'''
new = '''constexpr const char* kGenerationLines[] = {
    "=== GENERATION 3/4 ===",
    "Generation = material/development",
    "Arrows      Select target Song row",
    "Hold arrows Accelerate target move",
    "Enter/G     Materialize selected row",
    "PLAN        Single bar / base",
    "A/S/FILL    Generation probabilities",
    "Phrase len  Owned by PHRASE CORE",
    "Linear constructive pass",
    "No scoring or retry loop",
    "No texture or microtiming changes",
};
'''
if old not in text:
    raise SystemExit("Generation help block not found")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
