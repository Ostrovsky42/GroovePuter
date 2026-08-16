#include "../platform_sdl/arduino_compat.h"
#include "../scenes.h"
#include "../src/dsp/song_pattern_materializer.h"

#include <cassert>
#include <cstdint>
#include <string>

SerialMock Serial;
SDMock SD;

int main() {
    SceneManager manager;
    manager.loadDefaultScene();

    Scene& before = manager.currentScene();
    before.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::LoFi);
    before.genre.recipe = kDustyJazzRecipeId;
    before.genre.regenerateOnApply = false;

    SynthStep& synth = before.synthABanks[0].patterns[2].steps[7];
    synth.note = 49;
    synth.slide = true;
    synth.accent = true;
    synth.ghost = true;
    synth.velocity = 73;
    synth.timing = -9;
    synth.fx = 2;
    synth.fxParam = 6;
    synth.probability = 81;
    SongPatternMaterializer::markSlotSongGenerated(
        before, SongTrack::SynthA, 2);

    DrumStep& drum = before.drumBanks[1].patterns[3].voices[4].steps[11];
    drum.hit = true;
    drum.accent = true;
    drum.velocity = 64;
    drum.timing = 12;
    drum.fx = 3;
    drum.fxParam = 4;
    drum.probability = 76;
    SongPatternMaterializer::markSlotSongGenerated(
        before, SongTrack::Drums, 11);

    const std::string json = manager.dumpCurrentScene();
    assert(!json.empty());
    assert(json.find("\"gen\":15") != std::string::npos);
    assert(json.find("\"rcp\":17") != std::string::npos);

    // Destroy both semantic generation identity and realized material. Load
    // must restore the serialized result itself; it must not regenerate it.
    before.genre.generativeMode = static_cast<uint8_t>(GenerativeMode::Acid);
    before.genre.recipe = kBaseRecipeId;
    before.synthABanks[0].patterns[2] = SynthPattern{};
    before.drumBanks[1].patterns[3] = DrumPatternSet{};

    assert(manager.loadScene(json));
    const Scene& after = manager.currentScene();

    assert(after.genre.generativeMode == static_cast<uint8_t>(GenerativeMode::LoFi));
    assert(after.genre.recipe == kDustyJazzRecipeId);
    assert(!after.genre.regenerateOnApply);

    const SynthStep& loadedSynth =
        after.synthABanks[0].patterns[2].steps[7];
    assert(loadedSynth.note == 49);
    assert(loadedSynth.slide);
    assert(loadedSynth.accent);
    assert(loadedSynth.ghost);
    assert(loadedSynth.velocity == 73);
    assert(loadedSynth.timing == -9);
    assert(loadedSynth.fx == 2);
    assert(loadedSynth.fxParam == 6);
    assert(loadedSynth.probability == 81);
    assert(SongPatternMaterializer::slotIsSongGenerated(
        after, SongTrack::SynthA, 2));

    const DrumStep& loadedDrum =
        after.drumBanks[1].patterns[3].voices[4].steps[11];
    assert(loadedDrum.hit);
    assert(loadedDrum.accent);
    assert(loadedDrum.velocity == 64);
    assert(loadedDrum.timing == 12);
    assert(loadedDrum.fx == 3);
    assert(loadedDrum.fxParam == 4);
    assert(loadedDrum.probability == 76);
    assert(SongPatternMaterializer::slotIsSongGenerated(
        after, SongTrack::Drums, 11));

    // A second serialization must preserve both realized material and Lo-Fi
    // semantic identity byte-for-byte at the relevant persistence fields.
    const std::string secondJson = manager.dumpCurrentScene();
    assert(secondJson.find("\"gen\":15") != std::string::npos);
    assert(secondJson.find("\"rcp\":17") != std::string::npos);
    return 0;
}
