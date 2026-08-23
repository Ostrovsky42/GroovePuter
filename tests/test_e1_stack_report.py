#!/usr/bin/env python3
from pathlib import Path
import sys

need = ("addVariation", "evolveRhythmPhrase", "evolveMultiBarPhrase",
        "regeneratePhraseAuditionWithProbe")
text = "\n".join(Path(p).read_text(encoding="utf-8") for p in sys.argv[1:])
for symbol in need:
    rows = [line for line in text.splitlines()
            if symbol in line and "::<lambda>" not in line]
    if not rows:
        raise SystemExit(f"missing stack-usage symbol: {symbol}")
    print(f"{symbol}: {rows[0]}")
print("CHAIN production: regeneratePhraseAuditionWithProbe -> "
      "evolveMultiBarPhrase -> evolveRhythmPhrase -> addVariation helper")
print("CHAIN legacy: evolveRhythmPhrase -> trajectory policy/drop primitive")
print("CHAIN estimate uses only these actual nested calls; unrelated frames are not summed")
print("DUPLICATE/FUTURE accounting: orchestration frames are reported for comparison only")
print("E1 host stack baseline: comparison only; not ESP32-S3 runtime HWM")
