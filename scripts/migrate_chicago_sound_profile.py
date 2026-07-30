#!/usr/bin/env python3
"""Select TB303 engines before applying Chicago Jack timbre parameters."""

from pathlib import Path

path = Path(__file__).resolve().parents[1] / "src/dsp/genre_manager.cpp"
text = path.read_text(encoding="utf-8")
old = """void GenreManager::applyGenreTimbre(MiniAcid& engine) {
    const GenreBehavior b = getBehavior();
    const GenreTimbre& t = b.timbre;

    for (int v = 0; v < 2; ++v) {
"""
new = """void GenreManager::applyGenreTimbre(MiniAcid& engine) {
    // Atlas Chicago Jack is authored for an acid voice. Parameter IDs below
    // are TB303-specific and must not be sent to SID/AY/OPL2 engines where the
    // same numeric indices control unrelated synthesis parameters.
    if (state_.recipe == 6) {
        engine.setSynthEngine(0, \"TB303\");
        engine.setSynthEngine(1, \"TB303\");
    }

    const GenreBehavior b = getBehavior();
    const GenreTimbre& t = b.timbre;

    for (int v = 0; v < 2; ++v) {
"""
if text.count(old) != 1:
    raise RuntimeError(f"expected one applyGenreTimbre prologue, found {text.count(old)}")
path.write_text(text.replace(old, new, 1), encoding="utf-8")
