// isGlobalNav() makes pages refuse a key so the app dispatcher can own it.
// A Ctrl/Fn letter reserved there but not handled by MiniAcidDisplay::handleEvent
// is a dead key: no page can use it and nothing else answers it. This test fails
// when isGlobalNav reserves a Ctrl/Fn letter that the dispatcher does not handle.
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include "src/ui/ui_input.h"

namespace {

int failures = 0;

#define CHECK(cond, msg)                                    \
  do {                                                      \
    if (!(cond)) {                                          \
      std::fprintf(stderr, "FAIL: %s (%s)\n", msg, #cond);  \
      ++failures;                                           \
    }                                                       \
  } while (0)

std::string readDispatcherSource() {
  const char* candidates[] = {
      "../src/ui/miniacid_display.cpp",
      "src/ui/miniacid_display.cpp",
  };
  for (const char* path : candidates) {
    std::ifstream in(path);
    if (!in) continue;
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
  }
  return std::string();
}

UIEvent letterEvent(char key, bool ctrl, bool meta) {
  UIEvent e{};
  e.event_type = GROOVEPUTER_KEY_DOWN;
  e.key = key;
  e.ctrl = ctrl;
  e.meta = meta;
  return e;
}

// The dispatcher handles a modified letter as
// `event.<mod> && (event.key == 'x'` (|| event.key == 'X'), on one line.
bool dispatcherHandles(const std::string& src, const char* mod, char key) {
  const char lower = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
  std::string needle = std::string("event.") + mod + " && (event.key == '";
  needle += lower;
  needle += "'";
  std::string caseForm = "event.key == '";
  caseForm += key;
  caseForm += "'";
  for (size_t pos = src.find(needle); pos != std::string::npos;
       pos = src.find(needle, pos + 1)) {
    const size_t eol = src.find('\n', pos);
    const std::string line = src.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
    if (line.find(caseForm) != std::string::npos) return true;
  }
  return false;
}

}  // namespace

int main() {
  const std::string src = readDispatcherSource();
  CHECK(!src.empty(), "read src/ui/miniacid_display.cpp");
  if (src.empty()) return 1;

  for (char base = 'a'; base <= 'z'; ++base) {
    // 'h' is reserved without modifiers (help), not as a Ctrl/Fn page key.
    if (base == 'h') continue;
    const char variants[] = {base, static_cast<char>(base - 'a' + 'A')};
    for (char key : variants) {
      if (UIInput::isGlobalNav(letterEvent(key, true, false))) {
        char msg[96];
        std::snprintf(msg, sizeof(msg),
                      "isGlobalNav reserves Ctrl+%c but the dispatcher does not handle it", key);
        CHECK(dispatcherHandles(src, "ctrl", key), msg);
      }
      if (UIInput::isGlobalNav(letterEvent(key, false, true))) {
        char msg[96];
        std::snprintf(msg, sizeof(msg),
                      "isGlobalNav reserves Fn+%c but the dispatcher does not handle it", key);
        CHECK(dispatcherHandles(src, "meta", key), msg);
      }
    }
  }

  // Fn+M is the workspace launcher and stays reserved.
  CHECK(UIInput::isGlobalNav(letterEvent('m', false, true)), "Fn+M stays reserved");
  CHECK(dispatcherHandles(src, "meta", 'm'), "dispatcher handles Fn+M");

  // The released combos reach the page.
  const char released[] = {'g', 'd', 'e', 'y', 't', 'p'};
  for (char key : released) {
    CHECK(!UIInput::isGlobalNav(letterEvent(key, true, false)), "Ctrl letter released");
    CHECK(!UIInput::isGlobalNav(letterEvent(key, false, true)), "Fn letter released");
  }
  CHECK(!UIInput::isGlobalNav(letterEvent('m', true, false)), "Ctrl+M released");

  // Unmodified page cycling and help stay global.
  CHECK(UIInput::isGlobalNav(letterEvent('[', false, false)), "[ stays global");
  CHECK(UIInput::isGlobalNav(letterEvent(']', false, false)), "] stays global");

  if (failures) {
    std::fprintf(stderr, "test_global_nav_reserved_keys: %d failure(s)\n", failures);
    return 1;
  }
  std::printf("test_global_nav_reserved_keys: PASS\n");
  return 0;
}
