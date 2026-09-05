#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
page = (root / "src/ui/pages/genre_page.cpp").read_text()
help_content = (root / "src/ui/global_help_content.h").read_text()

required_page_fragments = [
    "keyL",
    "event.meta",
    "toggleLegacyGenreSound(activeGenre)",
    '"SOUND: LEGACY',
    '"SOUND: CURRENT"',
]
for fragment in required_page_fragments:
    if fragment not in page:
        raise AssertionError(f"Genre Fn+L sound A/B wiring missing fragment: {fragment}")

if "Fn+L" not in help_content or "Legacy sound A/B" not in help_content:
    raise AssertionError("Help must document Fn+L Legacy sound A/B on the Genre page")

print("Legacy Acid/Techno Fn+L + Help wiring: OK")
