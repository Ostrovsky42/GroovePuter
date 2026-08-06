#!/usr/bin/env python3
from pathlib import Path

path = Path('scripts/apply_axis_hardware_feedback_fixes.py')
text = path.read_text(encoding='utf-8')
old = "'    char message[48];\\n"
new = "'    char message[32];\\n"
if old not in text:
    raise SystemExit('single-cell Song toast helper anchor not found')
path.write_text(text.replace(old, new, 1), encoding='utf-8')
