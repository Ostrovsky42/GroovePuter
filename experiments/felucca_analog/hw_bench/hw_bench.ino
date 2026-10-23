// SPDX-License-Identifier: MIT
// DSP-B hardware bench: Felucca ANALOG (native sample rate of this build) against SH101 and WAVEMORPH, on the
// Cardputer's ESP32-S3. A standalone sketch for measurement only: it is not GroovePuter and not in its firmware.
//
// Each engine renders one second of a held note in blocks of CTL samples (Felucca) or the same blocks sample by
// sample (GroovePuter voices). Per block the time is taken with esp_timer: load = total / 1 s, and the worst block
// against its budget (CTL / FS seconds). The FIR (decimation 44.1 -> 22.05) is timed only in the 44.1 kHz build.
// Output: one RESULT line per measurement on the USB serial, then DONE.
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <math.h>

#include "dsp/sh101_synth_voice.h"
#include "dsp/wave_morph_synth_voice.h"
#include "dsp/mono_synth_voice.h"

extern "C" {
int fa_fs(void);
int fa_ctl(void);
void fa_reset(void);
void fa_preset(int id);
void fa_note_on(float freq_hz, int accent, int slide, int velocity);
void fa_note_off(void);
void fa_render(int32_t *out, unsigned n);
unsigned fa_voice_bytes(void);
unsigned fa_table_bytes(void);
}

struct Result;                              // forward: Arduino's generated prototypes come before the struct

static const int kRuns = 3;                 // each measurement: kRuns seconds, the first is a warm-up and not printed
static const int kTaps = 145;               // the decimator's filter, as analyze.py (Kaiser, beta 5.65, fc 10474 Hz at 44.1k)
static float g_fir[kTaps];

static void fir_design(float fs_in) {
    const float fc = 10474.0f, beta = 5.65f;
    const float M = (kTaps - 1) / 2.0f, wc = 2.0f * (float)M_PI * fc / fs_in;
    auto i0 = [](float x) { float t = 1.0f, s = 1.0f; for (int k = 1; k < 60; ++k) { t *= (x / (2 * k)) * (x / (2 * k)); s += t; } return s; };
    float sum = 0.0f;
    for (int n = 0; n < kTaps; ++n) {
        const float t = n - M;
        const float sinc = (t == 0.0f) ? wc / (float)M_PI : sinf(wc * t) / ((float)M_PI * t);
        const float w = i0(beta * sqrtf(fmaxf(0.0f, 1.0f - (t / M) * (t / M)))) / i0(beta);
        g_fir[n] = sinc * w;
        sum += g_fir[n];
    }
    for (int n = 0; n < kTaps; ++n) g_fir[n] /= sum;
}

// the 2:1 decimator's output for `outs` output samples from a 44.1 kHz input buffer (every second output)
static void fir_block(const float *in, float *out, int outs) {
    for (int o = 0; o < outs; ++o) {
        const float *p = in + 2 * o;     // window ends at the input index 2*o (the group delay is part of the cost)
        float acc = 0.0f;
        for (int k = 0; k < kTaps; ++k) acc += g_fir[k] * p[kTaps - 1 - k];
        out[o] = acc;
    }
}

struct Result { double total_us; double max_block_us; };

static void report(const char *name, int fs, int ctl, Result r, int run) {
    const double budget = 1e6 * ctl / fs;
    Serial.printf("RESULT %s fs=%d run=%d load_pct=%.2f max_block_us=%.1f budget_us=%.1f worst_pct_of_budget=%.1f\n",
                  name, fs, run, r.total_us / 1e4, r.max_block_us, budget, 100.0 * r.max_block_us / budget);
}

// Felucca: one held note; the block = CTL samples in one fa_render call
static Result time_felucca(int fs, int ctl, int preset) {
    static int32_t buf[256];
    fa_reset();
    fa_preset(preset);
    fa_note_on(440.0f * powf(2.0f, (45 - 69) / 12.0f), 0, 0, 100);   // MIDI 45, 110 Hz
    Result r{0.0, 0.0};
    const int blocks = fs / ctl;
    for (int b = 0; b < blocks; ++b) {
        memset(buf, 0, sizeof(int32_t) * ctl);
        const int64_t t0 = esp_timer_get_time();
        fa_render(buf, ctl);
        const double dt = (double)(esp_timer_get_time() - t0);
        r.total_us += dt;
        if (dt > r.max_block_us) r.max_block_us = dt;
    }
    fa_note_off();
    return r;
}

// the GroovePuter voices: process() once per sample, timed in blocks of ctl samples
static Result time_voice(IMonoSynthVoice &v, int fs, int ctl, float freq) {
    v.startNote(freq, false, false, 100);
    volatile float sink = 0.0f;
    Result r{0.0, 0.0};
    const int blocks = fs / ctl;
    for (int b = 0; b < blocks; ++b) {
        const int64_t t0 = esp_timer_get_time();
        for (int i = 0; i < ctl; ++i) sink = sink + v.process();
        const double dt = (double)(esp_timer_get_time() - t0);
        r.total_us += dt;
        if (dt > r.max_block_us) r.max_block_us = dt;
    }
    v.release();
    (void)sink;
    return r;
}

static Result time_fir(int fs, int ctl) {
    // 1 s of output (fs / 2 samples) from a 4 kB circular input, so the buffers do not skew the heap numbers.
    // The MACs are the same for any input; the output is discarded.
    static float in[4096 + 256];
    static float out[64];
    for (int i = 0; i < 4096 + 256; ++i) in[i] = sinf(0.001f * i) * 0.5f;
    Result r{0.0, 0.0};
    const int outs = fs / 2;
    for (int o = 0; o < outs; o += ctl) {
        const int s = (2 * o) % 4096;    // contiguous window: s + 2*ctl + kTaps <= 4096 + 256
        const int64_t t0 = esp_timer_get_time();
        fir_block(in + s, out, ctl);
        const double dt = (double)(esp_timer_get_time() - t0);
        r.total_us += dt;
        if (dt > r.max_block_us) r.max_block_us = dt;
    }
    return r;
}

static void heap_line(const char *tag) {
    Serial.printf("HEAP %s free8=%u largest8=%u\n", tag,
                  (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
                  (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
}

void setup() {
    Serial.begin(115200);
    const uint32_t t_wait = millis();
    while (!Serial && millis() - t_wait < 3000) delay(10);
    delay(300);

    const int fs = fa_fs();
    const int ctl = fa_ctl();
    Serial.printf("BENCH build fs=%d ctl=%d cpu_mhz=%d chip=%s\n", fs, ctl, (int)getCpuFrequencyMhz(), ESP.getChipModel());
    Serial.printf("MEM felucca_voice_bytes=%u table_bytes=%u\n", fa_voice_bytes(), fa_table_bytes());
    heap_line("start");

    static Sh101SynthVoice sh(static_cast<float>(fs));
    static WaveMorphSynthVoice wm(static_cast<float>(fs));
    Serial.printf("MEM sh101_object=%u wavemorph_object=%u\n", (unsigned)sizeof(Sh101SynthVoice), (unsigned)sizeof(WaveMorphSynthVoice));

    const char *pnames[4] = {"INIT", "DUAL_SAW", "DEEP_BASS", "ACID_LEAD"};
    for (int p = 0; p < 4; ++p) {
        for (int run = 0; run < kRuns; ++run) {
            Result r = time_felucca(fs, ctl, p);
            if (run > 0) {
                char name[40];
                snprintf(name, sizeof name, "FELUCCA_%s", pnames[p]);
                report(name, fs, ctl, r, run);
            }
        }
    }
    heap_line("after_felucca");

    for (int run = 0; run < kRuns; ++run) {
        Result r = time_voice(sh, fs, ctl, 110.0f);
        if (run > 0) report("SH101", fs, ctl, r, run);
    }
    for (int run = 0; run < kRuns; ++run) {
        Result r = time_voice(wm, fs, ctl, 110.0f);
        if (run > 0) report("WAVEMORPH", fs, ctl, r, run);
    }

    if (fs == 44100) {
        fir_design(44100.0f);
        for (int run = 0; run < kRuns; ++run) {
            Result r = time_fir(fs, ctl);
            if (run > 0) {
                Serial.printf("RESULT FIR145_2to1 fs=%d run=%d load_pct=%.2f max_block_us=%.1f budget_us=%.1f worst_pct_of_budget=%.1f\n",
                              fs, run, r.total_us / 1e4, r.max_block_us, 1e6 * ctl / (fs / 2),
                              100.0 * r.max_block_us / (1e6 * ctl / (fs / 2)));
            }
        }
    }
    heap_line("end");
    Serial.println("DONE");
}

void loop() { delay(1000); }
