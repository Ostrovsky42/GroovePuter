#!/usr/bin/env python3
"""H6 source regression: opening FORM loads the genre's form (song_page_r4_owner.inc)."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
owner = (ROOT / "src/ui/pages/song_page_r4_owner.inc").read_text(encoding="utf-8")
header = (ROOT / "src/ui/pages/song_page.h").read_text(encoding="utf-8")

assert "int form_genre_ = -1;" in header, "SongPage must remember the genre of the loaded form"
start = owner.index("if (ui_event.event_type == GROOVEPUTER_KEY_DOWN && !ui_event.alt &&")
block = owner[start:owner.index("if (form_view_ && ui_event.event_type == GROOVEPUTER_KEY_DOWN)", start)]
assert "form_view_ = !form_view_;" in block
assert "if (form_view_) {" in block, "the load happens when FORM opens"
assert "form_genre_ != mode" in block, "reload only when the genre changed (or none loaded)"
assert "EnergyCurve::genreCurve(static_cast<uint8_t>(mode))" in block
assert '"FORM: %s"' in block, "toast names the genre"
assert "form_section_ = 0;" in block
assert "GenreManager::generativeModeName" in block
print("song form genre source regressions: OK")
