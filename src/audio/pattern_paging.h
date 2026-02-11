#ifndef PATTERN_PAGING_H
#define PATTERN_PAGING_H

#include <string>
#include "../../scenes.h"

class PatternPagingService {
public:
    static bool savePage(int pageIndex, const Scene& scene);
    static bool loadPage(int pageIndex, Scene& scene);
    static bool ensureDirectory();

private:
    static std::string getSynthAPath(int pageIndex);
    static std::string getSynthBPath(int pageIndex);
    static std::string getDrumsPath(int pageIndex);
};

#endif // PATTERN_PAGING_H
