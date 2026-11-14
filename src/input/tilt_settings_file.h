#ifndef GROOVEPUTER_TILT_SETTINGS_FILE_H
#define GROOVEPUTER_TILT_SETTINGS_FILE_H
// SD side of the shared TILT settings (see tilt_axis.h for the format). Included by
// the sketch only under GROOVEPUTER_TILT; a missing or unreadable card keeps the defaults.
#include <SD.h>

#include "src/input/tilt_axis.h"

namespace GroovePuterTilt {

inline constexpr const char* kSettingsPath = "/grooveputer_tilt.txt";

inline void loadSettingsFile() {
    if (!SD.exists(kSettingsPath)) return;
    File f = SD.open(kSettingsPath, FILE_READ);
    if (!f) return;
    char buffer[128];
    size_t n = 0;
    while (f.available() && n < sizeof(buffer) - 1) buffer[n++] = static_cast<char>(f.read());
    buffer[n] = '\0';
    f.close();
    applySettings(parseSettings(buffer));
}

inline void saveSettingsFile() {
    char body[96];
    formatSettings(currentSettings(), body, sizeof(body));
    if (SD.exists(kSettingsPath)) SD.remove(kSettingsPath);   // FILE_WRITE appends
    File f = SD.open(kSettingsPath, FILE_WRITE);
    if (!f) return;
    f.print(body);
    f.close();
}

}  // namespace GroovePuterTilt

#endif  // GROOVEPUTER_TILT_SETTINGS_FILE_H
