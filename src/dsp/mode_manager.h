#pragma once
#include "mode_config.h"
#include "mini_tb303.h"
#include "mini_drumvoices.h"
#include "../../scenes.h"
#include <stdlib.h>

class MiniAcid;

class GrooveboxModeManager {
public:
    GrooveboxModeManager(MiniAcid& engine) : engine_(engine), currentMode_(GrooveboxMode::Acid) {}
    
    GrooveboxMode mode() const { return currentMode_; }
    
    void setMode(GrooveboxMode mode);
    void toggle();
    
    const ModeConfig& config() const {
        return (currentMode_ == GrooveboxMode::Acid) ? kAcidConfig : kMinimalConfig;
    }
    
    const TB303ModePreset* get303Presets(int& count) const {
        if (currentMode_ == GrooveboxMode::Acid) {
            count = 4;
            return kAcidPresets;
        } else {
            count = 4;
            return kMinimalPresets;
        }
    }
    
    const TapeModePreset* getTapePresets(int& count) const {
        if (currentMode_ == GrooveboxMode::Acid) {
            count = 4;
            return kAcidTapePresets;
        } else {
            count = 4;
            return kMinimalTapePresets;
        }
    }
    
    // Apply a mode-specific preset to a voice
    void apply303Preset(int voiceIndex, int presetIndex);
    
    // Pattern generation according to current mode rules
    void generatePattern(SynthPattern& pattern) const;
    void generateDrumPattern(DrumPatternSet& patternSet) const;
    
private:
    MiniAcid& engine_;
    GrooveboxMode currentMode_;
};
