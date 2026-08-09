#pragma once

class MiniAcid;
struct UIEvent;

namespace GroovePuterRhythm {
namespace Stage7AAudition {

// Lightweight status facade used by MiniAcidDisplay to give an already-active
// audition first refusal before global shortcuts. The actual Session remains
// private to stage7a_cardputer.cpp.
class CardputerSessionFacade {
 public:
  bool active() const;
};

const CardputerSessionFacade& cardputerSession();
bool handleCardputerEvent(const UIEvent& event, MiniAcid& engine);

}  // namespace Stage7AAudition
}  // namespace GroovePuterRhythm
