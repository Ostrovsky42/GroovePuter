#pragma once

namespace GroovePuterPlatform {

using CardputerSdReadyHook = void (*)();

bool cardputerSdMounted();
bool ensureCardputerSdMounted();

// Register one lightweight callback that is invoked once, immediately after
// the shared Cardputer SD mount becomes usable. The callback must not remount
// SD and is intended for control-side registries that must exist before Scene
// restore begins.
void setCardputerSdReadyHook(CardputerSdReadyHook hook);

}  // namespace GroovePuterPlatform
