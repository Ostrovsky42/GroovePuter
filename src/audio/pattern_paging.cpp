#include "pattern_paging.h"
#include <SD.h>
#include <Arduino.h>

static const char* kPatternDir = "/patterns";

bool PatternPagingService::ensureDirectory() {
    if (!SD.exists(kPatternDir)) {
        return SD.mkdir(kPatternDir);
    }
    return true;
}

std::string PatternPagingService::getSynthAPath(int pageIndex) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s/synthA_p%d.bin", kPatternDir, pageIndex);
    return std::string(buf);
}

std::string PatternPagingService::getSynthBPath(int pageIndex) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s/synthB_p%d.bin", kPatternDir, pageIndex);
    return std::string(buf);
}

std::string PatternPagingService::getDrumsPath(int pageIndex) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%s/drums_p%d.bin", kPatternDir, pageIndex);
    return std::string(buf);
}

bool PatternPagingService::savePage(int pageIndex, const Scene& scene) {
    if (!ensureDirectory()) return false;

    auto saveFile = [&](const std::string& path, const void* data, size_t size) {
        File f = SD.open(path.c_str(), FILE_WRITE);
        if (!f) return false;
        size_t written = f.write((const uint8_t*)data, size);
        f.close();
        return written == size;
    };

    bool ok = true;
    ok &= saveFile(getSynthAPath(pageIndex), &scene.synthABanks, sizeof(scene.synthABanks));
    ok &= saveFile(getSynthBPath(pageIndex), &scene.synthBBanks, sizeof(scene.synthBBanks));
    ok &= saveFile(getDrumsPath(pageIndex), &scene.drumBanks, sizeof(scene.drumBanks));

    return ok;
}

bool PatternPagingService::loadPage(int pageIndex, Scene& scene) {
    auto loadFile = [&](const std::string& path, void* data, size_t size) {
        if (!SD.exists(path.c_str())) return false;
        File f = SD.open(path.c_str(), FILE_READ);
        if (!f) return false;
        if (f.size() != size) {
            f.close();
            return false;
        }
        size_t read = f.read((uint8_t*)data, size);
        f.close();
        return read == size;
    };

    bool ok = true;
    
    // If files don't exist, we just leave the current patterns or clear them?
    // The plan says "load requested page, or clear if missing".
    
    if (!loadFile(getSynthAPath(pageIndex), &scene.synthABanks, sizeof(scene.synthABanks))) {
        memset(&scene.synthABanks, 0, sizeof(scene.synthABanks));
        ok = false;
    }
    if (!loadFile(getSynthBPath(pageIndex), &scene.synthBBanks, sizeof(scene.synthBBanks))) {
        memset(&scene.synthBBanks, 0, sizeof(scene.synthBBanks));
        ok = false;
    }
    if (!loadFile(getDrumsPath(pageIndex), &scene.drumBanks, sizeof(scene.drumBanks))) {
        memset(&scene.drumBanks, 0, sizeof(scene.drumBanks));
        ok = false;
    }

    return ok;
}
