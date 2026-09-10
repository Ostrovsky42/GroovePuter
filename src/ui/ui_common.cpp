#include "ui_common.h"
#include "ui_utils.h"
#include "ui_widgets.h"
#include "ui_theme.h"
#include "src/dsp/miniacid_engine.h"
#include "src/midi/smf_player_service.h"
#include "src/midi/transport_clock_runtime.h"
#include "src/pattern/pattern_address.h"
#include "retro_ui_theme.h"
#include "amber_ui_theme.h"
#include <cstdio>
#include <cstring>
#ifndef ARDUINO
#include "../../platform_sdl/arduino_compat.h"
#endif

namespace UI {

    // Global overlay state
    WaveformOverlayState waveformOverlay;
    VisualStyle currentStyle = VisualStyle::RETRO_CLASSIC;
    IGfxColor currentGenreAccent = IGfxColor(0);
    bool hintOverlayActive = false;

    // Internal state for the compact global audio waveform.
    namespace {
        MiniAcid* gActiveEngine = nullptr;
        int sVuDecay = 0;
        constexpr int kOverlayMaxPoints = 256;
        int16_t overlayWave[kOverlayMaxPoints];
        int overlayLength = 0;

        bool g_hintOverlayHeld = false;
        uint32_t g_hPressStartMs = 0;
        bool g_hintOverlayTimedActive = false;
        uint32_t g_hintOverlayTimedUntilMs = 0;

        char gToastMsg[64] = {0};
        unsigned long gToastEndMs = 0;

        char gInfoLeft[64] = {0};
        char gInfoRight[32] = {0};
        bool gInfoValid = false;

        UiStatusSnapshot gStatusSnapshot{};
        char gStatusLine[48] = {0};
        bool gStatusInitialized = false;
        UiShellFrameModel* gShellFrameModel = nullptr;

        uint16_t statusCount(uint32_t value) {
            if (value == 0) return 1;
            if (value > 65535u) return 65535u;
            return static_cast<uint16_t>(value);
        }

        uint16_t statusOneBasedIndex(int value) {
            if (value < 0) return 1;
            const uint32_t oneBased = static_cast<uint32_t>(value) + 1u;
            return statusCount(oneBased);
        }

        bool smfStateOwnsStatus(GroovePuterMidi::SmfPlayerState state) {
            using GroovePuterMidi::SmfPlayerState;
            return state == SmfPlayerState::Loading ||
                   state == SmfPlayerState::Armed ||
                   state == SmfPlayerState::Playing ||
                   state == SmfPlayerState::Paused;
        }

        UiStatusState uiStateForSmf(GroovePuterMidi::SmfPlayerState state) {
            using GroovePuterMidi::SmfPlayerState;
            switch (state) {
                case SmfPlayerState::Loading: return UiStatusState::Loading;
                case SmfPlayerState::Armed: return UiStatusState::Armed;
                case SmfPlayerState::Playing: return UiStatusState::Play;
                case SmfPlayerState::Paused: return UiStatusState::Pause;
                case SmfPlayerState::Error: return UiStatusState::Error;
                case SmfPlayerState::Unloaded:
                case SmfPlayerState::Stopped:
                    return UiStatusState::Stop;
            }
            return UiStatusState::Stop;
        }

        UiSequencedSource uiSequencedSourceForEngine(
            MiniAcid::SequencedSource source) {
            return source == MiniAcid::SequencedSource::Phrase
                ? UiSequencedSource::Phrase
                : UiSequencedSource::Pattern;
        }

        UiSequencedSource sequencedSourceForContext(
            MiniAcid& miniAcid,
            UiStatusContext context) {
            switch (context) {
                case UiStatusContext::SynthA:
                    return uiSequencedSourceForEngine(
                        miniAcid.currentSequencedSource(0));
                case UiStatusContext::SynthB:
                    return uiSequencedSourceForEngine(
                        miniAcid.currentSequencedSource(1));
                case UiStatusContext::Drums:
                    return UiSequencedSource::Pattern;
                default:
                    return UiSequencedSource::NotApplicable;
            }
        }

        void populatePatternAddress(UiStatusSnapshot& status,
                                    MiniAcid& miniAcid) {
            if (status.routing.sequencedSource() !=
                UiSequencedSource::Pattern) {
                return;
            }

            int bank = -1;
            int slot = -1;
            switch (status.context) {
                case UiStatusContext::SynthA:
                    bank = miniAcid.current303BankIndex(0);
                    slot = miniAcid.display303LocalPatternIndex(0);
                    break;
                case UiStatusContext::SynthB:
                    bank = miniAcid.current303BankIndex(1);
                    slot = miniAcid.display303LocalPatternIndex(1);
                    break;
                case UiStatusContext::Drums:
                    bank = miniAcid.currentDrumBankIndex();
                    slot = miniAcid.displayDrumLocalPatternIndex();
                    break;
                default:
                    return;
            }

            const PatternAddress address = patternAddressFromParts(
                miniAcid.currentPageIndex(), bank, slot);
            if (!address.valid()) return;
            status.patternPage = static_cast<uint8_t>(address.page);
            status.patternBank = static_cast<uint8_t>(address.bank);
            status.patternSlot = static_cast<uint8_t>(address.slot);
        }

        UiStatusSnapshot buildUiStatusSnapshot(MiniAcid& miniAcid,
                                               UiStatusContext context) {
            UiStatusSnapshot status{};
            status.context = context;
            status.bpm = normalizeUiStatusBpm(static_cast<int>(miniAcid.bpm()));
            status.liveMixLocked = miniAcid.liveMixModeEnabled();

            const UiSequencedSource sequencedSource =
                sequencedSourceForContext(miniAcid, context);
            const UiTransportOwner defaultTransportOwner =
                miniAcid.songModeEnabled()
                    ? UiTransportOwner::Song
                    : UiTransportOwner::Cycle;
            status.routing = UiStatusRouting{
                sequencedSource,
                defaultTransportOwner,
            };

            const GroovePuterMidi::TransportClockRuntimeSnapshot clock =
                GroovePuterMidi::transportClockRuntime().snapshot();
            status.clock =
                clock.source == GroovePuterMidi::TransportClockSource::SeqtrakExternal
                    ? UiStatusClock::External
                    : UiStatusClock::Internal;

            GroovePuterMidi::ISmfPlayerService* player =
                GroovePuterMidi::smfPlayerService();
            if (player != nullptr) {
                const GroovePuterMidi::SmfPlayerSnapshot smf = player->snapshot();
                const bool playerPageSelected =
                    status.context == UiStatusContext::Player;
                const bool loadedPlayerSelected =
                    playerPageSelected &&
                    smf.state != GroovePuterMidi::SmfPlayerState::Unloaded;
                if (smfStateOwnsStatus(smf.state) || loadedPlayerSelected) {
                    status.routing = UiStatusRouting{
                        sequencedSource,
                        UiTransportOwner::Smf,
                    };
                    status.state = uiStateForSmf(smf.state);
                    status.bar = statusCount(smf.bar);
                    status.totalBars = statusCount(smf.totalBars);
                    status.output = UiStatusOutput::Midi;
                    populatePatternAddress(status, miniAcid);
                    if (smf.tempoMode == GroovePuterMidi::SmfTempoMode::Original) {
                        status.clock = UiStatusClock::File;
                    }
                    return status;
                }
            }

            status.state = miniAcid.isPlaying()
                ? UiStatusState::Play
                : UiStatusState::Stop;
            status.output = UiStatusOutput::InternalAudio;
            populatePatternAddress(status, miniAcid);

            if (defaultTransportOwner == UiTransportOwner::Song) {
                status.bar = statusOneBasedIndex(miniAcid.songPlayheadPosition());
                status.totalBars = statusCount(
                    static_cast<uint32_t>(miniAcid.songLength() > 0
                        ? miniAcid.songLength()
                        : 1));
            } else {
                status.bar = statusOneBasedIndex(miniAcid.cycleBarIndex());
                status.totalBars = statusCount(
                    static_cast<uint32_t>(miniAcid.cycleBarCount() > 0
                        ? miniAcid.cycleBarCount()
                        : 1));
            }
            return status;
        }
    }

    UiStatusSnapshot captureUiStatusSnapshot(MiniAcid& mini_acid,
                                             UiStatusContext context) {
        gActiveEngine = &mini_acid;
        const auto mode = static_cast<GenerativeMode>(
            mini_acid.sceneManager().currentScene().genre.generativeMode);
        currentGenreAccent = genreAccentColor(mode);
        return buildUiStatusSnapshot(mini_acid, context);
    }

    void drawStandardHeader(IGfx& gfx, MiniAcid& mini_acid, const char* title) {
        // U1F: pages no longer own global header pixels or re-read live
        // status truth while composing their body.
        (void)gfx;
        (void)mini_acid;
        (void)title;
    }

    void drawStatusChrome(IGfx& gfx, const UiStatusSnapshot& status) {
        if (!gStatusInitialized || status != gStatusSnapshot) {
            gStatusSnapshot = status;
            formatUiStatusLine(status, gStatusLine, sizeof(gStatusLine));
            gStatusInitialized = true;
        }

        IGfxColor background = COLOR_BLACK;
        IGfxColor foreground = COLOR_WHITE;
        IGfxColor divider = COLOR_DARKER;
        if (currentStyle == VisualStyle::RETRO_CLASSIC) {
            background = IGfxColor(RetroTheme::BG_DEEP_BLACK);
            foreground = IGfxColor(RetroTheme::NEON_CYAN);
            divider = IGfxColor(RetroTheme::STATUS_ACCENT);
        } else if (currentStyle == VisualStyle::AMBER) {
            background = IGfxColor(AmberTheme::BG_DEEP_BLACK);
            foreground = IGfxColor(AmberTheme::NEON_ORANGE);
            divider = IGfxColor(AmberTheme::TEXT_DIM);
        }

        // The current renderer redraws every page each UI frame. Keep status
        // derivation outside the draw path, then paint only the already-
        // reserved 16-pixel header from the captured frame snapshot.
        gfx.fillRect(Layout::HEADER.x,
                     Layout::HEADER.y,
                     Layout::HEADER.w,
                     Layout::HEADER.h,
                     background);
        gfx.drawLine(Layout::HEADER.x,
                     Layout::HEADER.y + Layout::HEADER.h - 1,
                     Layout::HEADER.x + Layout::HEADER.w - 1,
                     Layout::HEADER.y + Layout::HEADER.h - 1,
                     divider);
        gfx.setTextColor(foreground);
        Widgets::drawClippedText(gfx,
                                 Layout::HEADER.x + 4,
                                 Layout::HEADER.y + 4,
                                 Layout::HEADER.w - 8,
                                 gStatusLine);
    }

    void drawLiveMixLockBadge(IGfx& gfx, const UiStatusSnapshot& status) {
        // Compatibility hook: MiniAcidDisplay already invokes this once after
        // every page. U1C makes that call render-only; U2 removes the competing
        // page/global header ownership itself.
        drawStatusChrome(gfx, status);
    }

    void beginShellFrameModel(UiShellFrameModel& model) {
        model.clear();
        gShellFrameModel = &model;
        gInfoValid = false;
        gInfoLeft[0] = '\0';
        gInfoRight[0] = '\0';
    }

    void endShellFrameModel() {
        gShellFrameModel = nullptr;
    }

    void publishShellFooter(const char* left, const char* right) {
        if (gShellFrameModel == nullptr) return;
        gShellFrameModel->setFooter(left, right);
    }

    void publishShellInfo(const char* left, const char* right) {
        if (left) {
            std::strncpy(gInfoLeft, left, sizeof(gInfoLeft) - 1);
            gInfoLeft[sizeof(gInfoLeft) - 1] = '\0';
        } else {
            gInfoLeft[0] = '\0';
        }
        if (right) {
            std::strncpy(gInfoRight, right, sizeof(gInfoRight) - 1);
            gInfoRight[sizeof(gInfoRight) - 1] = '\0';
        } else {
            gInfoRight[0] = '\0';
        }
        gInfoValid = (left != nullptr || right != nullptr);
    }

    void publishShellFeelOverlay(bool visible) {
        if (gShellFrameModel == nullptr) return;
        gShellFrameModel->feelOverlay = visible;
    }

    void drawShellFooter(IGfx& gfx, const UiFooterModel& footer) {
        if (!hintOverlayActive) {
            const ThemePalette p = themePalette();
            gfx.fillRect(Layout::FOOTER.x, Layout::FOOTER.y, Layout::FOOTER.w, Layout::FOOTER.h, p.background);
            gfx.drawLine(Layout::FOOTER.x, Layout::FOOTER.y,
                         Layout::FOOTER.x + Layout::FOOTER.w - 1, Layout::FOOTER.y,
                         p.panel);

            const int y = Layout::FOOTER.y + 3;

            if (gActiveEngine != nullptr) {
                // 1. Audio Peak VU Meter (x: 2..54)
                const auto& waveBuffer = gActiveEngine->getWaveformBuffer();
                int32_t peak = 0;
                for (size_t i = 0; i < waveBuffer.count; ++i) {
                    int32_t s = std::abs(static_cast<int32_t>(waveBuffer.data[i]));
                    if (s > peak) peak = s;
                }
                if (peak > sVuDecay) {
                    sVuDecay = peak;
                } else {
                    sVuDecay = (sVuDecay * 7) / 8;
                }
                int numSegments = 0;
                if (sVuDecay > 150) numSegments = 1;
                if (sVuDecay > 800) numSegments = 2;
                if (sVuDecay > 2500) numSegments = 3;
                if (sVuDecay > 6000) numSegments = 4;
                if (sVuDecay > 13000) numSegments = 5;
                if (sVuDecay > 22000) numSegments = 6;

                gfx.setTextColor(p.dim);
                gfx.drawText(2, y + 1, "VU");
                for (int i = 0; i < 6; ++i) {
                    int sx = 16 + i * 6;
                    IGfxColor segCol = (i < 3) ? IGfxColor(0x00E676)
                                     : ((i < 5) ? IGfxColor(0xFFD600) : IGfxColor(0xFF1744));
                    if (i < numSegments) {
                        gfx.fillRect(sx, y, 5, 7, segCol);
                    } else {
                        gfx.drawRect(sx, y, 5, 7, IGfxColor(0x18222B));
                    }
                }

                gfx.drawLine(56, y - 1, 56, y + 7, p.panel);

                // 2. Genre Badge (Right edge: x: badgeX..238)
                const auto& genre = gActiveEngine->sceneManager().currentScene().genre;
                const auto mode = static_cast<GenerativeMode>(genre.generativeMode);
                const auto& prof = genreProfile(mode);

                int badgeW = gfx.textWidth(prof.tag) + 8;
                int badgeX = Layout::FOOTER.w - badgeW - 2;

                if (gStatusSnapshot.dirty) {
                    gfx.setTextColor(p.warning);
                    gfx.drawText(badgeX - 8, y + 1, "*");
                }
                gfx.fillRect(badgeX, y, badgeW, 8, prof.primary);
                gfx.setTextColor(COLOR_BLACK);
                gfx.drawText(badgeX + 4, y + 1, prof.tag);

                gfx.drawLine(badgeX - 12, y - 1, badgeX - 12, y + 7, p.panel);

                // 3. Center zone (x: 60 .. badgeX - 14):
                // If page published contextual parameter info, show it!
                if (gInfoValid && gInfoLeft[0] != '\0') {
                    gfx.setTextColor(p.accent);
                    int maxW = (badgeX - 16) - 60;
                    Widgets::drawClippedText(gfx, 60, y + 1, maxW, gInfoLeft);
                } else {
                    // Live Transport & Position
                    const bool playing = gActiveEngine->isPlaying();
                    if (playing) {
                        gfx.setTextColor(IGfxColor(0x00E676));
                        gfx.drawText(60, y + 1, "PLAY");
                    } else {
                        gfx.setTextColor(p.dim);
                        gfx.drawText(60, y + 1, "STOP");
                    }

                    char posBuf[24];
                    if (gActiveEngine->songModeEnabled()) {
                        int pos = gActiveEngine->currentSongPosition() + 1;
                        int len = gActiveEngine->songLength();
                        std::snprintf(posBuf, sizeof(posBuf), "SONG %02d/%02d", pos, len > 0 ? len : 16);
                        gfx.setTextColor(p.warning);
                    } else {
                        int step = gActiveEngine->currentStep();
                        int stepInBar = (step >= 0 ? (step % 16) + 1 : 1);
                        std::snprintf(posBuf, sizeof(posBuf), "STEP %02d/16", stepInBar);
                        gfx.setTextColor(playing ? p.text : p.dim);
                    }
                    gfx.drawText(94, y + 1, posBuf);
                }
                return;
            }

            if (gInfoValid) {
                if (gInfoLeft[0] != '\0') {
                    gfx.setTextColor(p.text);
                    int maxW = Layout::FOOTER.w - 8;
                    if (gInfoRight[0] != '\0') {
                        maxW -= (gfx.textWidth(gInfoRight) + 8);
                    }
                    Widgets::drawClippedText(gfx, Layout::FOOTER.x + 4, y, maxW, gInfoLeft);
                }
                if (gInfoRight[0] != '\0') {
                    gfx.setTextColor(p.secondary);
                    int tw = gfx.textWidth(gInfoRight);
                    gfx.drawText(Layout::FOOTER.x + Layout::FOOTER.w - 4 - tw, y, gInfoRight);
                }
            } else {
                if (gStatusSnapshot.dirty) {
                    gfx.setTextColor(p.accent);
                    gfx.drawText(Layout::FOOTER.x + 4, y, "* MODIFIED");
                }
                gfx.setTextColor(p.dim);
                const char* hPrompt = "[H] HELP";
                int tw = gfx.textWidth(hPrompt);
                gfx.drawText(Layout::FOOTER.x + Layout::FOOTER.w - 4 - tw, y, hPrompt);
            }
            return;
        }
        LayoutManager::drawFooter(gfx,
                                  footer.valid ? footer.left : "",
                                  footer.valid ? footer.right : "");
    }

    void drawStandardFooter(IGfx& gfx, const char* left, const char* right) {
        (void)gfx;
        publishShellFooter(left, right);
    }

    void drawVerticalList(IGfx& gfx, int x, int y, int width,
                          const char* const* items, int itemCount,
                          int selectedIndex, bool hasFocus,
                          int iconIndex) {
        for (int i = 0; i < itemCount; i++) {
            int rowY = y + i * Layout::LINE_HEIGHT;
            bool selected = (i == selectedIndex) && hasFocus;
            bool hasIcon = (i == iconIndex);
            Widgets::drawListRow(gfx, x, rowY, width, items[i], selected, hasIcon);
        }
    }

    void drawChannelActivityBar(IGfx& gfx, int x, int y, int width, int height,
                                const bool* activeFlags, int channelCount) {
        if (channelCount <= 0) return;

        const int gap = 2;
        const int segmentW = (width - gap * (channelCount - 1)) / channelCount;

        for (int i = 0; i < channelCount; ++i) {
            int sx = x + i * (segmentW + gap);

            if (activeFlags[i]) {
                gfx.fillRect(sx, y, segmentW, height, COLOR_KNOB_1);
            } else {
                gfx.drawRect(sx, y, segmentW, height, COLOR_DARKER);
            }
        }
    }

    void drawButtonGridHelper(IGfx& gfx, int x, int y,
                              const char* const* labels, int labelsCount,
                              int selectedIndex, bool hasFocus) {
        Widgets::drawButtonGrid(
            gfx,
            x, y,
            58, 10,
            2, 4,
            labels,
            labelsCount,
            hasFocus ? selectedIndex : -1
        );
    }

    void drawWaveformOverlay(IGfx& gfx, MiniAcid& mini_acid) {
        if (!waveformOverlay.enabled) return;

        // The waveform shares the reserved performance strip without entering
        // either the FEEL label on the left or mute/activity digits on the right.
        const int h = Layout::PERFORMANCE_WAVEFORM.h;
        const int y = Layout::PERFORMANCE_WAVEFORM.y;
        const int x = Layout::PERFORMANCE_WAVEFORM.x;
        const int w = Layout::PERFORMANCE_WAVEFORM.w;

        if (w < 10 || h < 4) return;

        // Get waveform buffer (thread-safe)
        const auto& waveBuffer = mini_acid.getWaveformBuffer();
        
        const int midY = y + h / 2;
        const int amplitudeUp = midY - y;
        const int amplitudeDown = y + h - 1 - midY;
        int points = w < kOverlayMaxPoints ? w : kOverlayMaxPoints;

        // 1) Reference center line (matches page)
        gfx.drawLine(x, midY, x + w - 1, midY, COLOR_WAVE);

        // 2) Snapshot one real audio trace. Do not add synthetic phase motion:
        // it dirties the entire wide HUD every UI frame and can starve the
        // physical display/audio schedule. At volume zero a flat trace is the
        // correct representation of the post-volume output buffer.
        if (waveBuffer.count > 1 && points > 1) {
            overlayLength = points;
            for (int px = 0; px < points; ++px) {
                const size_t idx = static_cast<size_t>(
                    (static_cast<uint64_t>(px) * (waveBuffer.count - 1)) /
                    (points - 1));
                overlayWave[px] = waveBuffer.data[idx];
            }
        }

        // 3) Draw helper
        auto drawWave = [&](const int16_t* wave, int len, IGfxColor color) {
            if (len < 2) return;
            int drawLen = len < w ? len : w;
            int32_t peak = 0;
            for (int px = 0; px < drawLen; ++px) {
                int32_t sample = wave[px];
                if (sample < 0) sample = -sample;
                if (sample > peak) peak = sample;
            }
            // A small noise gate keeps silence flat. Above it, bounded visual
            // auto-gain lets quiet but intentional material use the compact
            // four-pixel half-height without changing the audio signal.
            if (peak < 128) return;
            const int32_t visualPeak = peak < 2048 ? 2048 : peak;
            for (int px = 0; px < drawLen - 1; ++px) {
                const int32_t sample0 = wave[px];
                const int32_t sample1 = wave[px + 1];
                const int scale0 = sample0 >= 0 ? amplitudeUp : amplitudeDown;
                const int scale1 = sample1 >= 0 ? amplitudeUp : amplitudeDown;
                const int y0 = midY - static_cast<int>((sample0 * scale0) / visualPeak);
                const int y1 = midY - static_cast<int>((sample1 * scale1) / visualPeak);
                drawLineColored(gfx, x + px, y0, x + px + 1, y1, color);
            }
        };

        // 4) Draw one crisp current trace. The HUD owner clears the previous
        // frame, so a ghost layer only masks motion on the physical display.
        IGfxColor waveColor = WAVE_COLORS[waveformOverlay.colorIndex % NUM_WAVE_COLORS];
        drawWave(overlayWave, overlayLength, waveColor);
    }

    void drawMutesOverlay(IGfx& gfx, MiniAcid& mini_acid) {
        // Mutes Overlay v3: Compact, Numbered, Themed
        
        // 1. Setup Theme Colors
        IGfxColor kActive, kMuted, kIdle;
        
        if (currentStyle == VisualStyle::MINIMAL) {
            kActive = COLOR_WHITE;
            kMuted = COLOR_RED;
            kIdle = IGfxColor(0x404040); // Dark Gray
        } else if (currentStyle == VisualStyle::RETRO_CLASSIC) {
            kActive = IGfxColor(RetroTheme::NEON_CYAN);
            kMuted = IGfxColor(RetroTheme::STATUS_ACCENT); // Red/Pink
            kIdle = IGfxColor(RetroTheme::TEXT_DIM);
        } else { // AMBER
            kActive = IGfxColor(AmberTheme::NEON_CYAN);
            kMuted = IGfxColor(AmberTheme::NEON_ORANGE);
            kIdle = IGfxColor(AmberTheme::TEXT_DIM);
        }

        // 2. Position (Bottom Right)
        const int itemW = 8;     // Reduced width for numbers
        const int spacing = 2;
        const int totalW = (10 * itemW) + (9 * spacing);
        const int x = gfx.width() - totalW - 4; // Right aligned
        const int y = Layout::PERFORMANCE_HUD.y;
        const bool playing = mini_acid.isPlaying();
        int step = mini_acid.currentStep();
        if (step < 0 || step >= 16) step = 0;
        const bool sp12Swap90 = (mini_acid.currentDrumEngineName() == "SP12");

        // Step-hit lookup for flash behavior.
        const int8_t* s303a = mini_acid.pattern303Steps(0);
        const int8_t* s303b = mini_acid.pattern303Steps(1);
        const bool* dKick = mini_acid.patternKickSteps();
        const bool* dSnare = mini_acid.patternSnareSteps();
        const bool* dHat = mini_acid.patternHatSteps();
        const bool* dOpenHat = mini_acid.patternOpenHatSteps();
        const bool* dMidTom = mini_acid.patternMidTomSteps();
        const bool* dHighTom = mini_acid.patternHighTomSteps();
        const bool* dRim = mini_acid.patternRimSteps();
        const bool* dClap = mini_acid.patternClapSteps();

        auto keyHitNow = [&](int keyIndex) -> bool {
            // keyIndex: 0..9 for keys 1..0
            switch (keyIndex) {
                case 0: return s303a && (s303a[step] >= 0);
                case 1: return s303b && (s303b[step] >= 0);
                case 2: return dKick && dKick[step];
                case 3: return dSnare && dSnare[step];
                case 4: return dHat && dHat[step];
                case 5: return dOpenHat && dOpenHat[step];
                case 6: return dMidTom && dMidTom[step];
                case 7: return dHighTom && dHighTom[step];
                case 8: return (sp12Swap90 ? (dClap && dClap[step]) : (dRim && dRim[step]));
                case 9: return (sp12Swap90 ? (dRim && dRim[step]) : (dClap && dClap[step]));
                default: return false;
            }
        };
        
        // 3. Draw Loop
        for (int i = 0; i < 10; ++i) {
            int cx = x + i * (itemW + spacing);
            
            // Determine Mute Status
            bool muted = false;
            switch(i) {
                case 0: muted = mini_acid.is303Muted(0); break;
                case 1: muted = mini_acid.is303Muted(1); break;
                case 2: muted = mini_acid.isKickMuted(); break;
                case 3: muted = mini_acid.isSnareMuted(); break;
                case 4: muted = mini_acid.isHatMuted(); break;
                case 5: muted = mini_acid.isOpenHatMuted(); break;
                case 6: muted = mini_acid.isMidTomMuted(); break;
                case 7: muted = mini_acid.isHighTomMuted(); break;
                case 8: muted = sp12Swap90 ? mini_acid.isClapMuted() : mini_acid.isRimMuted(); break;
                case 9: muted = sp12Swap90 ? mini_acid.isRimMuted() : mini_acid.isClapMuted(); break;
            }
            
            // Determine Color & Style
            bool active = !muted;
            bool hitNow = playing && keyHitNow(i);
            bool blink = (millis() % 170) < 95;
            IGfxColor color = kIdle;
            
            if (muted) {
                color = kMuted;
            } else if (active) {
                color = kActive;
            }

            // Compact flash block on musical trigger, keeps palette consistent per theme.
            if (hitNow && blink) {
                IGfxColor flashBg = muted ? kMuted : kActive;
                gfx.fillRect(cx - 1, y, itemW, 8, flashBg);
                color = COLOR_BLACK;
            }
            
            // Draw Number
            char num[2]; 
            snprintf(num, sizeof(num), "%d", (i + 1) % 10);
            
            gfx.setTextColor(color);
            gfx.drawText(cx, y, num);
            
            // Small timing tick stays inside the owned HUD strip.
            if (active && !muted && blink) {
                gfx.fillRect(cx + 3, y + 8, 2, 2, hitNow ? kActive : kIdle);
            }
            
        }
    }

    void drawFeelOverlay(IGfx& gfx, MiniAcid& mini_acid, bool pulse) {
        const auto& feel = mini_acid.sceneManager().currentScene().feel;
        int grid = feel.gridSteps;
        if (grid != 8 && grid != 16 && grid != 32) grid = 16;
        int bars = feel.patternBars;
        if (bars != 1 && bars != 2 && bars != 4 && bars != 8) bars = 1;
        int tb = feel.timebase;
        if (tb < 0) tb = 0;
        if (tb > 2) tb = 2;

        const char* gridStr = (grid == 8) ? "1/8" : (grid == 32) ? "1/32" : "1/16";
        const char* tbStr = (tb == 0) ? "H" : (tb == 2) ? "D" : "N";
        char buf[20];
        snprintf(buf, sizeof(buf), "G%s T%s L%dB", gridStr, tbStr, bars);

        const int x = Layout::CONTENT_PAD_X;
        const int y = Layout::PERFORMANCE_HUD.y;

        IGfxColor textColor = COLOR_LABEL;
        if (currentStyle == VisualStyle::RETRO_CLASSIC) {
            textColor = IGfxColor(RetroTheme::TEXT_SECONDARY);
        } else if (currentStyle == VisualStyle::AMBER) {
            textColor = IGfxColor(AmberTheme::TEXT_SECONDARY);
        }

        if (pulse) {
            int w = gfx.textWidth(buf);
            gfx.fillRect(x - 2, y - 1, w + 4, 10, COLOR_ACCENT);
            gfx.setTextColor(COLOR_BLACK);
        } else {
            gfx.setTextColor(textColor);
        }

        gfx.drawText(x, y, buf);
    }

    void drawPerformanceHud(IGfx& gfx, MiniAcid& mini_acid, bool feelPulse,
                            bool showFeelOverlay) {
        const ThemePalette palette = themePalette();
        gfx.fillRect(Layout::PERFORMANCE_HUD.x,
                     Layout::PERFORMANCE_HUD.y,
                     Layout::PERFORMANCE_HUD.w,
                     Layout::PERFORMANCE_HUD.h,
                     palette.background);
        drawWaveformOverlay(gfx, mini_acid);
        if (showFeelOverlay) drawFeelOverlay(gfx, mini_acid, feelPulse);
        // Mutes are intentionally last so their digits remain the topmost,
        // readable layer even while the waveform is moving.
        drawMutesOverlay(gfx, mini_acid);
    }

    void drawFeelHeaderHud(IGfx& gfx, MiniAcid& mini_acid, int x, int y) {
        (void)x;
        (void)y;
        const auto& feel = mini_acid.sceneManager().currentScene().feel;
        int grid = feel.gridSteps;
        if (grid != 8 && grid != 16 && grid != 32) grid = 16;
        int bars = feel.patternBars;
        if (bars != 1 && bars != 2 && bars != 4 && bars != 8) bars = 1;
        int tb = feel.timebase;
        if (tb < 0) tb = 0;
        if (tb > 2) tb = 2;

        char tbChar = (tb == 0) ? 'H' : (tb == 2) ? 'D' : 'N';
        char buf[20];
        snprintf(buf, sizeof(buf), "G%d T%c L%d", grid, tbChar, bars);

        // Draw as right-aligned chip inside header, clipped and isolated from title text.
        const int chipW = 72;
        const int chipH = 9;
        const int chipX = Layout::HEADER.x + Layout::HEADER.w - chipW - 14; // keep REC area free
        const int chipY = 3;

        gfx.fillRect(chipX, chipY, chipW, chipH, COLOR_BLACK);
        gfx.setTextColor(COLOR_LABEL);
        Widgets::drawClippedText(gfx, chipX, chipY, chipW, buf);
    }

    void showToast(const char* msg, int durationMs) {
        if (!msg) return;
        snprintf(gToastMsg, sizeof(gToastMsg), "%s", msg);
        gToastEndMs = millis() + durationMs;
    }

    void drawToast(IGfx& gfx) {
        if (millis() < gToastEndMs) {
            const ThemePalette p = themePalette();
            int w = gfx.width();
            int tw = gfx.textWidth(gToastMsg);
            int x = (w - tw) / 2;
            int y = gfx.height() - 25;
            gfx.fillRect(x - 4, y - 2, tw + 8, 11, p.background);
            gfx.drawRect(x - 4, y - 2, tw + 8, 11, p.accent);
            gfx.setTextColor(COLOR_WHITE);
            gfx.drawText(x, y, gToastMsg);
        }
    }

    void setHintOverlayActive(bool active) {
        hintOverlayActive = active;
        if (!active) {
            g_hintOverlayHeld = false;
            g_hintOverlayTimedActive = false;
            g_hintOverlayTimedUntilMs = 0;
        }
    }

    bool dismissHintOverlay() {
        g_hintOverlayTimedActive = false;
        g_hintOverlayTimedUntilMs = 0;
        if (hintOverlayActive) {
            hintOverlayActive = false;
            return true;
        }
        return false;
    }

    bool updateHintOverlay(bool hHeld, uint32_t nowMs) {
        if (hHeld && !g_hintOverlayHeld) {
            g_hPressStartMs = nowMs;
        } else if (!hHeld && g_hintOverlayHeld) {
            const uint32_t holdDuration = nowMs - g_hPressStartMs;
            if (holdDuration < 350) {
                // Short press (single click / tap): toggle or activate for 5 seconds
                if (g_hintOverlayTimedActive && static_cast<int32_t>(g_hintOverlayTimedUntilMs - nowMs) > 0) {
                    g_hintOverlayTimedActive = false;
                    g_hintOverlayTimedUntilMs = 0;
                } else {
                    g_hintOverlayTimedActive = true;
                    g_hintOverlayTimedUntilMs = nowMs + 5000;
                }
            } else {
                // Long press (hold): immediately dismiss on release
                g_hintOverlayTimedActive = false;
                g_hintOverlayTimedUntilMs = 0;
            }
        }
        g_hintOverlayHeld = hHeld;

        if (g_hintOverlayTimedActive && static_cast<int32_t>(nowMs - g_hintOverlayTimedUntilMs) >= 0) {
            g_hintOverlayTimedActive = false;
            g_hintOverlayTimedUntilMs = 0;
        }

        const bool shouldBeActive = hHeld || g_hintOverlayTimedActive;
        if (shouldBeActive != hintOverlayActive) {
            hintOverlayActive = shouldBeActive;
            return true;
        }
        return false;
    }

}
