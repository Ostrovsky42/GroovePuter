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
undo_ux = read('src/ui/undo_ux.h')

assert 'togglePrepared' in owner
assert 'exchangeFixedValue' in owner
assert 'next_is_redo_' in slot
assert 'DrumPatternUndoPayload' in receipts
assert 'commitDrumPatternMutation' in drum_legacy
assert 'regenerateDrumsWithQuantizedCommit' in drum
plain_g = drum[drum.index('if (keyG && !ui_event.ctrl && !ui_event.alt && !ui_event.meta)'):drum.index('// P owns the single P1/P2/P3 request selector.') ]
assert 'page->withAudioGuard' not in plain_g
assert 'page->audio_guard_' in plain_g
assert 'toggleLastQuantizedGeneration' in generation
assert 'GROOVEPUTER_APP_EVENT_UNDO' in genre
assert 'REDO: GEN' in genre
assert 'REDO: DRUMS' in drum
assert "value >= 1 && value <= 26" in tb
for scan in ['GROOVEPUTER_A', 'GROOVEPUTER_X', 'GROOVEPUTER_C', 'GROOVEPUTER_V']:
    assert scan in tb
assert 'GroovePuterUndoUx::isUndoEvent(event)' in display
assert 'GroovePuterUndoUx::fallbackToast(hasReceipt)' in display
assert 'UNDO: RETURN PAGE' in undo_ux and 'UNDO: EMPTY' in undo_ux
assert 'QuantizedGenerationScope::Drums' in generation
print('R9 source contracts PASS')
