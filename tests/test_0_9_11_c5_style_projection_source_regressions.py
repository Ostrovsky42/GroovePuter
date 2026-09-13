#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
STATE = (ROOT / "src/state/generation_request_state.h").read_text(encoding="utf-8")
GENRE = (ROOT / "src/ui/pages/genre_page.cpp").read_text(encoding="utf-8")
FEEL = (ROOT / "src/ui/pages/feel_page.cpp").read_text(encoding="utf-8")
DRUM = (ROOT / "src/ui/pages/drum_sequencer_page.cpp").read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    # P1/P2/P3 remains the stable internal realization contract.
    for token in ('return "P1";', 'return "P2";', 'return "P3";',
                  "cycleGenerationLevel", "currentGenerationLevel"):
        require(token in STATE, f"internal realization contract changed: {token}")

    # Every musician-facing generate surface must project that mechanism as STYLE.
    for name, source in (("GENRE", GENRE), ("FEEL", FEEL), ("DRUMS", DRUM)):
        require("generationStyleName" in source,
                f"{name} does not expose the shared realization selector as STYLE")
        require("generationLevelShortName" not in source,
                f"{name} leaks P1/P2/P3 engineering vocabulary to the musician")

    require("P:STYLE" in GENRE,
            "GENRE footer must name the musician-facing STYLE action")
    require("P:STYLE" in FEEL,
            "FEEL footer must name the musician-facing STYLE action")
    require("P:LEVEL" not in FEEL,
            "FEEL footer must not expose the internal P-level")

    print("0.9.11 C5 STYLE projection source contracts: OK")


if __name__ == "__main__":
    main()
