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
synth_parent = read('src/ui/pages/synth_sequencer_page.cpp')
song_owner = read('src/ui/pages/song_page_r4_owner.inc')
phrase = read('src/ui/pages/phrase_page.cpp')

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
assert 'UNDO: NOT HERE' in undo_ux and 'REDO: NOT HERE' in undo_ux
assert 'UNDO: EMPTY' in undo_ux
assert 'RETURN PAGE' not in undo_ux
assert 'QuantizedGenerationScope::Drums' in generation

# Ctrl+Z never navigates or cross-routes to a hidden SYNTH subpage. The parent
# only admits Pattern history while NOTES is already visible; the Pattern child
# owns receipt-target validation, including Synth A/B. Esc/back is navigation.
assert 'GroovePuterUndoUx::isUndoEvent(ui_event)' in synth_parent
assert 'synth_tab_ == SynthTab::Notes' in synth_parent
assert 'owner.kind() == GroovePuterUndo::UndoKind::Pattern' in synth_parent
assert 'MultiPage::handleEvent(ui_event)' in synth_parent
assert 'pattern_page_->handleEvent(ui_event)' not in synth_parent
assert 'SynthPatternUndoPayload retained' not in synth_parent
assert 'synthPatternUndoTargetAvailable' not in synth_parent
assert 'src/state/undo_receipts.h' not in synth_parent
assert 'RETURN PAGE' not in synth_parent

# One-slot history is bidirectional. User feedback must describe the action
# that just happened, not merely the shortcut name.
assert 'const bool redo = owner.nextIsRedo();' in synth_parent
assert 'owner.nextIsRedo() != redo' in synth_parent
assert 'REDO: PATTERN' in synth_parent and 'UNDO: PATTERN' in synth_parent
assert 'const bool redo = owner.nextIsRedo();' in song_owner
assert 'REDO: SONG' in song_owner and 'UNDO: SONG' in song_owner
assert phrase.count('const bool redo = owner.nextIsRedo();') >= 2
assert 'REDO: PHRASE' in phrase and 'UNDO: PHRASE' in phrase
assert 'REDO: SONG' in phrase and 'UNDO: SONG' in phrase
assert 'RETURN PAGE' not in song_owner
assert 'RETURN PAGE' not in phrase

print('R9 source contracts PASS')