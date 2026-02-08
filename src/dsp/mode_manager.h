#pragma once
#include "mode_config.h"
#include "mini_tb303.h"
#include "mini_drumvoices.h"
#include "src/dsp/genre_manager.h"
#include "../../scenes.h"
#include <stdlib.h>

class MiniAcid;

class GrooveboxModeManager {
public:
    GrooveboxModeManager(MiniAcid& engine) : engine_(engine), currentMode_(GrooveboxMode::Minimal), currentFlavor_(0) {}
    
    GrooveboxMode mode() const { return currentMode_; }
    int flavor() const { return currentFlavor_; }
    
    void setMode(GrooveboxMode mode);
    void toggle();
    void setFlavor(int flavor);
    void shiftFlavor(int delta);
    int flavorCount() const { return 5; }
    void setModeLocal(GrooveboxMode mode) { currentMode_ = mode; }
    void setFlavorLocal(int flavor) {
        if (flavor < 0) flavor = 0;
        if (flavor >= flavorCount()) flavor = flavorCount() - 1;
        currentFlavor_ = flavor;
    }
    
    const ModeConfig& config() const {
        switch (currentMode_) {
            case GrooveboxMode::Acid: return kAcidConfig;
            case GrooveboxMode::Minimal: return kMinimalConfig;
            case GrooveboxMode::Breaks: return kBreaksConfig;
            case GrooveboxMode::Dub: return kDubConfig;
            case GrooveboxMode::Electro: return kElectroConfig;
            default: return kMinimalConfig;
        }
    }
    
    const TB303ModePreset* get303Presets(int& count) const {
        count = 5;
        switch (currentMode_) {
            case GrooveboxMode::Acid: return kAcidPresets;
            case GrooveboxMode::Minimal: return kMinimalPresets;
            case GrooveboxMode::Breaks: return kBreaksPresets;
            case GrooveboxMode::Dub: return kDubPresets;
            case GrooveboxMode::Electro: return kElectroPresets;
            default: return kMinimalPresets;
        }
    }
    
    const TapeModePreset* getTapePresets(int& count) const {
        count = 5;
        switch (currentMode_) {
            case GrooveboxMode::Acid: return kAcidTapePresets;
            case GrooveboxMode::Minimal: return kMinimalTapePresets;
            case GrooveboxMode::Breaks: return kBreaksTapePresets;
            case GrooveboxMode::Dub: return kDubTapePresets;
            case GrooveboxMode::Electro: return kElectroTapePresets;
            default: return kMinimalTapePresets;
        }
    }
    
    // Apply a mode-specific preset to a voice
    void apply303Preset(int voiceIndex, int presetIndex);
    
    // Pattern generation (Legacy Mode-based)
    void generatePattern(SynthPattern& pattern, float bpm) const;
    void generateDrumPattern(DrumPatternSet& patternSet) const;

    // Pattern generation (Genre-based)
    void generatePattern(SynthPattern& pattern, float bpm, const GenerativeParams& params) const;
    void generateDrumPattern(DrumPatternSet& patternSet, const GenerativeParams& params) const;
    
    // Pattern generation (Genre + Behavior + Voice Role)
    // voiceIndex: 0 = bass (low, repetitive), 1 = lead/arp (high, melodic)
    void generatePattern(SynthPattern& pattern, float bpm, const GenerativeParams& params, const GenreBehavior& behavior, int voiceIndex = 0) const;
    void generateDrumPattern(DrumPatternSet& patternSet, const GenerativeParams& params, const GenreBehavior& behavior) const;
    void generateDrumVoice(DrumPattern& pattern, int voiceIndex, const GenerativeParams& params, const GenreBehavior& behavior) const;
    
private:
    MiniAcid& engine_;
    GrooveboxMode currentMode_;
    int currentFlavor_;
};
