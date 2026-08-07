#include "pattern_paging.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

namespace {
constexpr char kPatternRootDirectory[] = "/patterns";

bool removeIfPresent(const std::string& path) {
    return !SD.exists(path.c_str()) || SD.remove(path.c_str());
}
}  // namespace

bool PatternPagingService::clearProjectPages(const std::string& projectName) {
    if (!(SD.exists(kPatternRootDirectory) || SD.mkdir(kPatternRootDirectory))) {
        return false;
    }

    const std::string directory = projectDirectory(projectName);
    if (!(SD.exists(directory.c_str()) || SD.mkdir(directory.c_str()))) {
        return false;
    }

    bool ok = true;
    for (int page = 0; page < kMaxPages; ++page) {
        const std::string mainPath = pagePathForProject(projectName, page);
        ok = removeIfPresent(mainPath) && ok;
        ok = removeIfPresent(mainPath + ".tmp") && ok;
        ok = removeIfPresent(mainPath + ".bak") && ok;
    }
    return ok;
}
