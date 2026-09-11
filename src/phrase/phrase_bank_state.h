#pragma once

#include <cstdint>

namespace PhraseBank {

constexpr uint8_t kPhraseBankSlotCount = 8;
constexpr uint8_t kPhraseBankVoiceCount = 2;
constexpr char kPhraseBankKeys[kPhraseBankSlotCount] = {
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I'};

inline bool slotForKey(char key, uint8_t& slot) {
  if (key >= 'a' && key <= 'z') {
    key = static_cast<char>(key - 'a' + 'A');
  }
  for (uint8_t i = 0; i < kPhraseBankSlotCount; ++i) {
    if (kPhraseBankKeys[i] == key) {
      slot = i;
      return true;
    }
  }
  return false;
}

inline char keyForSlot(uint8_t slot) {
  return slot < kPhraseBankSlotCount ? kPhraseBankKeys[slot] : '-';
}

class PhraseBankState {
 public:
  uint8_t editingSlot(uint8_t voice) const {
    return voice < kPhraseBankVoiceCount ? editingSlot_[voice] : 0;
  }

  bool selectEditingSlot(uint8_t voice, uint8_t slot) {
    if (voice >= kPhraseBankVoiceCount || slot >= kPhraseBankSlotCount) {
      return false;
    }
    editingSlot_[voice] = slot;
    return true;
  }

 private:
  // PLAY is intentionally absent. MiniAcid::activeMaterial(voice) remains the
  // single audible authority; this state owns only the user's edit target.
  uint8_t editingSlot_[kPhraseBankVoiceCount]{};
};

static_assert(sizeof(PhraseBankState) <= kPhraseBankVoiceCount,
              "Phrase Bank must not grow into a second material owner");

}  // namespace PhraseBank
