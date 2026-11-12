// The APPLY-row hint on GENRE: the text for each profile and its width (a text that
// fits 240 px at 6 px per character fits the line).
#include <cassert>
#include <cstdio>
#include <cstring>

#include "src/ui/genre_sound_hint.h"

int main() {
    const char* seqtrak = GenreUi::applyEnterHint(true);
    const char* other = GenreUi::applyEnterHint(false);
    assert(seqtrak != nullptr && other != nullptr);
    assert(GenreUi::fitsLine(seqtrak));
    assert(GenreUi::fitsLine(other));
    assert(std::strstr(seqtrak, "ENT") != nullptr && std::strstr(seqtrak, "SOUND") != nullptr);
    assert(std::strstr(seqtrak, "SEQTRAK") == nullptr);  // the profile is named only when it is missing
    assert(std::strstr(other, "SEQTRAK") != nullptr);     // and it says how to get the sound
    // The width rule itself: 40 characters is the longest line that fits 240 px.
    assert(GenreUi::fitsLine("0123456789012345678901234567890123456789"));
    assert(!GenreUi::fitsLine("01234567890123456789012345678901234567890"));
    assert(!GenreUi::fitsLine(nullptr));
    std::printf("GENRE APPLY hint: OK (%zu and %zu chars, fits %d px)\n",
                std::strlen(seqtrak), std::strlen(other), GenreUi::kScreenWidthPx);
    return 0;
}
