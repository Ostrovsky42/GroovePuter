#!/usr/bin/env python3
"""The lazy Cardputer SMF player wrapper must forward every service method.

ISmfPlayerService gives some methods defaults (return false / empty). A method
the wrapper does not override silently uses that default on the device: the
0.9.18 loop keys read "MIDI PLAYER BUSY", HUB route saves read "SAVE BUSY",
and the file manager could not tell which file the player had open.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
service = (ROOT / "src/midi/smf_player_service.h").read_text(encoding="latin-1")
registry = (ROOT / "src/platform/cardputer_smf_player_registry.cpp").read_text(encoding="latin-1")

start = service.index("class ISmfPlayerService")
body = service[start:service.index("\n};", start)]  # class end, not a "{};" inside
methods = sorted(set(re.findall(r"virtual\s+[\w:<>]+\s+(\w+)\s*\(", body)))
assert methods, "no ISmfPlayerService methods found"

wrapper_start = registry.index("class LazyCardputerSmfPlayer")
wrapper = registry[wrapper_start:registry.index("\n};", wrapper_start)]
missing = [m for m in methods
           if not re.search(r"\b" + m + r"\s*\([^)]*\)\s*(const\s*)?override", wrapper)]
if missing:
    print("LazyCardputerSmfPlayer does not forward: " + ", ".join(missing))
    sys.exit(1)
print(f"SMF player wrapper forwards all {len(methods)} service methods: PASS")
