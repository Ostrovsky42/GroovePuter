#pragma once

#include "../ui_core.h"
#include <string>

class TestLayoutPage : public IPage {
public:
    TestLayoutPage() = default;
    
    void draw(IGfx& gfx) override;
    bool handleEvent(UIEvent& ui_event) override;
    const std::string& getTitle() const override;

private:
    std::string title_ = "UI KIT V0";
};
