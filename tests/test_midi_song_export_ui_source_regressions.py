from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
header = (ROOT / "src/ui/pages/project_page.h").read_text(encoding="utf-8")
source = (ROOT / "src/ui/pages/project_page.cpp").read_text(encoding="utf-8")

assert "ImportMidi, ExportMidi, ClearProject" in header, \
    "Project > Scenes must expose Export MIDI beside Import MIDI"
assert "bool exportMidiAtSelection();" in header, \
    "ProjectPage must keep export dispatch in a dedicated action"
assert '#include "src/platform/cardputer_midi_song_export.h"' in source, \
    "ProjectPage must call the platform export adapter"
assert 'case MainFocus::ExportMidi: label = "Export MIDI"; break;' in source, \
    "the Scenes menu must show Export MIDI"
assert 'if (main_focus_ == MainFocus::ExportMidi) return exportMidiAtSelection();' in source, \
    "Enter on Export MIDI must dispatch the export action"
assert '"STOP BEFORE EXPORT"' in source, \
    "the playing-state help must fit the small display"
assert '"EXPORT /MIDI; TEMP MACROS OMITTED"' in source, \
    "export help must explain the output and macro policy in a short line"

start = source.index("bool ProjectPage::exportMidiAtSelection()")
end = source.index("bool ProjectPage::deleteSelectionInDialog()", start)
export_action = source[start:end]
assert "mini_acid_.isPlaying()" in export_action and "STOP TO EXPORT" in export_action, \
    "export must refuse playback before touching project storage"
assert "exportCurrentSongAsMidi(mini_acid_, result)" in export_action, \
    "export action must use the committed-Song adapter"
