#include <cassert>
#include <cstdint>

#include "scenes.h"
#include "src/dsp/tape_fx.h"
#include "src/dsp/tape_looper.h"

static void mirrorEffectiveModeToScene(Scene& scene, const TapeLooper& looper) {
    if (scene.tape.mode != looper.mode()) {
        scene.tape.mode = looper.mode();
    }
}

static void assertNonModeTapeStatePreserved(const TapeState& actual,
                                            const TapeState& expected) {
    assert(actual.preset == expected.preset);
    assert(actual.speed == expected.speed);
    assert(actual.fxEnabled == expected.fxEnabled);
    assert(actual.macro.wow == expected.macro.wow);
    assert(actual.macro.age == expected.macro.age);
    assert(actual.macro.sat == expected.macro.sat);
    assert(actual.macro.tone == expected.macro.tone);
    assert(actual.macro.crush == expected.macro.crush);
    assert(actual.looperVolume == expected.looperVolume);
    assert(actual.space == expected.space);
    assert(actual.movement == expected.movement);
    assert(actual.groove == expected.groove);
}

static void verifyUnavailableModeRequest(TapeMode requestedMode) {
    Scene scene{};
    scene.tape.mode = requestedMode;
    scene.tape.preset = TapePreset::VHS;
    scene.tape.speed = 2;
    scene.tape.fxEnabled = true;
    scene.tape.macro = TapeMacro{44, 33, 22, 77, 2};
    scene.tape.looperVolume = 0.42f;
    scene.tape.space = 17;
    scene.tape.movement = 23;
    scene.tape.groove = 31;
    const TapeState requestedState = scene.tape;

    TapeLooper looper;
    assert(!looper.init(0.5f));
    assert(!looper.storageReady());
    assert(looper.mode() == TapeMode::Stop);
    assert(!looper.hasLoop());

    looper.setMode(scene.tape.mode);
    assert(looper.mode() == TapeMode::Stop);

    looper.setStutter(true);
    looper.setDubAutoExit(true);
    assert(!looper.stutterActive());
    assert(!looper.dubAutoExit());

    float loopPart = 1.0f;
    looper.process(0.75f, &loopPart);
    assert(loopPart == 0.0f);
    assert(looper.mode() == TapeMode::Stop);

    mirrorEffectiveModeToScene(scene, looper);
    assert(scene.tape.mode == TapeMode::Stop);
    assertNonModeTapeStatePreserved(scene.tape, requestedState);
}

int main() {
#if !defined(ARDUINO_M5STACK_CARDPUTER)
#error "This contract test must compile with ARDUINO_M5STACK_CARDPUTER defined"
#endif

    // The ADV TapeFX compatibility object must not contain the historical
    // 1024 + 4096 float delay arrays. Empty C++ classes are normally one byte;
    // keep a small allowance for ABI changes while rejecting resident DSP state.
    static_assert(sizeof(TapeFX) <= 8,
                  "Cardputer ADV TapeFX unexpectedly regained resident DSP state");

    TapeFX fx;
    fx.setEnabled(true);
    assert(!fx.isEnabled());
    const float dry = 0.375f;
    assert(fx.process(dry) == dry);

    verifyUnavailableModeRequest(TapeMode::Stop);
    verifyUnavailableModeRequest(TapeMode::Rec);
    verifyUnavailableModeRequest(TapeMode::Dub);
    verifyUnavailableModeRequest(TapeMode::Play);

    return 0;
}
