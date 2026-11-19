#ifndef GROOVEPUTER_TILT_SETTINGS_FILE_H
#define GROOVEPUTER_TILT_SETTINGS_FILE_H
// SD side of the shared TILT settings (see tilt_axis.h for the format). Included by
// the sketch only under GROOVEPUTER_TILT; a missing or unreadable card keeps the defaults.
#include <SD.h>

#include "src/input/tilt_axis.h"
#include "src/platform/cardputer_sd.h"
#include "src/storage/data_root.h"
#include "src/storage/data_root.h"

namespace GroovePuterTilt {

inline constexpr const char* kSettingsPath = GroovePuterStorage::kTiltSettingsFile;

// The card is mounted on demand, like the other features do, and only from a user
// action (the first TILT switch-on, a settings change). Nothing touches it at boot.
inline void loadSettingsFile() {
    if (!GroovePuterPlatform::ensureCardputerSdMounted()) return;
    const std::string source = kSettingsPath;
    if (!SD.exists(source.c_str())) return;
    File f = SD.open(source.c_str(), FILE_READ);
    if (!f) return;
    char buffer[128];
    size_t n = 0;
    while (f.available() && n < sizeof(buffer) - 1) buffer[n++] = static_cast<char>(f.read());
    buffer[n] = '\0';
    f.close();
    applySettings(parseSettings(buffer));
}

inline void saveSettingsFile() {
    if (!GroovePuterPlatform::ensureCardputerSdMounted()) return;
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
