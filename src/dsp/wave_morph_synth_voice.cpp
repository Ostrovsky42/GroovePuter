#include "wave_morph_synth_voice.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float kTwoPi = 6.28318530718f;
constexpr float kDcBlockPole = 0.995f;
constexpr int16_t kWaveTables[WaveMorphSynthVoice::kTableCount][WaveMorphSynthVoice::kTableSize] = {
  {
    0, 1608, 3212, 4808, 6393, 7962, 9512, 11039, 12539, 14010, 15446, 16846, 18204, 19519, 20787, 22005,
    23170, 24279, 25329, 26319, 27245, 28105, 28898, 29621, 30273, 30852, 31356, 31785, 32137, 32412, 32609, 32728,
    32767, 32728, 32609, 32412, 32137, 31785, 31356, 30852, 30273, 29621, 28898, 28105, 27245, 26319, 25329, 24279,
    23170, 22005, 20787, 19519, 18204, 16846, 15446, 14010, 12539, 11039, 9512, 7962, 6393, 4808, 3212, 1608,
    0, -1608, -3212, -4808, -6393, -7962, -9512, -11039, -12539, -14010, -15446, -16846, -18204, -19519, -20787, -22005,
    -23170, -24279, -25329, -26319, -27245, -28105, -28898, -29621, -30273, -30852, -31356, -31785, -32137, -32412, -32609, -32728,
    -32767, -32728, -32609, -32412, -32137, -31785, -31356, -30852, -30273, -29621, -28898, -28105, -27245, -26319, -25329, -24279,
    -23170, -22005, -20787, -19519, -18204, -16846, -15446, -14010, -12539, -11039, -9512, -7962, -6393, -4808, -3212, -1608,
  },
  {
    0, -1024, -2048, -3072, -4096, -5120, -6144, -7168, -8192, -9216, -10240, -11264, -12288, -13312, -14336, -15360,
    -16384, -17407, -18431, -19455, -20479, -21503, -22527, -23551, -24575, -25599, -26623, -27647, -28671, -29695, -30719, -31743,
    -32767, -31743, -30719, -29695, -28671, -27647, -26623, -25599, -24575, -23551, -22527, -21503, -20479, -19455, -18431, -17407,
    -16384, -15360, -14336, -13312, -12288, -11264, -10240, -9216, -8192, -7168, -6144, -5120, -4096, -3072, -2048, -1024,
    0, 1024, 2048, 3072, 4096, 5120, 6144, 7168, 8192, 9216, 10240, 11264, 12288, 13312, 14336, 15360,
    16384, 17407, 18431, 19455, 20479, 21503, 22527, 23551, 24575, 25599, 26623, 27647, 28671, 29695, 30719, 31743,
    32767, 31743, 30719, 29695, 28671, 27647, 26623, 25599, 24575, 23551, 22527, 21503, 20479, 19455, 18431, 17407,
    16384, 15360, 14336, 13312, 12288, 11264, 10240, 9216, 8192, 7168, 6144, 5120, 4096, 3072, 2048, 1024,
  },
  {
    -32767, -32255, -31743, -31231, -30719, -30207, -29695, -29183, -28671, -28159, -27647, -27135, -26623, -26111, -25599, -25087,
    -24575, -24063, -23551, -23039, -22527, -22015, -21503, -20991, -20479, -19967, -19455, -18943, -18431, -17919, -17407, -16895,
    -16384, -15872, -15360, -14848, -14336, -13824, -13312, -12800, -12288, -11776, -11264, -10752, -10240, -9728, -9216, -8704,
    -8192, -7680, -7168, -6656, -6144, -5632, -5120, -4608, -4096, -3584, -3072, -2560, -2048, -1536, -1024, -512,
    0, 512, 1024, 1536, 2048, 2560, 3072, 3584, 4096, 4608, 5120, 5632, 6144, 6656, 7168, 7680,
    8192, 8704, 9216, 9728, 10240, 10752, 11264, 11776, 12288, 12800, 13312, 13824, 14336, 14848, 15360, 15872,
    16384, 16895, 17407, 17919, 18431, 18943, 19455, 19967, 20479, 20991, 21503, 22015, 22527, 23039, 23551, 24063,
    24575, 25087, 25599, 26111, 26623, 27135, 27647, 28159, 28671, 29183, 29695, 30207, 30719, 31231, 31743, 32255,
  },
  {
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767, 32767,
    -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
    -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
    -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
    -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
    -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
    -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767, -32767,
  },
  {
    0, 3941, 7819, 11572, 15143, 18476, 21526, 24251, 26622, 28614, 30216, 31422, 32238, 32679, 32767, 32531,
    32007, 31235, 30257, 29120, 27868, 26547, 25197, 23857, 22560, 21336, 20205, 19185, 18283, 17503, 16842, 16291,
    15837, 15464, 15152, 14879, 14623, 14363, 14079, 13753, 13370, 12919, 12394, 11792, 11114, 10365, 9556, 8700,
    7811, 6907, 6005, 5125, 4284, 3498, 2781, 2144, 1594, 1135, 767, 486, 284, 149, 69, 25,
    0, -25, -69, -149, -284, -486, -767, -1135, -1594, -2144, -2781, -3498, -4284, -5125, -6005, -6907,
    -7811, -8700, -9556, -10365, -11114, -11792, -12394, -12919, -13370, -13753, -14079, -14363, -14623, -14879, -15152, -15464,
    -15837, -16291, -16842, -17503, -18283, -19185, -20205, -21336, -22560, -23857, -25197, -26547, -27868, -29120, -30257, -31235,
    -32007, -32531, -32767, -32679, -32238, -31422, -30216, -28614, -26622, -24251, -21526, -18476, -15143, -11572, -7819, -3941,
  },
  {
    0, 7764, 15060, 21460, 26612, 30271, 32319, 32767, 31752, 29519, 26397, 22762, 19000, 15477, 12502, 10303,
    9018, 8685, 9251, 10586, 12504, 14788, 17220, 19599, 21765, 23607, 25069, 26150, 26887, 27345, 27601, 27723,
    27758, 27723, 27601, 27345, 26887, 26150, 25069, 23607, 21765, 19599, 17220, 14788, 12504, 10586, 9251, 8685,
    9018, 10303, 12502, 15477, 19000, 22762, 26397, 29519, 31752, 32767, 32319, 30271, 26612, 21460, 15060, 7764,
    0, -7764, -15060, -21460, -26612, -30271, -32319, -32767, -31752, -29519, -26397, -22762, -19000, -15477, -12502, -10303,
    -9018, -8685, -9251, -10586, -12504, -14788, -17220, -19599, -21765, -23607, -25069, -26150, -26887, -27345, -27601, -27723,
    -27758, -27723, -27601, -27345, -26887, -26150, -25069, -23607, -21765, -19599, -17220, -14788, -12504, -10586, -9251, -8685,
    -9018, -10303, -12502, -15477, -19000, -22762, -26397, -29519, -31752, -32767, -32319, -30271, -26612, -21460, -15060, -7764,
  },
  {
    0, 12036, 21521, 26659, 26905, 23037, 16796, 10247, 5086, 2159, 1343, 1798, 2465, 2585, 2031, 1325,
    1335, 2821, 6012, 10414, 14946, 18352, 19728, 18931, 16706, 14441, 13640, 15298, 19443, 25006, 30126, 32767,
    31444, 25797, 16786, 6445, -2741, -8598, -9911, -6735, -273, 7612, 15044, 20706, 24135, 25647, 25977, 25799,
    25347, 24302, 21996, 17817, 11654, 4152, -3342, -9160, -11893, -10929, -6754, -860, 4721, 8098, 8181, 5066,
    0, -5066, -8181, -8098, -4721, 860, 6754, 10929, 11893, 9160, 3342, -4152, -11654, -17817, -21996, -24302,
    -25347, -25799, -25977, -25647, -24135, -20706, -15044, -7612, 273, 6735, 9911, 8598, 2741, -6445, -16786, -25797,
    -31444, -32767, -30126, -25006, -19443, -15298, -13640, -14441, -16706, -18931, -19728, -18352, -14946, -10414, -6012, -2821,
    -1335, -1325, -2031, -2585, -2465, -1798, -1343, -2159, -5086, -10247, -16796, -23037, -26905, -26659, -21521, -12036,
  },
  {
    32767, 28086, 28086, 23405, 18724, 18724, 14043, 9362, 9362, 4681, 0, 0, -4681, -9362, -9362, -14043,
    -18724, -18724, -23405, -23405, -28086, -32767, -32767, -28086, -23405, -23405, -18724, -14043, -14043, -9362, -4681, -4681,
    0, 4681, 4681, 9362, 14043, 14043, 18724, 23405, 23405, 28086, 32767, 32767, 28086, 23405, 23405, 18724,
    18724, 14043, 9362, 9362, 4681, 0, 0, -4681, -9362, -9362, -14043, -18724, -18724, -23405, -28086, -28086,
    -32767, -28086, -28086, -23405, -18724, -18724, -14043, -9362, -9362, -4681, 0, 0, 4681, 9362, 9362, 14043,
    18724, 18724, 23405, 23405, 28086, 32767, 32767, 28086, 23405, 23405, 18724, 14043, 14043, 9362, 4681, 4681,
    0, -4681, -4681, -9362, -14043, -14043, -18724, -23405, -23405, -28086, -32767, -32767, -28086, -23405, -23405, -18724,
    -18724, -14043, -9362, -9362, -4681, 0, 0, 4681, 9362, 9362, 14043, 18724, 18724, 23405, 28086, 28086,
  },
};
}  // namespace

WaveMorphSynthVoice::WaveMorphSynthVoice(float sampleRate) {
    static const char* const kWaveNames[] = {
        "Sine", "Tri", "Saw", "Pulse", "Organ", "Vowel", "Metal", "Digital"};
    params_[0] = Parameter("Wave", "", kWaveNames, 8, 2);
    params_[1] = Parameter("Morph", "", 0.0f, 1.0f, 0.15f, 1.0f / 64.0f);
    params_[2] = Parameter("Sub", "", 0.0f, 1.0f, 0.20f, 1.0f / 64.0f);
    params_[3] = Parameter("Cutoff", "Hz", 80.0f, 7000.0f, 2400.0f, 20.0f);
    params_[4] = Parameter("Reso", "", 0.0f, 0.90f, 0.18f, 0.01f);
    params_[5] = Parameter("Decay", "ms", 30.0f, 2400.0f, 520.0f, 10.0f);
    setSampleRate(sampleRate);
    reset();
}

float WaveMorphSynthVoice::clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float WaveMorphSynthVoice::fastSaturate(float value) {
    return value / (1.0f + std::fabs(value));
}

float WaveMorphSynthVoice::tableSample(std::size_t tableIndex, float phase) {
    if (tableIndex >= kTableCount) tableIndex = 0;
    const float position = phase * static_cast<float>(kTableSize);
    const std::size_t index = static_cast<std::size_t>(position) & (kTableSize - 1u);
    const std::size_t next = (index + 1u) & (kTableSize - 1u);
    const float fraction = position - static_cast<float>(static_cast<uint32_t>(position));
    constexpr float kScale = 1.0f / 32767.0f;
    const float a = static_cast<float>(kWaveTables[tableIndex][index]);
    const float b = static_cast<float>(kWaveTables[tableIndex][next]);
    return (a + (b - a) * fraction) * kScale;
}

void WaveMorphSynthVoice::reset() {
    phase_ = 0.0f;
    subPhase_ = 0.0f;
    currentFreqHz_ = 110.0f;
    targetFreqHz_ = 110.0f;
    ampEnv_ = 0.0f;
    velocityGain_ = 1.0f;
    filter1_ = 0.0f;
    filter2_ = 0.0f;
    dcInput_ = 0.0f;
    dcOutput_ = 0.0f;
    gate_ = false;
    attack_ = false;
    slide_ = false;
}

void WaveMorphSynthVoice::setSampleRate(float sampleRate) {
    if (sampleRate <= 0.0f) sampleRate = 44100.0f;
    sampleRate_ = sampleRate;
    invSampleRate_ = 1.0f / sampleRate_;
    updateEnvelopeCoefficients();
    updateFilterCoefficients();
}

void WaveMorphSynthVoice::updateEnvelopeCoefficients() {
    const float decaySamples = std::max(
        1.0f, params_[5].value() * 0.001f * sampleRate_);
    decayCoef_ = std::exp(-1.0f / decaySamples);
    const float releaseSamples = std::max(1.0f, 70.0f * 0.001f * sampleRate_);
    releaseCoef_ = std::exp(-1.0f / releaseSamples);
}

void WaveMorphSynthVoice::updateFilterCoefficients() {
    const float cutoff = std::clamp(
        params_[3].value(), 40.0f, sampleRate_ * 0.40f);
    const float g = kTwoPi * cutoff * invSampleRate_;
    filterAlpha_ = std::clamp(g / (1.0f + g), 0.001f, 0.82f);
    filterFeedback_ = params_[4].value() * 3.2f;
}

void WaveMorphSynthVoice::startNote(float freqHz,
                                    bool accent,
                                    bool slideFlag,
                                    uint8_t velocity) {
    if (freqHz <= 0.0f) return;
    targetFreqHz_ = freqHz;
    const bool alreadyActive = gate_ || ampEnv_ > 0.0001f;
    slide_ = slideFlag && alreadyActive;
    if (!slide_) {
        currentFreqHz_ = freqHz;
        ampEnv_ = 0.0f;
        attack_ = true;
    }
    gate_ = true;
    velocityGain_ = std::clamp(
        static_cast<float>(velocity) / 127.0f, 0.05f, 1.0f);
    if (accent) velocityGain_ = std::min(1.15f, velocityGain_ * 1.15f);
}

void WaveMorphSynthVoice::release() {
    gate_ = false;
    attack_ = false;
}

void WaveMorphSynthVoice::advancePhase(float& phase, float increment) {
    phase += increment;
    if (phase >= 1.0f) phase -= 1.0f;
}

float WaveMorphSynthVoice::process() {
    if (!gate_ && ampEnv_ <= 0.0001f) return 0.0f;

    if (slide_) {
        currentFreqHz_ += (targetFreqHz_ - currentFreqHz_) * 0.0018f;
    } else {
        currentFreqHz_ = targetFreqHz_;
    }

    if (attack_) {
        const float step = std::min(1.0f, 750.0f * invSampleRate_);
        ampEnv_ += (1.0f - ampEnv_) * step;
        if (ampEnv_ >= 0.995f) {
            ampEnv_ = 1.0f;
            attack_ = false;
        }
    } else if (gate_) {
        constexpr float kSustain = 0.60f;
        ampEnv_ = kSustain + (ampEnv_ - kSustain) * decayCoef_;
    } else {
        ampEnv_ *= releaseCoef_;
    }

    if (!gate_ && ampEnv_ < 0.0001f) {
        ampEnv_ = 0.0f;
        return 0.0f;
    }

    const float frequency = std::clamp(
        currentFreqHz_, 1.0f, sampleRate_ * 0.45f);
    const float increment = frequency * invSampleRate_;
    const std::size_t wave = static_cast<std::size_t>(params_[0].optionIndex());
    const std::size_t nextWave = (wave + 1u) % kTableCount;
    const float morph = params_[1].value();
    const float a = tableSample(wave, phase_);
    const float b = tableSample(nextWave, phase_);
    float source = a + (b - a) * morph;

    const float sub = params_[2].value();
    if (sub > 0.0001f) {
        const float subOsc = subPhase_ < 0.5f ? 1.0f : -1.0f;
        source = source * (1.0f - 0.40f * sub) + subOsc * (0.40f * sub);
    }

    advancePhase(phase_, increment);
    advancePhase(subPhase_, increment * 0.5f);

    const float driven = fastSaturate(source - filter2_ * filterFeedback_);
    filter1_ += filterAlpha_ * (driven - filter1_);
    filter2_ += filterAlpha_ * (filter1_ - filter2_);

    float modeGain = 1.0f;
    if (mode_ == GrooveboxMode::Dub) modeGain = 0.90f;
    else if (mode_ == GrooveboxMode::Electro) modeGain = 1.06f;

    float out = filter2_ * ampEnv_ * velocityGain_ * modeGain * 1.20f;
    if (loFiAmount_ > 0.001f) {
        const float levels = 128.0f - loFiAmount_ * 96.0f;
        out = std::floor(out * levels + 0.5f) / levels;
    }

    const float dcBlocked = out - dcInput_ + kDcBlockPole * dcOutput_;
    dcInput_ = out;
    dcOutput_ = dcBlocked;
    return std::clamp(dcBlocked, -1.0f, 1.0f);
}

void WaveMorphSynthVoice::setParameterNormalized(uint8_t index, float norm) {
    if (index >= parameterCount()) return;
    params_[index].setNormalized(clamp01(norm));
    if (index == 3 || index == 4) updateFilterCoefficients();
    if (index == 5) updateEnvelopeCoefficients();
}

float WaveMorphSynthVoice::getParameterNormalized(uint8_t index) const {
    if (index >= parameterCount()) return 0.0f;
    return params_[index].normalized();
}

const Parameter& WaveMorphSynthVoice::getParameter(uint8_t index) const {
    if (index >= parameterCount()) return params_[0];
    return params_[index];
}

void WaveMorphSynthVoice::setLoFiAmount(float amount) {
    loFiAmount_ = clamp01(amount);
}
