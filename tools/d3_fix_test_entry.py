#!/usr/bin/env python3
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
# Retrigger after the workflow already exists in the branch parent.

def replace_once(path, old, new):
    p = ROOT / path
    text = p.read_text(encoding='utf-8')
    if text.count(old) != 1:
        raise RuntimeError(f'{path}: anchor count={text.count(old)}')
    p.write_text(text.replace(old, new, 1), encoding='utf-8')

replace_once(
    'src/generation/migration/live_song_arrangement_activation.h',
    '#include <type_traits>\n',
    '#include <type_traits>\n\n#include "../../state/undo_receipts.h"\n',
)

replace_once(
    'tests/test_song_phrase_edit_0_9_8_r4_source_regressions.py',
    '''    require("hasPendingSongReverseToggle" in song_r4 and\n            "GroovePuterState::markSceneMutated();" in song_r4,\n            "queued reverse must remain an explicit 0.9.9 boundary that expires stale Undo")\n''',
    '''    require("songR4QueuedReverseGesture" in song_r4 and\n            "return handleEventLegacyUnowned(ui_event);" in song_r4,\n            "D3 must preserve Ctrl+R gesture routing while moving reverse persistence to the canonical Song owner")\n    require("hasPendingSongReverseToggle" not in song_r4 and\n            "GroovePuterState::markSceneMutated();" not in song_r4,\n            "D3 must retire the private reverse queue and manual revision invalidation")\n''',
)

print('D3 compile include + inherited R4 gate migrated')
