#!/usr/bin/env python3
from pathlib import Path

path = Path("tests/test_scene_revision_source_regressions.py")
text = path.read_text(encoding="utf-8")
old = '        "MiniAcidParamId::MainVolume",\n'
if text.count(old) != 1:
    raise RuntimeError("legacy MainVolume dirty assertion missing")
text = text.replace(old, "", 1)
anchor = '''    for mutation in (
        "genre.applySoundMacros",
'''
if anchor not in text:
    raise RuntimeError("persistent project setting loop anchor missing")
path.write_text(text, encoding="utf-8")
