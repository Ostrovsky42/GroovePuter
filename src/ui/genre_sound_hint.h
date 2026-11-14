#pragma once
#ifndef GROOVEPUTER_SRC_UI_GENRE_SOUND_HINT_H
#define GROOVEPUTER_SRC_UI_GENRE_SOUND_HINT_H

#include <cstring>

// H8 hint on GENRE: the line on the APPLY row, where ENTER also sends the genre's
// sound to SEQTRAK. Text only; the key is ENTER as before.
namespace GenreUi {

// Screen width of the content and the shell line, and the per-character width the
// page harnesses use. A hint that fits 240 px at 6 px per character fits the device line.
constexpr int kScreenWidthPx = 240;
constexpr int kCharWidthPx = 6;

inline const char* applyEnterHint(bool seqtrakProfile) {
    return seqtrakProfile ? "ENT: STYLE + SOUND" : "ENT: STYLE (SOUND: SEQTRAK)";
}

inline bool fitsLine(const char* text) {
    return text != nullptr &&
           static_cast<int>(std::strlen(text)) * kCharWidthPx <= kScreenWidthPx;
}

}  // namespace GenreUi

#endif  // GROOVEPUTER_SRC_UI_GENRE_SOUND_HINT_H
