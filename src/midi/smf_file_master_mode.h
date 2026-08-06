#pragma once

#include <algorithm>
#include <cstdint>

#include "smf_player_service.h"

namespace GroovePuterMidi {

enum class SmfFileMasterStage : uint8_t {
    Disabled = 0,
    AwaitOriginalSnapshot,
    AwaitProjectRestore,
    Ready,
};

struct SmfFileMasterModeState {
    bool enabled{false};
    SmfFileMasterStage stage{SmfFileMasterStage::Disabled};
    uint32_t loadedEndTick{0};
    char loadedFilename[48]{};

    void begin() {
        enabled = true;
        stage = SmfFileMasterStage::AwaitOriginalSnapshot;
    }

    void disable() {
        enabled = false;
        stage = SmfFileMasterStage::Disabled;
        loadedEndTick = 0;
        loadedFilename[0] = '\0';
    }
};

inline bool smfFileMasterUsesProjectScheduler(
        const SmfFileMasterModeState& state,
        SmfTempoMode serviceMode) {
    return state.enabled && serviceMode == SmfTempoMode::Project;
}

inline float smfFileMasterBpm(uint16_t originalBpmX10) {
    const float bpm = static_cast<float>(originalBpmX10) / 10.0f;
    return std::max(10.0f, std::min(250.0f, bpm));
}

inline const char* smfFileMasterStageName(SmfFileMasterStage stage) {
    switch (stage) {
        case SmfFileMasterStage::Disabled: return "OFF";
        case SmfFileMasterStage::AwaitOriginalSnapshot: return "READ FILE BPM";
        case SmfFileMasterStage::AwaitProjectRestore: return "RESTORE GP SYNC";
        case SmfFileMasterStage::Ready: return "READY";
    }
    return "?";
}

}  // namespace GroovePuterMidi
