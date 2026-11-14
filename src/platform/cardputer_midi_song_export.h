#pragma once

#include "src/dsp/miniacid_engine.h"
#include "src/midi/midi_song_export_file.h"

bool exportCurrentSongAsMidi(
    MiniAcid& engine,
    GroovePuterMidi::MidiSongExportFileResult& result);
