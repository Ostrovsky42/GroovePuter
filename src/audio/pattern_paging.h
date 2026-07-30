#pragma once

#include "../../scenes.h"
#include <cstdint>
#include <string>

class PatternPagingService {
public:
    static constexpr uint16_t kFormatVersion = 3;

    // Persist or load all pattern banks for one logical page. Both operations
    // are transactional from the caller's perspective: a failed save keeps
    // the previous page file, and a failed load leaves Scene unchanged.
    static bool savePage(int pageIndex, const Scene& scene);
    static bool loadPage(int pageIndex, Scene& scene);

    // Initialize only pattern banks. Scene metadata, songs, sampler settings,
    // genre and mix state remain unchanged.
    static void initializeEmptyPage(Scene& scene);

    static bool pageExists(int pageIndex);
    static bool removePage(int pageIndex);

private:
    static bool ensureDirectory();
    static bool validPageIndex(int pageIndex);
    static std::string pagePath(int pageIndex);
    static std::string tempPath(int pageIndex);
    static std::string backupPath(int pageIndex);
};
