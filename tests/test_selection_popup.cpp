#include <cassert>
#include <cstdio>

#include "src/ui/selection_popup.h"

namespace {
UIEvent key(char value) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.key = value;
  return event;
}

UIEvent arrow(KeyScanCode code) {
  UIEvent event{};
  event.event_type = GROOVEPUTER_KEY_DOWN;
  event.scancode = code;
  return event;
}
}  // namespace

int main() {
  UI::SelectionPopup popup;
  assert(popup.open("TEST", 3, 0));
  assert(popup.isOpen());
  assert(popup.selectedIndex() == 0);

  assert(popup.handleEvent(arrow(GROOVEPUTER_DOWN)) ==
         UI::SelectionPopupResult::None);
  assert(popup.selectedIndex() == 1);
  assert(popup.handleEvent(arrow(GROOVEPUTER_UP)) ==
         UI::SelectionPopupResult::None);
  assert(popup.selectedIndex() == 0);
  assert(popup.handleEvent(arrow(GROOVEPUTER_UP)) ==
         UI::SelectionPopupResult::None);
  assert(popup.selectedIndex() == 2);

  assert(popup.handleEvent(key('\n')) ==
         UI::SelectionPopupResult::Confirmed);
  assert(!popup.isOpen());

  assert(popup.open("TEST", 2, 1));
  assert(popup.handleEvent(key(0x1B)) ==
         UI::SelectionPopupResult::Cancelled);
  assert(!popup.isOpen());

  std::puts("selection popup state: PASS");
}
