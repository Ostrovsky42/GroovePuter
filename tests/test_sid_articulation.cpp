#include "../src/dsp/sid_synth.h"
#include "../src/dsp/sid_synth_voice.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <vector>

namespace {
constexpr float kSampleRate = 22050.0f;

std::vector<float> render(SidSynth& synth, size_t count) {
    std::vector<float> out(count, 0.0f);
    synth.process(out.data(), out.size());
    return out;
}

float rms(const std::vector<float>& data, size_t begin = 0) {
    if (begin >= data.size()) return 0.0f;
    double sum = 0.0;
    for (size_t i = begin; i < data.size(); ++i) {
        sum += static_cast<double>(data[i]) * static_cast<double>(data[i]);
    }
    return static_cast<float>(
        std::sqrt(sum / static_cast<double>(data.size() - begin)));
}

float mean(const std::vector<float>& data, size_t begin) {
    double sum = 0.0;
    for (size_t i = begin; i < data.size(); ++i) sum += data[i];
    return static_cast<float>(sum / static_cast<double>(data.size() - begin));
}

float peakAbs(const std::vector<float>& data) {
    float peak = 0.0f;
    for (float sample : data) peak = std::max(peak, std::fabs(sample));
    return peak;
}

float estimateSquareFrequency(const std::vector<float>& data) {
    size_t crossings = 0;
    for (size_t i = 1; i < data.size(); ++i) {
        if ((data[i - 1] < 0.0f && data[i] >= 0.0f) ||
            (data[i - 1] >= 0.0f && data[i] < 0.0f)) {
            ++crossings;
        }
    }
    const float seconds = static_cast<float>(data.size()) / kSampleRate;
    return static_cast<float>(crossings) / (2.0f * seconds);
}

void configureRaw(SidSynth& synth) {
    synth.setFilterType(3);
    synth.setPulseWidth(2048);
}

void testVelocityZeroIsSilent() {
    SidSynth synth;
    synth.init(kSampleRate);
    configureRaw(synth);
    synth.startNoteFrequency(220.0f, 0, false, false);
    const auto out = render(synth, 512);
    assert(peakAbs(out) == 0.0f);
    assert(!synth.isActive());
}

void testAttackAndReleaseAreBounded() {
    SidSynth synth;
    synth.init(kSampleRate);
    configureRaw(synth);
    synth.startNoteFrequency(220.0f, 100, false, false);

    const auto attack = render(synth, 1);
    const auto steady = render(synth, 2048);
    assert(std::fabs(attack[0]) < peakAbs(steady) * 0.20f);

    synth.stopNote();
    const auto tail = render(synth, 128);
    assert(peakAbs(tail) > 0.0001f);
    render(synth, static_cast<size_t>(kSampleRate * 0.10f));
    assert(!synth.isActive());
    const auto after = render(synth, 32);
    assert(peakAbs(after) == 0.0f);
}

void testResetSilencesImmediately() {
    SidSynth synth;
    synth.init(kSampleRate);
    configureRaw(synth);
    synth.startNoteFrequency(330.0f, 100, false, false);
    render(synth, 256);
    synth.reset();
    assert(!synth.isActive());
    assert(peakAbs(render(synth, 64)) == 0.0f);
}

void testAccentIsStrongerAndBounded() {
    SidSynth normal;
    SidSynth accent;
    normal.init(kSampleRate);
    accent.init(kSampleRate);
    configureRaw(normal);
    configureRaw(accent);
    normal.startNoteFrequency(220.0f, 80, false, false);
    accent.startNoteFrequency(220.0f, 80, true, false);
    const auto normalOut = render(normal, 2048);
    const auto accentOut = render(accent, 2048);
    const float normalRms = rms(normalOut, 256);
    const float accentRms = rms(accentOut, 256);
    assert(accentRms > normalRms * 1.08f);
    assert(peakAbs(accentOut) < 0.35f);
}

void testActiveSlidePreservesPhase() {
    SidSynth control;
    SidSynth slide;
    control.init(kSampleRate);
    slide.init(kSampleRate);
    configureRaw(control);
    configureRaw(slide);
    control.startNoteFrequency(220.0f, 100, false, false);
    slide.startNoteFrequency(220.0f, 100, false, false);

    const auto prefixA = render(control, 777);
    const auto prefixB = render(slide, 777);
    assert(prefixA == prefixB);

    slide.startNoteFrequency(220.0f, 100, false, true);
    const auto nextControl = render(control, 32);
    const auto nextSlide = render(slide, 32);
    assert(nextControl == nextSlide);
}

void testSlideConvergesAndFirstSlideStartsNormally() {
    SidSynth firstNormal;
    SidSynth firstSlide;
    firstNormal.init(kSampleRate);
    firstSlide.init(kSampleRate);
    configureRaw(firstNormal);
    configureRaw(firstSlide);
    firstNormal.startNoteFrequency(220.0f, 100, false, false);
    firstSlide.startNoteFrequency(220.0f, 100, false, true);
    assert(render(firstNormal, 256) == render(firstSlide, 256));

    SidSynth synth;
    synth.init(kSampleRate);
    configureRaw(synth);
    synth.startNoteFrequency(220.0f, 100, false, false);
    render(synth, 2048);
    synth.startNoteFrequency(440.0f, 100, false, true);
    render(synth, static_cast<size_t>(kSampleRate * 0.07f));
    const auto settled = render(synth, static_cast<size_t>(kSampleRate * 0.08f));
    const float frequency = estimateSquareFrequency(settled);
    assert(frequency > 420.0f && frequency < 460.0f);
}

void testDcBlockerAtPulseExtremes() {
    for (uint16_t pulseWidth : {uint16_t{64}, uint16_t{4095}}) {
        SidSynth synth;
        synth.init(kSampleRate);
        synth.setFilterType(3);
        synth.setPulseWidth(pulseWidth);
        synth.startNoteFrequency(110.0f, 100, false, false);
        const auto out = render(synth, static_cast<size_t>(kSampleRate * 1.0f));
        const float dc = mean(out, out.size() / 2);
        assert(std::fabs(dc) < 0.005f);
    }
}

void testTruthfulLabelsKeepIndices() {
    SidSynthVoice voice(kSampleRate);
    assert(std::strcmp(voice.getEngineName(), "SID") == 0);
    assert(voice.parameterCount() == 4);
    assert(std::strcmp(voice.getParameter(1).label(), "Damp") == 0);

    voice.setParameterNormalized(3, 1.0f / 3.0f);
    assert(voice.getParameter(3).optionIndex() == 1);
    assert(std::strcmp(voice.getParameter(3).optionLabel(), "EDGE") == 0);

    voice.setParameterNormalized(3, 1.0f);
    assert(voice.getParameter(3).optionIndex() == 3);
    assert(std::strcmp(voice.getParameter(3).optionLabel(), "RAW") == 0);
}
}  // namespace

int main() {
    testVelocityZeroIsSilent();
    testAttackAndReleaseAreBounded();
    testResetSilencesImmediately();
    testAccentIsStrongerAndBounded();
    testActiveSlidePreservesPhase();
    testSlideConvergesAndFirstSlideStartsNormally();
    testDcBlockerAtPulseExtremes();
    testTruthfulLabelsKeepIndices();
    return 0;
}
