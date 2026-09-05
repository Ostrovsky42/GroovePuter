#pragma once

// Decides what the panic key (x) tells the operator. Isolated as a pure
// function, in the style of smf_dispatch_policy.h, so it is testable without
// constructing the page (IGfx, MiniAcid, AudioGuard) or the player service.
//
// player_->panic() enqueues a Panic command whether or not a file is loaded;
// the player task silently no-ops it when unloaded (see
// CardputerSmfPlayerService::taskLoop, case CommandType::Panic: if (loaded_)).
// The caller previously read the enqueue result alone and reported
// "MIDI PANIC / PAUSE" regardless, so pressing panic with nothing loaded
// showed a confirmation for a command that did nothing.

inline const char* smfPanicToastMessage(bool loaded, bool queued) {
    if (!loaded) return "PANIC: NO FILE LOADED";
    return queued ? "MIDI PANIC / PAUSE" : "PANIC QUEUE BUSY";
}
