#include <cassert>
#include <cstring>
#include <iostream>

#include "src/ui/global_help_content.h"

namespace {

bool sectionContains(int page, const char* needle) {
    const int pageLines = HelpContent::getPageLineCount(page);
    for (int i = 0; i < pageLines; ++i) {
        const char* line = HelpContent::getLine(page, i);
        if (line && std::strstr(line, needle)) return true;
    }
    return false;
}

bool globalContains(const char* needle) {
    const int globalCount = static_cast<int>(sizeof(HelpContent::kGlobalLines) /
                                             sizeof(HelpContent::kGlobalLines[0]));
    for (int i = 0; i < globalCount; ++i) {
        if (std::strstr(HelpContent::kGlobalLines[i], needle)) return true;
    }
    return false;
}

}  // namespace

int main() {
    constexpr int kFirstPage = WorkflowPages::kGenre;
    constexpr int kLastPage = WorkflowPages::kPhraseCore;

    for (int page = kFirstPage; page <= kLastPage; ++page) {
        const int pageLineCount = HelpContent::getPageLineCount(page);
        assert(pageLineCount > 0);
        assert(std::strcmp(HelpContent::pageTitle(page), "PAGE") != 0);

        const char* first = HelpContent::getLine(page, 0);
        assert(first != nullptr);
        assert(std::strncmp(first, "===", 3) == 0);

        const int total = HelpContent::getTotalLines(page);
        assert(total > pageLineCount);
        assert(HelpContent::getLine(page, total - 1) != nullptr);
        assert(HelpContent::getLine(page, total) == nullptr);

        for (int line = 0; line < total; ++line) {
            const char* text = HelpContent::getLine(page, line);
            assert(text != nullptr);
            assert(std::strlen(text) <= 38u);
        }
    }

    assert(globalContains("Alt+H"));
    assert(globalContains("Fn+M"));
    assert(globalContains("Track mute fallback"));
    assert(globalContains("Waveform except CORE"));
    assert(!globalContains("Ctrl+H"));

    assert(sectionContains(WorkflowPages::kArrange, "Assign existing pattern"));
    assert(sectionContains(WorkflowPages::kArrange, "Generate/materialize cell"));
    assert(sectionContains(WorkflowPages::kArrange, "Generate current row"));
    // PHRASE CORE retains the legacy capture/derive/write help content,
    // split out of PHRASE into its own page in PHW-P1.
    assert(sectionContains(WorkflowPages::kPhraseCore, "PHRASE CORE"));
    assert(sectionContains(WorkflowPages::kPhraseCore, "Mutable pattern references"));
    assert(sectionContains(WorkflowPages::kPhraseCore, "Ctrl+L/R    Move TO row +/-1"));
    assert(sectionContains(WorkflowPages::kPhraseCore, "Ctrl+U/D    Move TO row +/-8"));
    assert(sectionContains(WorkflowPages::kPhraseCore, "INSERT before TO row"));
    assert(sectionContains(WorkflowPages::kPhraseCore, "Shifts following rows"));
    assert(sectionContains(WorkflowPages::kPhraseCore, "REPLACE at TO row"));
    assert(sectionContains(WorkflowPages::kPhraseCore, "No row shift"));

    // PHRASE (product) no longer carries CORE's write/insert help text or a
    // V:CORE mode switch -- placement is APPEND/EXPLICIT via a TO focus.
    assert(sectionContains(WorkflowPages::kPhrase, "=== PHRASE ==="));
    assert(sectionContains(WorkflowPages::kPhrase, "APPEND or EXPLICIT"));
    assert(sectionContains(WorkflowPages::kPhrase, "FREE/OCCUPIED/NO ROOM"));
    assert(!sectionContains(WorkflowPages::kPhrase, "PHRASE CORE"));
    assert(!sectionContains(WorkflowPages::kPhrase, "Mutable pattern references"));
    assert(sectionContains(WorkflowPages::kPerform, "PERFORMANCE TOOLS"));
    assert(sectionContains(WorkflowPages::kPlayer, "Physical track mute"));
    assert(sectionContains(WorkflowPages::kPattern, "SEQUENCER HUB"));
    assert(sectionContains(WorkflowPages::kPattern, "saved per-file route"));

    assert(sectionContains(WorkflowPages::kGenre, "GENRE 1/2"));
    assert(sectionContains(WorkflowPages::kGenre, "No texture or feel changes"));
    assert(sectionContains(WorkflowPages::kFeel, "FEEL 2/2"));
    assert(sectionContains(WorkflowPages::kFeel, "Profile     Straight/Swing/Laid/Push"));
    assert(sectionContains(WorkflowPages::kFeel, "Next gen; clock/pitch unchanged"));

    // Historical GENERATION/TEXTURE page ids remain readable but both resolve
    // to FEEL content; neither is a normal navigation destination anymore.
    assert(std::strcmp(WorkflowPages::pageName(WorkflowPages::kGeneration), "FEEL") == 0);
    assert(sectionContains(WorkflowPages::kGeneration, "FEEL 2/2"));
    assert(!sectionContains(WorkflowPages::kGeneration, "GENERATION"));
    assert(std::strcmp(WorkflowPages::pageName(WorkflowPages::kTexture), "FEEL") == 0);
    assert(sectionContains(WorkflowPages::kTexture, "FEEL 2/2"));
    assert(!sectionContains(WorkflowPages::kTexture, "TEXTURE"));

    std::cout << "global help content tests passed\n";
    return 0;
}
