#pragma once

#include <cctype>
#include <cstddef>
#include <cstring>

namespace GroovePuterUi {

inline bool midiAsciiEqualIgnoreCase(char a, char b) {
    return std::tolower(static_cast<unsigned char>(a)) ==
           std::tolower(static_cast<unsigned char>(b));
}

inline bool midiEndsWithExtension(const char* name) {
    if (!name) return false;
    const std::size_t length = std::strlen(name);
    if (length < 4) return false;
    const char* ext = name + length - 4;
    return ext[0] == '.' &&
           midiAsciiEqualIgnoreCase(ext[1], 'm') &&
           midiAsciiEqualIgnoreCase(ext[2], 'i') &&
           midiAsciiEqualIgnoreCase(ext[3], 'd');
}

inline bool midiFilenameIsVisibleAndSupported(const char* name) {
    return name != nullptr && name[0] != '\0' && name[0] != '.' &&
           midiEndsWithExtension(name);
}

inline bool midiRenameCharacterAllowed(char ch) {
    const unsigned char value = static_cast<unsigned char>(ch);
    return std::isalnum(value) || ch == ' ' || ch == '-' || ch == '_' ||
           ch == '(' || ch == ')';
}

inline bool buildMidiFilenameFromStem(const char* input,
                                      char* output,
                                      std::size_t outputSize) {
    if (!input || !output || outputSize < 6) return false;
    output[0] = '\0';

    const char* begin = input;
    while (*begin == ' ') ++begin;
    const char* end = begin + std::strlen(begin);
    while (end > begin && end[-1] == ' ') --end;
    if (end == begin) return false;

    if (static_cast<std::size_t>(end - begin) >= 4) {
        const char* ext = end - 4;
        if (ext[0] == '.' &&
            midiAsciiEqualIgnoreCase(ext[1], 'm') &&
            midiAsciiEqualIgnoreCase(ext[2], 'i') &&
            midiAsciiEqualIgnoreCase(ext[3], 'd')) {
            end = ext;
            while (end > begin && end[-1] == ' ') --end;
        }
    }
    if (end == begin) return false;

    const std::size_t stemLength = static_cast<std::size_t>(end - begin);
    if (stemLength + 5 > outputSize) return false;
    for (std::size_t i = 0; i < stemLength; ++i) {
        if (!midiRenameCharacterAllowed(begin[i])) return false;
        output[i] = begin[i];
    }
    output[stemLength] = '.';
    output[stemLength + 1] = 'm';
    output[stemLength + 2] = 'i';
    output[stemLength + 3] = 'd';
    output[stemLength + 4] = '\0';
    return true;
}

}  // namespace GroovePuterUi
