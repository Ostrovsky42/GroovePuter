#pragma once

#include "../../scenes.h"
#include <cstdint>
#include <string>

class PatternPagingService {
public:
    static constexpr uint16_t kFormatVersion = 3;

    // Select the project namespace used by all following page operations.
    // Names are encoded before becoming SD paths; callers pass scene names.
    static bool setProjectName(const std::string& projectName);
    static const std::string& currentProjectName();
    static int activePageIndex();

    // Persist or load all pattern banks for one logical page. Both operations
    // are transactional from the caller's perspective: a failed save keeps
    // the previous page file, and a failed load leaves Scene unchanged.
    static bool savePage(int pageIndex, const Scene& scene);
    static bool loadPage(int pageIndex, Scene& scene);

    // Restore the previous validated page version created by savePage().
    // Used by multi-page import transactions when a later page fails.
    static bool restoreBackup(int pageIndex);

    // Initialize only pattern banks. Scene metadata, songs, sampler settings,
    // genre and mix state remain unchanged.
    static void initializeEmptyPage(Scene& scene);

    static bool pageExists(int pageIndex);
    static bool removePage(int pageIndex);

    // Project lifecycle helpers. Copy is used by Save As. New/Clear remove
    // every page plus transactional .tmp/.bak siblings in one project only.
    static bool copyProjectPages(const std::string& sourceProject,
                                 const std::string& targetProject);
    static bool clearProjectPages();
    static bool clearProjectPages(const std::string& projectName);

    // Compatibility entry point for the existing SceneManager constructor.
    // It will be removed when early SD access is migrated out of static init.
    static bool ensureDirectory();

private:
    static bool validPageIndex(int pageIndex);
    static std::string projectDirectory(const std::string& projectName);
    static std::string pagePathForProject(const std::string& projectName,
                                          int pageIndex);
    static std::string pagePath(int pageIndex);
    static std::string tempPath(int pageIndex);
    static std::string backupPath(int pageIndex);
};
