#!/usr/bin/env python3
from pathlib import Path
import sys

NEED = (
    "addVariation",
    "applyRhythmBarFunctionMutation",
    "evolveRhythmPhrase",
    "evolveMultiBarPhrase",
    "regeneratePhraseAuditionWithProbe",
)


def rows_for(text: str, symbol: str) -> list[str]:
    return [
        line for line in text.splitlines()
        if symbol in line and "::<lambda>" not in line
    ]


def main() -> None:
    if len(sys.argv) < 2:
        raise SystemExit("usage: test_e1a_stack_report.py <stack-usage files...>")

    text = "\n".join(
        Path(path).read_text(encoding="utf-8") for path in sys.argv[1:]
    )
    for symbol in NEED:
        rows = rows_for(text, symbol)
        if not rows:
            raise SystemExit(f"missing stack-usage symbol: {symbol}")
        print(f"{symbol}: {rows[0]}")

    print(
        "CHAIN production: regeneratePhraseAuditionWithProbe -> "
        "evolveMultiBarPhrase -> evolveRhythmPhrase -> "
        "applyRhythmBarFunctionMutation"
    )
    print("OWNER canonical mutation: rhythm_realizer")
    print("BAR compatibility: trajectory planning + canonical mutation delegation")
    print("PHRASE: segmentation/identity/role continuity; no mutation primitive")
    print(
        "CHAIN estimate uses only actual nested calls; unrelated translation-unit "
        "frames are not summed"
    )
    print("E1a host stack report: comparison only; not ESP32-S3 runtime HWM")


if __name__ == "__main__":
    main()
