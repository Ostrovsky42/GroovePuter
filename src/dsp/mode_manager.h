#pragma once
#include "mode_config.h"
#include "mini_tb303.h"
#include "mini_drumvoices.h"
#include "genre_manager.h"
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
};
