#include "drum_pattern_automation_page.h"

#include <cctype>
#include <utility>

#include "../ui_colors.h"

DrumPatternAutomationPage::DrumPatternAutomationPage(IGfx &gfx,
                                                     MiniAcid &mini_acid,
                                                     AudioGuard &audio_guard)
    : gfx_(gfx),
      mini_acid_(mini_acid),
      audio_guard_(audio_guard),
      pattern_row_cursor_(0),
      bank_index_(0),
      bank_cursor_(0)
{
    int idx = mini_acid_.currentDrumPatternIndex();
    if (idx < 0 || idx >= Bank<DrumPatternSet>::kPatterns)
        idx = 0;
    pattern_row_cursor_ = idx;
    bank_index_ = mini_acid_.currentDrumBankIndex();
    bank_cursor_ = bank_index_;
    title_ = "DRUM PATTERNS";
    pattern_label_ = std::make_shared<LabelComponent>("PATTERNS");
    pattern_label_->setTextColor(COLOR_LABEL);
    pattern_bar_ = std::make_shared<PatternSelectionBarComponent>("PATTERNS");
    bank_bar_ = std::make_shared<BankSelectionBarComponent>("BANK", "ABCD");
    PatternSelectionBarComponent::Callbacks pattern_callbacks;
    pattern_callbacks.onSelect = [this](int index)
    {
        if (mini_acid_.songModeEnabled())
            return;
        setPatternCursor(index);
        withAudioGuard([&]()
                       { mini_acid_.setDrumPatternIndex(index); });
    };
    pattern_callbacks.onCursorMove = [this](int index)
    {
        if (mini_acid_.songModeEnabled())
            return;
        setPatternCursor(index);
    };
    pattern_bar_->setCallbacks(std::move(pattern_callbacks));
    BankSelectionBarComponent::Callbacks bank_callbacks;
    bank_callbacks.onSelect = [this](int index)
    {
        if (mini_acid_.songModeEnabled())
            return;
        bank_cursor_ = index;
        setBankIndex(index);
    };
    bank_callbacks.onCursorMove = [this](int index)
    {
        if (mini_acid_.songModeEnabled())
            return;
        bank_cursor_ = index;
    };
    bank_bar_->setCallbacks(std::move(bank_callbacks));
    // Add bars as focusable children for keyboard navigation
    bank_bar_->setFocusable(true);
    addChild(bank_bar_);
    pattern_bar_->setFocusable(true);
    addChild(pattern_bar_);
    std::vector<std::shared_ptr<Component>> param_options;
    auto addParamOption = [&](DrumAutomationParamId param_id, const char* label) {
        param_ids_.push_back(param_id);
        param_options.push_back(std::make_shared<DrumAutomationLaneLabel>(
            mini_acid_, param_id, label));
    };
    addParamOption(DrumAutomationParamId::DrumEngine, "ENG");
    combo_box_ = std::make_shared<ComboBoxComponent>(std::move(param_options));
    combo_box_->setFocusable(true);
    addChild(combo_box_);
    automation_editor_ = std::make_shared<DrumAutomationLaneEditor>(mini_acid_, audio_guard_);
    automation_editor_->setFocusable(true);
    addChild(automation_editor_);
}

int DrumPatternAutomationPage::clampCursor(int cursorIndex) const
{
    int cursor = cursorIndex;
    if (cursor < 0)
        cursor = 0;
    if (cursor >= Bank<DrumPatternSet>::kPatterns)
        cursor = Bank<DrumPatternSet>::kPatterns - 1;
    return cursor;
}

int DrumPatternAutomationPage::activeBankCursor() const
{
    int cursor = bank_cursor_;
    if (cursor < 0)
        cursor = 0;
    if (cursor >= kBankCount)
        cursor = kBankCount - 1;
    return cursor;
}

int DrumPatternAutomationPage::patternIndexFromKey(char key) const
{
    switch (std::tolower(static_cast<unsigned char>(key)))
    {
    case 'q':
        return 0;
    case 'w':
        return 1;
    case 'e':
        return 2;
    case 'r':
        return 3;
    case 't':
        return 4;
    case 'y':
        return 5;
    case 'u':
        return 6;
    case 'i':
        return 7;
    default:
        return -1;
    }
}

int DrumPatternAutomationPage::bankIndexFromKey(char key) const
{
    switch (key)
    {
    case '1':
        return 0;
    case '2':
        return 1;
    case '3':
        return 2;
    case '4':
        return 3;
    default:
        return -1;
    }
}

DrumAutomationParamId DrumPatternAutomationPage::selectedParamId() const
{
    if (!combo_box_ || param_ids_.empty())
        return DrumAutomationParamId::DrumEngine;
    int index = combo_box_->selectedIndex();
    if (index < 0)
        index = 0;
    if (index >= static_cast<int>(param_ids_.size()))
        index = static_cast<int>(param_ids_.size()) - 1;
    return param_ids_[index];
}

void DrumPatternAutomationPage::setBankIndex(int bankIndex)
{
    if (bankIndex < 0)
        bankIndex = 0;
    if (bankIndex >= kBankCount)
        bankIndex = kBankCount - 1;
    if (bank_index_ == bankIndex)
        return;
    bank_index_ = bankIndex;
    withAudioGuard([&]()
                   { mini_acid_.setDrumBankIndex(bank_index_); });
}

void DrumPatternAutomationPage::setPatternCursor(int cursorIndex)
{
    pattern_row_cursor_ = clampCursor(cursorIndex);
}

void DrumPatternAutomationPage::withAudioGuard(const std::function<void()> &fn)
{
    if (audio_guard_)
    {
        audio_guard_(fn);
        return;
    }
    fn();
}

int DrumPatternAutomationPage::activePatternCursor() const
{
    return clampCursor(pattern_row_cursor_);
}

bool DrumPatternAutomationPage::handleEvent(UIEvent &ui_event)
{
    // Handle keyboard shortcuts for pattern selection
    if (ui_event.event_type == MINIACID_KEY_DOWN)
    {
        char key = ui_event.key;
        if (key)
        {
            int patternIdx = patternIndexFromKey(key);
            if (patternIdx >= 0)
            {
                bool patternRowFocused = (focusedChild() == pattern_bar_.get());
                char lowerKey = static_cast<char>(std::tolower(static_cast<unsigned char>(key)));
                bool patternKeyReserved = (lowerKey == 'q' || lowerKey == 'w');
                if (!patternKeyReserved || patternRowFocused)
                {
                    if (!mini_acid_.songModeEnabled())
                    {
                        setPatternCursor(patternIdx);
                        withAudioGuard([&]()
                                       { mini_acid_.setDrumPatternIndex(patternIdx); });
                        return true;
                    }
                }
            }

            if (key == '\n' || key == '\r')
            {
                if (focusedChild() == bank_bar_.get())
                {
                    if (!mini_acid_.songModeEnabled())
                    {
                        setBankIndex(activeBankCursor());
                        return true;
                    }
                }
                if (focusedChild() == pattern_bar_.get())
                {
                    if (!mini_acid_.songModeEnabled())
                    {
                        int cursor = activePatternCursor();
                        setPatternCursor(cursor);
                        withAudioGuard([&]()
                                       { mini_acid_.setDrumPatternIndex(cursor); });
                        return true;
                    }
                }
            }
        }
    }

    // Let Container handle the rest (including mouse clicks and focus navigation)
    return Container::handleEvent(ui_event);
}

void DrumPatternAutomationPage::draw(IGfx &gfx)
{
    bank_index_ = mini_acid_.currentDrumBankIndex();
    const Rect &bounds = getBoundaries();
    int x = bounds.x;
    int y = bounds.y;
    int w = bounds.w;
    int h = bounds.h;

    int body_y = y + 2;
    int body_h = h - 2;
    if (body_h <= 0)
        return;

    int selectedPattern = mini_acid_.displayDrumPatternIndex();
    bool songMode = mini_acid_.songModeEnabled();
    bool patternFocus = !songMode && pattern_bar_ && pattern_bar_->isFocused();
    bool bankFocus = !songMode && bank_bar_ && bank_bar_->isFocused();
    int patternCursor = songMode && selectedPattern >= 0 ? selectedPattern : activePatternCursor();
    int bankCursor = activeBankCursor();
    int label_h = gfx.fontHeight();
    if (pattern_label_)
    {
        pattern_label_->setBoundaries(Rect{x, body_y, w, label_h});
        pattern_label_->draw(gfx);
    }
    int pattern_bar_y = body_y + label_h + 1;

    PatternSelectionBarComponent::State pattern_state;
    pattern_state.pattern_count = Bank<DrumPatternSet>::kPatterns;
    pattern_state.selected_index = selectedPattern;
    pattern_state.cursor_index = patternCursor;
    pattern_state.show_cursor = patternFocus;
    pattern_state.song_mode = songMode;
    pattern_bar_->setState(pattern_state);
    pattern_bar_->setBoundaries(Rect{x, pattern_bar_y, w, 0});
    int pattern_bar_h = pattern_bar_->barHeight(gfx);
    pattern_bar_->setBoundaries(Rect{x, pattern_bar_y, w, pattern_bar_h});

    BankSelectionBarComponent::State bank_state;
    bank_state.bank_count = kBankCount;
    bank_state.selected_index = bank_index_;
    bank_state.cursor_index = bankCursor;
    bank_state.show_cursor = bankFocus;
    bank_state.song_mode = songMode;
    bank_bar_->setState(bank_state);
    bank_bar_->setBoundaries(Rect{x, body_y - 1, w, 0});
    int bank_bar_h = bank_bar_->barHeight(gfx);
    bank_bar_->setBoundaries(Rect{x, body_y - 1, w, bank_bar_h});

    // Draw pattern bar first, then bank bar on top
    pattern_bar_->draw(gfx);
    bank_bar_->draw(gfx);

    if (combo_box_)
    {
        int combo_w = (w * 3) / 10;
        if (combo_w < 1)
            combo_w = 1;
        int row_h = gfx.fontHeight() + 2;
        int combo_h = row_h * combo_box_->optionCount();
        int combo_y = pattern_bar_y + pattern_bar_h + 6;
        combo_box_->setBoundaries(Rect{x, combo_y, combo_w, combo_h});
    }
    if (automation_editor_)
    {
        int combo_w = combo_box_ ? combo_box_->getBoundaries().w : 0;
        int gap = 6;
        int editor_x = x + combo_w + gap;
        int editor_y = pattern_bar_y + pattern_bar_h + 6;
        int editor_w = w - combo_w - gap;
        int editor_h = body_h - (editor_y - body_y);
        if (editor_w < 0)
            editor_w = 0;
        if (editor_h < 0)
            editor_h = 0;
        automation_editor_->setBoundaries(Rect{editor_x, editor_y, editor_w, editor_h});
        automation_editor_->setParamId(selectedParamId());
    }
    if (combo_box_) {
        combo_box_->draw(gfx);
    }
    if (automation_editor_) {
        automation_editor_->draw(gfx);
    }
}
