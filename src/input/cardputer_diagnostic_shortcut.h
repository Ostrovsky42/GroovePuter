#pragma once

#include "cardputer_input_edges.h"

namespace GroovePuterInput {

// Diagnostic builds reserve Fn+D. Claim both representations of this physical
// key so a library reporting HID and word cannot toggle twice.
template <typename KeysState>
bool claimDiagnosticFnD(const KeysState& state, uint8_t hid,
                        char& key, uint32_t& dispatchedLetters) {
  if (!state.fn || state.ctrl || state.alt || hid != 0x07) return false;
  key = 'd';
  dispatchedLetters |= letterDispatchMask(key);
  return true;
}

}  // namespace GroovePuterInput
