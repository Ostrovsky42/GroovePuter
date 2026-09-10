#include <cassert>
#include <cstring>
#include <string>

#include "../src/ui/ui_active_page_title.h"
#include "../src/ui/ui_core.h"

namespace {

class LayoutProbePage final : public IPage {
 public:
  const std::string& getTitle() const override { return title_; }
  bool handleEvent(UIEvent&) override { return false; }

 private:
  std::string title_{"LAYOUT PROBE PAGE"};
};

}  // namespace

int main() {
  UI::publishActivePageTitle("SEMANTIC SENTINEL");

  LayoutProbePage page;
  page.setBoundaries(Rect{4, 16, 232, 93});

  const Rect& bounds = page.getBoundaries();
  assert(bounds.x == 4);
  assert(bounds.y == 16);
  assert(bounds.w == 232);
  assert(bounds.h == 93);

  // UI Constitution V1: geometry propagation is semantically pure.
  // A layout operation must not publish page identity/title/global context.
  assert(std::strcmp(UI::activePageTitle(), "SEMANTIC SENTINEL") == 0);

  return 0;
}
