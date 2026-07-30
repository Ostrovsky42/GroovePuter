#include "pattern_paging.h"

#if defined(ARDUINO)
#include <Arduino.h>
#include <SD.h>
#else
#include "../../platform_sdl/arduino_compat.h"
#endif

#include <cstdio>
#include <cstring>

static const char* kPatternDir = "/patterns";
static constexpr uint32_t kPageVersion = 2;

bool PatternPagingService::ensureDirectory() {
#if defined(ARDUINO)
    if (!SD.exists(kPatternDir)) {
        return SD.mkdir(kPatternDir);
    }
#endif
    return true;
}

std::string PatternPagingService::getSynthAPath(int pageIndex) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s/synthA_p%d.bin", kPatternDir, pageIndex);
    return std::string(buf);
}

std::string PatternPagingService::getSynthBPath(int pageIndex) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s/synthB_p%d.bin", kPatternDir, pageIndex);
    return std::string(buf);
}

std::string PatternPagingService::getDrumsPath(int pageIndex) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s/drums_p%d.bin", kPatternDir, pageIndex);
    return std::string(buf);
}

bool PatternPagingService::savePage(int pageIndex, const Scene& scene) {
#if !defined(ARDUINO)
    (void)pageIndex;
    (void)scene;
    return false;
#else
    if (!ensureDirectory()) return false;

    auto saveFile = [&](const std::string& path, const void* data, size_t size) {
        File f = SD.open(path.c_str(), FILE_WRITE);
        if (!f) return false;
        const size_t headerWritten =
            f.write(reinterpret_cast<const uint8_t*>(&kPageVersion), sizeof(kPageVersion));
        const size_t payloadWritten =
            f.write(reinterpret_cast<const uint8_t*>(data), size);
        f.flush();
        f.close();
        return headerWritten == sizeof(kPageVersion) && payloadWritten == size;
    };

    return saveFile(getSynthAPath(pageIndex), &scene.synthABanks,
                    sizeof(scene.synthABanks)) &&
           saveFile(getSynthBPath(pageIndex), &scene.synthBBanks,
                    sizeof(scene.synthBBanks)) &&
           saveFile(getDrumsPath(pageIndex), &scene.drumBanks,
                    sizeof(scene.drumBanks));
#endif
}

bool PatternPagingService::loadPage(int pageIndex, Scene& scene) {
#if !defined(ARDUINO)
    (void)pageIndex;
    (void)scene;
    return false;
#else
    auto validateFile = [&](const std::string& path, size_t size) {
        if (!SD.exists(path.c_str())) return false;
        File f = SD.open(path.c_str(), FILE_READ);
        if (!f) return false;
        uint32_t version = 0;
        const bool valid =
            f.read(reinterpret_cast<uint8_t*>(&version), sizeof(version)) == sizeof(version) &&
            version == kPageVersion &&
            f.size() == size + sizeof(kPageVersion);
        f.close();
        return valid;
    };

    const std::string synthAPath = getSynthAPath(pageIndex);
    const std::string synthBPath = getSynthBPath(pageIndex);
    const std::string drumsPath = getDrumsPath(pageIndex);

    // Validate the complete page before touching active scene memory. Missing,
    // truncated, or version-mismatched pages leave the current page unchanged.
    if (!validateFile(synthAPath, sizeof(scene.synthABanks)) ||
        !validateFile(synthBPath, sizeof(scene.synthBBanks)) ||
        !validateFile(drumsPath, sizeof(scene.drumBanks))) {
        return false;
    }

    auto loadFile = [&](const std::string& path, void* data, size_t size) {
        File f = SD.open(path.c_str(), FILE_READ);
        if (!f) return false;
        uint32_t version = 0;
        if (f.read(reinterpret_cast<uint8_t*>(&version), sizeof(version)) != sizeof(version) ||
            version != kPageVersion) {
            f.close();
            return false;
        }
        const size_t read = f.read(reinterpret_cast<uint8_t*>(data), size);
        f.close();
        return read == size;
    };

    // The files were fully validated above, so normal corruption/missing-file
    // failures cannot partially clear the scene. A later revision will replace
    // the raw layout with a single checksummed schema file.
    return loadFile(synthAPath, &scene.synthABanks, sizeof(scene.synthABanks)) &&
           loadFile(synthBPath, &scene.synthBBanks, sizeof(scene.synthBBanks)) &&
           loadFile(drumsPath, &scene.drumBanks, sizeof(scene.drumBanks));
#endif
}
