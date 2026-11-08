#include <cassert>
#include <cstring>

#include "../src/ui/ui_shell_frame.h"

int main() {
  static_assert(sizeof(UI::UiShellFrameModel) <= 136,
                "U1F shell frame model must remain stack-bounded");

  UI::UiShellFrameModel model{};
  char left[96] = "TAB/U/D:FIELD L/R:CHANGE";
  char right[96] = "HOLD L/R:ACCEL P:LEVEL";

  model.setFooter(left, right);
  assert(model.footer.valid);
  assert(std::strcmp(model.footer.left, "TAB/U/D:FIELD L/R:CHANGE") == 0);
  assert(std::strcmp(model.footer.right, "HOLD L/R:ACCEL P:LEVEL") == 0);

  // Footer text may be assembled in page-local buffers. The frame model owns
  // a bounded copy so shell rendering after page draw cannot dereference stale
  // storage.
  left[0] = 'X';
  right[0] = 'Y';
  assert(model.footer.left[0] == 'T');
  assert(model.footer.right[0] == 'H');

  char longText[160];
  std::memset(longText, 'A', sizeof(longText));
  longText[sizeof(longText) - 1] = '\0';
  model.setFooter(longText, longText);
  assert(model.footer.left[sizeof(model.footer.left) - 1] == '\0');
  assert(model.footer.right[sizeof(model.footer.right) - 1] == '\0');

  model.clear();
  assert(!model.footer.valid);
  assert(model.footer.left[0] == '\0');
  assert(model.footer.right[0] == '\0');

  return 0;
}
