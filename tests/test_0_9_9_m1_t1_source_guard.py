from pathlib import Path
r=Path(__file__).resolve().parents[1]
m=(r/'src/generation/migration/strong_rhythm_migration.cpp').read_text()
b=(r/'src/generation/migration/strong_rhythm_live_bridge.cpp').read_text()
assert 'migrateStrongRhythmMaterial(' in b and 'selectionGeneration.phraseOrdinal' in m and 'context.patternAddress' in m
print('M1_T1_SOURCE_GUARD PASS')
