#pragma once

#include <cstdio>
#include <cstring>

namespace UI {

inline char* activePageTitleStorage() {
  static char title[48] = {0};
  return title;
}

inline void publishActivePageTitle(const char* title) {
  char* storage = activePageTitleStorage();
  const char* source = title ? title : "";
  if (std::strncmp(storage, source, 47) == 0 &&
      storage[47] == '\0') {
    return;
  }
  std::snprintf(storage, 48, "%s", source);
}

inline const char* activePageTitle() {
  return activePageTitleStorage();
}

}  // namespace UI
