#!/usr/bin/env python3
from pathlib import Path
import re

path = Path('tests/test_four_axis_ui_source_regressions.py')
text = path.read_text(encoding='utf-8')
pattern = re.compile(
    r'length_owner_tokens = .*?(?=# TEXTURE: sound surface and seven read-only macro projection only\.)',
    re.DOTALL,
)
replacement = '''length_owner_tokens = ("capture_length_", "cycleLength(")
unexpected_length_owners = []
for candidate in PAGE_DIR.glob("*_page.*"):
    page_source = candidate.read_text(encoding="utf-8")
    if any(token in page_source for token in length_owner_tokens):
        if candidate.name not in {"phrase_page.h", "phrase_page.cpp"}:
            unexpected_length_owners.append(candidate.name)
if unexpected_length_owners:
    raise AssertionError(
        "selected phrase length has duplicate UI owners: "
        + ", ".join(sorted(unexpected_length_owners))
    )

'''
text, count = pattern.subn(replacement, text, count=1)
if count != 1:
    raise SystemExit('generated phrase-length ownership block not found')
path.write_text(text, encoding='utf-8')
