#pragma once

// Pure helpers for the PROJECT page list and name editor (no screen, no SD).
#include <cctype>
#include <cstddef>
#include <string>
#include <vector>

namespace ProjectUx {

constexpr std::size_t kMaxNameLen = 32;

// Wrapping list step: Up on item 0 -> last, Down on last -> 0.
inline int wrapIndex(int index, int delta, int count) {
  if (count <= 0) return 0;
  int next = (index + delta) % count;
  if (next < 0) next += count;
  return next;
}

// First visible row so that `cursor` is on screen; keeps `offset` when it
// already shows the cursor (a wrap to the end shows the last rows).
inline int windowStart(int cursor, int count, int visibleRows, int offset) {
  if (visibleRows < 1) visibleRows = 1;
  if (count <= 0) return 0;
  if (cursor < 0) cursor = 0;
  if (cursor > count - 1) cursor = count - 1;
  if (offset > cursor) offset = cursor;
  if (cursor >= offset + visibleRows) offset = cursor - visibleRows + 1;
  int maxStart = count - visibleRows;
  if (maxStart < 0) maxStart = 0;
  if (offset > maxStart) offset = maxStart;
  if (offset < 0) offset = 0;
  return offset;
}

inline bool nameCharAllowed(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_';
}

inline void clampCaret(const std::string& s, std::size_t& caret) {
  if (caret > s.size()) caret = s.size();
}

inline bool insertAtCaret(std::string& s, std::size_t& caret, char c) {
  clampCaret(s, caret);
  if (!nameCharAllowed(c) || s.size() >= kMaxNameLen) return false;
  s.insert(s.begin() + static_cast<std::ptrdiff_t>(caret), c);
  ++caret;
  return true;
}

// Backspace: removes the character before the caret.
inline bool backspaceAtCaret(std::string& s, std::size_t& caret) {
  clampCaret(s, caret);
  if (caret == 0) return false;
  s.erase(s.begin() + static_cast<std::ptrdiff_t>(caret - 1));
  --caret;
  return true;
}

inline bool caretLeft(const std::string& s, std::size_t& caret) {
  clampCaret(s, caret);
  if (caret == 0) return false;
  --caret;
  return true;
}

inline bool caretRight(const std::string& s, std::size_t& caret) {
  clampCaret(s, caret);
  if (caret >= s.size()) return false;
  ++caret;
  return true;
}

inline bool equalsNoCase(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (std::tolower(static_cast<unsigned char>(a[i])) !=
        std::tolower(static_cast<unsigned char>(b[i]))) return false;
  }
  return true;
}

// True when `candidate` equals (case-insensitive, FAT semantics) the name of
// any project other than names[selfIndex]. selfIndex < 0: no exception.
inline bool nameUsedByOther(const std::vector<std::string>& names,
                            const std::string& candidate, int selfIndex) {
  for (std::size_t i = 0; i < names.size(); ++i) {
    if (static_cast<int>(i) == selfIndex) continue;
    if (equalsNoCase(names[i], candidate)) return true;
  }
  return false;
}

}  // namespace ProjectUx
