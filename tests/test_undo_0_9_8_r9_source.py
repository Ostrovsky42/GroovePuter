from pathlib import Path

root = Path(__file__).resolve().parents[1]

def read(path):
    return (root / path).read_text()

owner = read('src/state/undo_owner.h')
slot = read('src/state/bounded_undo_slot.h')
receipts = read('src/state/undo_receipts.h')
drum = read('src/ui/pages/drum_sequencer_page.cpp')
drum_legacy = read('src/ui/pages/drum_sequencer_page_legacy.h')
genre = read('src/ui/pages/genre_page.cpp')
generation = read('src/generation/migration/quantized_generation_undo_owner_impl.h')
tb = read('src/ui/pages/tb303_params_page.cpp')
display = read('src/ui/miniacid_display.cpp')

assert 'togglePrepared' in owner
assert 'exchangeFixedValue' in owner
assert 'next_is_redo_' in slot
assert 'DrumPatternUndoPayload' in receipts
assert 'commitDrumPatternMutation' in drum_legacy
assert 'regenerateDrumsWithQuantizedCommit' in drum
assert 'toggleLastQuantizedGeneration' in generation
assert 'GROOVEPUTER_APP_EVENT_UNDO' in genre
assert 'REDO: GEN' in genre
assert 'REDO: DRUMS' in drum
assert "value >= 1 && value <= 26" in tb
for scan in ['GROOVEPUTER_A', 'GROOVEPUTER_X', 'GROOVEPUTER_C', 'GROOVEPUTER_V']:
    assert scan in tb
assert 'UNDO: RETURN PAGE' in display and 'UNDO: EMPTY' in display
assert 'QuantizedGenerationScope::Drums' in generation
print('R9 source contracts PASS')
