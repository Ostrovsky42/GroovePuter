#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "../ui_common.h"
#include "../selection_popup.h"
#include "../ui_view_continuity.h"
#include "../workflow_mode.h"
#include "src/input/performance_keyboard.h"

enum class PerformanceToolContext : uint8_t {
    Key = 0,
    Chord,
    Arp,
    Rhythm,
    Input,
    Loop,  // [LOOP]: the looper, nine rows (0.9.23)
    Count,
};

// PERFORM > INPUT rows that take the cursor, in screen order. The group
// headings "--- MIDI IN ---" and "--- HELPER ---" are drawn above MidiIn and
// Helper and never take the cursor (Up/Down skip them). The KEY context owns
// TARGET/OUT; the USB role is shown on PROJECT > MIDI. The whole looper
// (PerformLoopRow) lives on the [LOOP] tab.
// 0.9.23 T14: INPUT was 20 rows (LOOPER 5..13, PARTNER 14..19); after the LOOP
// tab 14 rows (LOOP SETUP 5..7, HELPER 8..13); now 11 rows.
enum class PerformInputRow : uint8_t {
    // --- MIDI IN ---
    MidiIn = 0,     // MIDI IN: external MIDI input on/off
    ListenChannel,  // LISTEN CH: OMNI or CH1..16
    SendTo,         // SEND TO: where external MIDI notes go
    PlayAs,         // PLAY AS: live-note role
    LeadStyle,      // LEAD STYLE: lead interpretation (PLAY AS LEAD)
    // --- HELPER (the PARTNER) ---
    Helper,         // HELPER: OFF/ANSWER/DUET/RIFF (RIFF = Mode::Loop)
    ReplyBars,      // REPLY BARS
    ReplyChannel,   // REPLY CH
    FreezeRiff,     // FREEZE RIFF (listener loopHold)
    Variation,      // VARIATION (listener loopEvolve)
    NewRiffIf,      // NEW RIFF IF (listener loopReplace)
    Count,
};

constexpr uint8_t performInputRow(PerformInputRow row) { return static_cast<uint8_t>(row); }

// PERFORM > LOOP: all looper settings, nine rows (the window shows six and
// scrolls). TO MELODY is the last row.
enum class PerformLoopRow : uint8_t {
    Looper = 0,     // LOOPER (Enter = Z)
    RecordSource,   // REC: KEYS / SOUND
    LoopBars,       // LOOP BARS
    LoopChannel,    // LOOP CH (0 = DX ROUTE)
    NoteLength,     // NOTE LENGTH
    HoldLayer,      // HOLD LAYER (B)
    Layer,          // LAYER (M)
    MuteLayer,      // MUTE LAYER
    ToMelody,       // TO MELODY (Left/Right A/B, Enter twice)
    Count,
};

constexpr uint8_t performLoopRow(PerformLoopRow row) { return static_cast<uint8_t>(row); }

class PerformPage final : public IPage {
public:
    PerformPage(IGfx& gfx,
                MiniAcid& miniAcid,
                PerformanceKeyboard& keyboard,
                AudioGuard audioGuard = AudioGuard{});

    const std::string& getTitle() const override { return title_; }
    bool handleEvent(UIEvent& event) override;
    void drawHeader(IGfx& gfx) override;
    void drawContent(IGfx& gfx) override;
    void drawFooter(IGfx& gfx) override;
    // LOOPER: leaving PERFORM ends recording; on the free clock the loop stops.
    void onExit() override;

    // PERFORM > INPUT (see PerformInputRow): MIDI IN rows, then the HELPER
    // (PARTNER) rows. PERFORM > LOOP (see PerformLoopRow): the looper and all
    // its settings, TO MELODY last (ENTER twice: the unmuted layers become the
    // Working Melody of the picked synth). The groups are addressed by name,
    // never by number.
    static constexpr uint8_t kInputRowCount = performInputRow(PerformInputRow::Count);
    static constexpr uint8_t kPartnerRowFirst = performInputRow(PerformInputRow::Helper);
    static constexpr uint8_t kPartnerRowLast = performInputRow(PerformInputRow::NewRiffIf);
    static constexpr uint8_t kLoopRowCount = performLoopRow(PerformLoopRow::Count);
    static_assert(kPartnerRowLast + 1 == kInputRowCount, "NEW RIFF IF is the last INPUT row");
    static_assert(kLoopRowCount == 9, "the LOOP tab has nine rows");
    static_assert(kLoopRowCount <= 16 && kInputRowCount <= 16,
                  "a row index fits the continuity byte per tab");
    static_assert(static_cast<uint8_t>(PerformanceToolContext::Count) <= 8,
                  "the context fits the 3-bit continuity field");

private:
    void captureViewContinuity(UI::UiViewContinuityState& state) const override {
        state.performToolsVisible = toolsLayerVisible_ ? 1u : 0u;
        state.performContext = static_cast<uint8_t>(selectedContext_);
        for (int i = 0; i < static_cast<int>(PerformanceToolContext::Count); ++i) {
            state.performRows[i] = selectedRow_[i];
        }
    }

    void restoreViewContinuity(const UI::UiViewContinuityState& state) override {
        toolsLayerVisible_ = state.performToolsVisible != 0;
        uint8_t context = state.performContext;
        if (context >= static_cast<uint8_t>(PerformanceToolContext::Count)) context = 0;
        selectedContext_ = static_cast<PerformanceToolContext>(context);
        constexpr uint8_t kMaxRowByContext[6] = {5, 5, 5, 5, kInputRowCount - 1, kLoopRowCount - 1};
        for (int i = 0; i < static_cast<int>(PerformanceToolContext::Count); ++i) {
            selectedRow_[i] = state.performRows[i] <= kMaxRowByContext[i]
                ? state.performRows[i]
                : 0;
        }
    }

    static const char* noteName(int midiNote);
    bool handleToolKey(const UIEvent& event);
    void moveContext(int direction);
    void moveRow(int direction);
    void adjustSelectedValue(int direction);
    void toggleSelectedValue();
    void cyclePartnerMode(int direction);
    void cyclePartnerSetting(uint8_t row, int direction);
    enum class LooperSetting : uint8_t {
        RecordSource, LoopBars, LoopChannel, NoteLength, HoldLayer, Layer, MuteLayer,
    };
    void cycleLooperSetting(LooperSetting setting, int direction);
    bool looperKey(char key);
    void exportLooperToMelody();
    void formatLooperState(char* out, std::size_t size) const;
    void drawLooperStrip(IGfx& gfx, int y);
    void cycleOutput(int direction);
    void adjustInput(int direction);
    uint8_t rowCountForContext() const;
    uint8_t currentRow() const;
    const char* selectedRowHint() const;
    void drawToolTabs(IGfx& gfx, int y);
    void drawToolsLayer(IGfx& gfx);
    void openChordModePopup();
    void applyChordModePopup();

    MiniAcid& miniAcid_;
    PerformanceKeyboard& keyboard_;
    AudioGuard audioGuard_;
    // TO MELODY: the first ENTER arms (toast: what would be taken); the second
    // ENTER within kExportConfirmMs on the same voice / loop takes it.
    static constexpr uint32_t kExportConfirmMs = 4000;
    bool exportArmed_{false};
    bool dxRouteNoted_{false};  // the DX ROUTE toast shows once per page life
    uint32_t exportArmedMs_{0};
    int8_t exportVoice_{0};
    int8_t exportTarget_{-1};  // TO MELODY synth picked with LEFT/RIGHT; -1 = follow GEN TARGET
    int exportVoiceNow() const;
    uint8_t exportBars_{0};
    uint8_t exportMuteMask_{0};
    uint8_t exportNotes_{0};
    bool toolsLayerVisible_{false};
    // Local navigation state only: the selected context and, per context, the
    // selected row. Musical values are always read from PerformanceKeyboard.
    PerformanceToolContext selectedContext_{PerformanceToolContext::Key};
    uint8_t selectedRow_[static_cast<int>(PerformanceToolContext::Count)]{};
    UI::SelectionPopup chordModePopup_;
    std::string title_{"PERFORM"};
};
