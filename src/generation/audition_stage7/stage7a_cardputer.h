#pragma once

class MiniAcid;
struct UIEvent;

namespace GroovePuterRhythm {
namespace Stage7AAudition {

// Lightweight UI facade. Keep the temporary audition implementation and its
// Session/MIDI/Scene dependencies out of MiniAcidDisplay and GenrePage TUs.
bool cardputerSessionActive();
bool handleCardputerEvent(const UIEvent& event, MiniAcid& engine);

}  // namespace Stage7AAudition
}  // namespace GroovePuterRhythm
