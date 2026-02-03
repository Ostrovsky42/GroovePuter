#include "test_layout_page.h"
#include "../test_layout_screen.h"

void TestLayoutPage::draw(IGfx& gfx) {
    testAllWidgets(gfx);
}

bool TestLayoutPage::handleEvent(UIEvent& ui_event) {
    // Simple pass-through or basic navigation if needed.
    // The test screen is mostly static visual verification.
    return false; 
}

const std::string& TestLayoutPage::getTitle() const {
    return title_;
}
