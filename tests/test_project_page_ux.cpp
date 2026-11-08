// PROJECT page: list wrap, name-editor caret, duplicate-name check (pure).
#include <cstdio>
#include <string>
#include <vector>

#include "src/ui/project_page_ux.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++failures; } } while (0)

int main() {
  using namespace ProjectUx;
  // Wrap.
  CHECK(wrapIndex(0, -1, 5) == 4);
  CHECK(wrapIndex(4, 1, 5) == 0);
  CHECK(wrapIndex(2, 1, 5) == 3);
  CHECK(wrapIndex(2, -1, 5) == 1);
  CHECK(wrapIndex(0, -1, 1) == 0);
  CHECK(wrapIndex(0, 1, 0) == 0);
  // Window follows the cursor after a wrap.
  CHECK(windowStart(19, 20, 10, 0) == 10);   // Up from first: last rows shown
  CHECK(windowStart(0, 20, 10, 10) == 0);    // Down from last: back to top
  CHECK(windowStart(5, 20, 10, 3) == 3);     // already visible: unchanged
  CHECK(windowStart(3, 4, 10, 2) == 0);      // short list
  for (int cur = 0; cur < 20; ++cur) {
    int off = windowStart(cur, 20, 10, 7);
    CHECK(cur >= off && cur < off + 10);
  }
  // Caret editing.
  std::string s = "abc";
  std::size_t c = 3;
  CHECK(insertAtCaret(s, c, 'd') && s == "abcd" && c == 4);
  CHECK(caretLeft(s, c) && caretLeft(s, c) && c == 2);
  CHECK(insertAtCaret(s, c, 'X') && s == "abXcd" && c == 3);
  CHECK(backspaceAtCaret(s, c) && s == "abcd" && c == 2);
  c = 0;
  CHECK(!caretLeft(s, c) && !backspaceAtCaret(s, c) && s == "abcd");
  c = 4;
  CHECK(!caretRight(s, c) && c == 4);
  c = 0;
  CHECK(caretRight(s, c) && c == 1);
  CHECK(!insertAtCaret(s, c, ' ') && !insertAtCaret(s, c, '.') && s == "abcd");
  CHECK(insertAtCaret(s, c, '-') && s == "a-bcd");
  std::string full(kMaxNameLen, 'z');
  c = 5;
  CHECK(!insertAtCaret(full, c, 'q') && full.size() == kMaxNameLen);
  std::size_t wild = 99;
  CHECK(insertAtCaret(s, wild, '_') && s.back() == '_');
  // Duplicate names.
  std::vector<std::string> names = {"Alpha", "beta", "Gamma"};
  CHECK(nameUsedByOther(names, "beta", 0));
  CHECK(nameUsedByOther(names, "BETA", 0));
  CHECK(!nameUsedByOther(names, "beta", 1));   // own name is fine
  CHECK(!nameUsedByOther(names, "delta", 0));
  CHECK(nameUsedByOther(names, "alpha", -1));
  std::printf(failures ? "test_project_page_ux FAIL\n" : "test_project_page_ux PASS\n");
  return failures ? 1 : 0;
}
