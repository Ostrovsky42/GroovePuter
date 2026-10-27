#pragma once

// MATERIAL take API shared by the MATERIAL page and SONG FORM (T / Ctrl+Z).
// Kept out of phrase_page.h so SONG does not pull the whole MATERIAL header.

#include <cstdint>

#include "../ui_core.h"
#include "src/dsp/miniacid_engine.h"
#include "src/state/undo_owner.h"

// One MATERIAL take written into the Song at a row (the G action). Shared by the
// MATERIAL page and the SONG FORM T key, so both write the same way.
struct MaterialTakeOutcome {
  bool ok = false;
  int bars = 0;
  int songStart = 0;
  int songSlot = 0;
  bool pending = false;   // written for the next bar, not the current one
  int toastMs = 1600;
  char message[64] = {};  // refusal text when !ok
};
MaterialTakeOutcome generateMaterialTakeAtRow(MiniAcid& mini_acid,
                                              const AudioGuard& audio_guard,
                                              int songStart,
                                              uint8_t requestedBars);


// Ctrl+Z for a take written from SONG FORM (same owner as MATERIAL's own undo).
GroovePuterUndo::UndoResult undoLastMaterialTake(MiniAcid& mini_acid,
                                                 const AudioGuard& audio_guard);
bool materialTakeUndoOwned();
