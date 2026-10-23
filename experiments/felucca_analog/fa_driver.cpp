// SPDX-License-Identifier: MIT
// Host-only driver for the DSP-B experiment: renders one scene through Felucca ANALOG (fa_analog.c, C) and through
// GroovePuter's SH101 and WAVEMORPH voices (src/dsp, unchanged), at one sample rate, as 32-bit float WAVs.
//
// usage: fa_driver <fs> <outdir> render
//        fa_driver <fs> <outdir> sustain <midi> <seconds> [bright] [mix0]   one held note, same patch on all three
//        fa_driver <fs> <outdir> stress <notes>                      random on/off, checks finite output
//        fa_driver <fs> <outdir> cpu <seconds>                       wall time on this host (not ESP32 time)
//        fa_driver <fs> <outdir> sweep <lo> <hi> [bright] [preset=N]  Felucca only, one held note per MIDI number
// sustain/render take gain=<Q16> (engineOutputGain; 65536 = raw) and preset=<N> (0 INIT, 1 DUAL SAW, 2 DEEP BASS, 3 ACID LEAD)
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "sh101_synth_voice.h"
#include "wave_morph_synth_voice.h"

extern "C" {
int fa_fs(void);
int fa_ctl(void);
unsigned fa_voice_bytes(void);
unsigned fa_track_bytes(void);
unsigned fa_table_bytes(void);
void fa_set_param(int idx, int value);
void fa_preset(int id);
void fa_set_gain_q16(int32_t g);
unsigned fa_range_clamps(void);
int fa_playable_max_midi(void);
void fa_reset(void);
void fa_note_on(float freq_hz, int accent, int slide, int velocity);
void fa_note_off(void);
void fa_render(int32_t *out, unsigned n);
}

namespace {

struct Ev {
    uint64_t on;     // sample index
    uint64_t off;
    float midi;
    bool accent;
    bool slide;
    int vel;
};

// 16th notes at 120 BPM, two bars: acid-ish bass with accents and slides. A note gates to the next step's start when the
// next step slides (so the engines see a legato that can glide), else 60 % of the step.
// Event times are on the control-block grid (CTL samples) for every engine, so Felucca (block-rate) and the GroovePuter
// voices (sample-rate) get the same timeline: 1.45 ms at 22.05 kHz.
std::vector<Ev> melody(double fs, uint64_t ctl) {
    auto grid = [ctl](double samples) {
        return static_cast<uint64_t>(std::llround(samples / static_cast<double>(ctl))) * ctl;
    };
    struct S { int midi; bool acc; bool slide; };
    const S pat[16] = {
        {36, false, false}, {36, true, false}, {48, false, true}, {36, false, false},
        {39, false, false}, {36, true, false}, {46, false, true}, {36, false, false},
        {43, false, false}, {43, false, false}, {55, true, false}, {43, false, false},
        {41, false, true}, {36, false, false}, {38, true, false}, {41, false, false},
    };
    const double step = 60.0 / 120.0 / 4.0;
    std::vector<Ev> ev;
    for (int rep = 0; rep < 2; ++rep)
        for (int i = 0; i < 16; ++i) {
            const int k = rep * 16 + i;
            const S &nx = pat[(i + 1) % 16];
            const double on = k * step, len = step * (nx.slide ? 1.0 : 0.6);
            Ev e;
            e.on = grid(on * fs);
            e.off = grid((on + len) * fs);
            e.midi = static_cast<float>(pat[i].midi);
            e.accent = pat[i].acc;
            e.slide = pat[i].slide;
            e.vel = pat[i].acc ? 127 : 100;
            ev.push_back(e);
        }
    return ev;
}

float hz(float midi) { return 440.0f * std::pow(2.0f, (midi - 69.0f) / 12.0f); }

// the common engine face: note on/off; Felucca is block-rate (events on the CTL grid), the GroovePuter voices sample-rate
struct Engine {
    virtual ~Engine() = default;
    virtual const char *name() const = 0;
    virtual void on(float f, bool acc, bool slide, int vel) = 0;
    virtual void off() = 0;
    virtual void render(std::vector<float> &out, uint64_t from, uint64_t to) = 0;   // appends [from, to)
};

struct GpEngine final : Engine {
    IMonoSynthVoice *v;
    const char *nm;
    GpEngine(IMonoSynthVoice *voice, const char *n) : v(voice), nm(n) {}
    const char *name() const override { return nm; }
    void on(float f, bool acc, bool slide, int vel) override { v->startNote(f, acc, slide, static_cast<uint8_t>(vel)); }
    void off() override { v->release(); }
    void render(std::vector<float> &out, uint64_t from, uint64_t to) override {
        for (uint64_t i = from; i < to; ++i) out.push_back(v->process());
    }
};

struct FeluccaEngine final : Engine {
    const char *name() const override { return "FELUCCA_ANALOG"; }
    void on(float f, bool acc, bool slide, int vel) override { fa_note_on(f, acc, slide, vel); }
    void off() override { fa_note_off(); }
    void render(std::vector<float> &out, uint64_t from, uint64_t to) override {
        const unsigned blk = static_cast<unsigned>(fa_ctl());
        std::vector<int32_t> buf(blk);
        for (uint64_t i = from; i < to; i += blk) {
            std::fill(buf.begin(), buf.end(), 0);
            fa_render(buf.data(), blk);
            for (unsigned k = 0; k < blk && i + k < to; ++k)
                out.push_back(static_cast<float>(buf[k]) / 32768.0f);
        }
    }
};

// One patch for all three engines. The melody: saw, cutoff ~600 Hz, resonance 0.6 of its range, decay 300 ms.
// bright: filter wide open, no resonance, the alias test (a saw's harmonics above Nyquist are all there).
// Felucca's ADSR is the adapter's (fa_analog.c), so its decay is set by the adapter, not by the patch.
struct Rig {
    FeluccaEngine fel;
    Sh101SynthVoice sh;
    WaveMorphSynthVoice wm;
    GpEngine sh_e;
    GpEngine wm_e;
    Engine *engines[3];

    Rig(int fs, bool bright, int preset = 0)
        : sh(static_cast<float>(fs)), wm(static_cast<float>(fs)), sh_e(&sh, "SH101"), wm_e(&wm, "WAVEMORPH") {
        fa_reset();
        fa_preset(preset);                               // INIT (0) is a single saw; CUT 61 = 600 Hz, RES 83
        if (bright) {                                    // the alias test: filter wide open, no resonance
            fa_set_param(4, 127);
            fa_set_param(5, 0);
        }
        sh.setParameterNormalized(0, 0.0f);              // Wave: Saw
        sh.setParameterNormalized(1, 0.0f);              // Sub: off (ANALOG has no sub oscillator at WAVE 0)
        sh.setParameterNormalized(3, bright ? 1.0f : (600.0f - 80.0f) / (6500.0f - 80.0f));
        sh.setParameterNormalized(4, bright ? 0.0f : 0.6f / 0.92f);
        sh.setParameterNormalized(5, (300.0f - 30.0f) / (2400.0f - 30.0f));
        wm.setParameterNormalized(0, 2.0f / 7.0f);       // Wave: Saw (index 2 of 8)
        wm.setParameterNormalized(2, 0.0f);              // Sub: off
        wm.setParameterNormalized(3, bright ? 1.0f : (600.0f - 80.0f) / (7000.0f - 80.0f));
        wm.setParameterNormalized(4, bright ? 0.0f : 0.6f / 0.90f);
        wm.setParameterNormalized(5, (300.0f - 30.0f) / (2400.0f - 30.0f));
        engines[0] = &fel;
        engines[1] = &sh_e;
        engines[2] = &wm_e;
    }
};

void write_wav(const std::string &path, const std::vector<float> &x, int fs) {
    FILE *f = std::fopen(path.c_str(), "wb");
    if (!f) { std::perror(path.c_str()); std::exit(1); }
    const uint32_t n = static_cast<uint32_t>(x.size());
    const uint32_t data = n * 4, riff = 36 + data, rate = static_cast<uint32_t>(fs), brate = rate * 4;
    const uint16_t fmt = 3, ch = 1, bits = 32, align = 4;
    const uint32_t fmtlen = 16;
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVE", 1, 4, f);
    std::fwrite("fmt ", 1, 4, f); std::fwrite(&fmtlen, 4, 1, f);
    std::fwrite(&fmt, 2, 1, f); std::fwrite(&ch, 2, 1, f); std::fwrite(&rate, 4, 1, f); std::fwrite(&brate, 4, 1, f);
    std::fwrite(&align, 2, 1, f); std::fwrite(&bits, 2, 1, f);
    std::fwrite("data", 1, 4, f); std::fwrite(&data, 4, 1, f);
    std::fwrite(x.data(), 4, n, f);
    std::fclose(f);
}

struct Stats { double rms, peak, dc; size_t nonfinite; };
Stats stats(const std::vector<float> &x) {
    double s2 = 0, s1 = 0, pk = 0;
    size_t bad = 0;
    for (float v : x) {
        if (!std::isfinite(v)) { ++bad; continue; }
        s2 += static_cast<double>(v) * v;
        s1 += v;
        pk = std::fmax(pk, std::fabs(static_cast<double>(v)));
    }
    const double n = static_cast<double>(x.size());
    return {std::sqrt(s2 / n), pk, s1 / n, bad};
}

int run_render(int fs, const std::string &dir, int preset) {
    const uint64_t ctl = static_cast<uint64_t>(fa_ctl());
    std::vector<Ev> ev = melody(fs, ctl);
    const uint64_t total = (ev.back().off + static_cast<uint64_t>(0.3 * fs) + ctl - 1) / ctl * ctl;
    Rig rig(fs, false, preset);
    for (Engine *e : rig.engines) {
        std::vector<float> out;
        out.reserve(total);
        size_t next = 0;      // event index: 2k = note k on, 2k+1 = note k off
        uint64_t pos = 0;
        while (pos < total) {
            uint64_t stop = total;
            if (next < ev.size() * 2) {
                const size_t k = next / 2;
                const uint64_t t = (next % 2 == 0) ? ev[k].on : ev[k].off;
                if (t < stop) stop = t;
            }
            e->render(out, pos, stop);
            pos = stop;
            while (next < ev.size() * 2) {
                const size_t k = next / 2;
                const bool is_on = (next % 2 == 0);
                const uint64_t t = is_on ? ev[k].on : ev[k].off;
                if (t > pos) break;
                if (is_on) e->on(hz(ev[k].midi), ev[k].accent, ev[k].slide, ev[k].vel);
                else e->off();
                ++next;
            }
        }
        const Stats s = stats(out);
        std::string path = dir + "/melody_" + e->name() + "_" + std::to_string(fs) + "_p" + std::to_string(preset) + ".wav";
        write_wav(path, out, fs);
        std::printf("%-15s fs=%d samples=%zu rms=%.5f peak=%.5f dc=%+.6f nonfinite=%zu -> %s\n", e->name(), fs,
                    out.size(), s.rms, s.peak, s.dc, s.nonfinite, path.c_str());
    }
    std::printf("memory: felucca voice_t x NVOICE=%u B, track=%u B, tables=%u B | "
                "SH101 object=%zu B, WAVEMORPH object=%zu B (wavetables not included)\n",
                fa_voice_bytes(), fa_track_bytes(), fa_table_bytes(), sizeof(Sh101SynthVoice),
                sizeof(WaveMorphSynthVoice));
    return 0;
}

// one held note per engine, same patch: pitch, spectrum, attack
int run_sustain(int fs, const std::string &dir, float midi, double seconds, bool bright, int preset) {
    const uint64_t n = static_cast<uint64_t>(seconds * fs);
    const uint64_t ctl = static_cast<uint64_t>(fa_ctl());
    const uint64_t off = (n / ctl) * ctl;
    Rig rig(fs, bright, preset);
    for (Engine *e : rig.engines) {
        std::vector<float> out;
        e->on(hz(midi), false, false, 100);
        e->render(out, 0, off);
        e->off();
        e->render(out, off, n);
        std::string path = dir + "/sustain_" + e->name() + "_" + std::to_string(fs) + "_m" +
                           std::to_string(static_cast<int>(midi)) + (bright ? "_bright" : "") + "_p" + std::to_string(preset) + ".wav";
        write_wav(path, out, fs);
        const Stats s = stats(out);
        std::printf("sustain %-15s midi=%.1f rms=%.5f peak=%.5f -> %s\n", e->name(), midi, s.rms, s.peak, path.c_str());
    }
    return 0;
}

// random note on / off with random lengths, slides and accents: checks finite output
int run_stress(int fs, uint32_t count) {
    uint32_t rng = 0x9e3779b9u;
    auto next = [&rng]() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; };
    const uint32_t ctl = static_cast<uint32_t>(fa_ctl());
    Rig rig(fs, false);
    int bad_total = 0;
    for (Engine *e : rig.engines) {
        float peak = 0.0f;
        size_t bad = 0, samples = 0;
        std::vector<float> buf;
        for (uint32_t i = 0; i < count; ++i) {
            const float m = 36.0f + static_cast<float>(next() % 36u);
            const int acc = next() & 1u, sl = (next() >> 3) & 1u;
            const int vel = 1 + static_cast<int>(next() % 127u);
            e->on(hz(m), acc != 0, sl != 0, vel);
            buf.clear();
            e->render(buf, 0, (1 + next() % (static_cast<uint32_t>(fs) / (20u * ctl))) * ctl);   // up to 50 ms
            if (next() & 1u) e->off();
            for (float v : buf) {
                if (!std::isfinite(v)) ++bad; else peak = std::fmax(peak, std::fabs(v));
            }
            samples += buf.size();
        }
        e->off();
        std::printf("stress %-15s notes=%u samples=%zu peak=%.4f nonfinite=%zu\n", e->name(), count, samples, peak, bad);
        bad_total += static_cast<int>(bad);
    }
    return bad_total == 0 ? 0 : 2;
}

// Felucca only: one steady note per MIDI number in [lo, hi], for the pitch and alias sweep (analyze.py sweep)
int run_sweep(int fs, const std::string &dir, int lo, int hi, bool bright, int preset) {
    const uint64_t ctl = static_cast<uint64_t>(fa_ctl());
    const uint64_t n = (static_cast<uint64_t>(0.6 * fs) / ctl) * ctl;   // 0.6 s: resolution for MIDI 24 (32.7 Hz)
    for (int m = lo; m <= hi; ++m) {
        Rig rig(fs, bright, preset);
        std::vector<float> out;
        rig.fel.on(hz(static_cast<float>(m)), false, false, 100);
        rig.fel.render(out, 0, n);
        std::string path = dir + "/sweep_" + std::to_string(fs) + "_m" + std::to_string(m) +
                           (bright ? "_bright" : "") + "_p" + std::to_string(preset) + ".wav";
        write_wav(path, out, fs);
        std::printf("sweep m%d clamps=%u\n", m, fa_range_clamps());
    }
    std::printf("playable max midi at fs %d: %d\n", fs, fa_playable_max_midi());
    return 0;
}

int run_cpu(int fs, double seconds) {
    const uint64_t n = static_cast<uint64_t>(seconds * fs);
    Rig rig(fs, false);
    for (Engine *e : rig.engines) {
        std::vector<float> buf;
        buf.reserve(n);
        e->on(hz(45.0f), false, false, 110);
        auto t0 = std::chrono::steady_clock::now();
        e->render(buf, 0, n);
        auto t1 = std::chrono::steady_clock::now();
        const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count();
        std::printf("cpu %-15s fs=%d: %.1f ns/sample on this host (x%.0f realtime on this host)\n", e->name(), fs,
                    ns / static_cast<double>(n), (seconds * 1e9) / ns);
    }
    return 0;
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 4) {
        std::fprintf(stderr, "usage: %s <fs> <outdir> render|sustain <midi> <s> [bright]|stress <n>|cpu <s>\n", argv[0]);
        return 1;
    }
    const int fs = std::atoi(argv[1]);
    const std::string dir = argv[2];
    const std::string mode = argv[3];
    if (fs != fa_fs()) {
        std::fprintf(stderr, "fs mismatch: built for %d, asked %d\n", fa_fs(), fs);
        return 1;
    }
    if (mode == "render") {
        int preset = 0;
        for (int i = 4; i < argc; ++i)
            if (std::string(argv[i]).rfind("preset=", 0) == 0) preset = std::atoi(argv[i] + 7);
        return run_render(fs, dir, preset);
    }
    if (mode == "stress" && argc >= 5) return run_stress(fs, static_cast<uint32_t>(std::atoi(argv[4])));
    if (mode == "cpu" && argc >= 5) return run_cpu(fs, std::atof(argv[4]));
    if (mode == "sweep" && argc >= 6) {
        bool bright = false;
        int preset = 0;
        for (int i = 6; i < argc; ++i) {
            const std::string a = argv[i];
            bright = bright || a == "bright";
            if (a.rfind("preset=", 0) == 0) preset = std::atoi(a.c_str() + 7);
        }
        return run_sweep(fs, dir, std::atoi(argv[4]), std::atoi(argv[5]), bright, preset);
    }
    if (mode == "sustain" && argc >= 6) {
        bool bright = false;
        int preset = 0;
        for (int i = 6; i < argc; ++i) {
            const std::string a = argv[i];
            bright = bright || a == "bright";
            if (a.rfind("preset=", 0) == 0) preset = std::atoi(a.c_str() + 7);
            if (a.rfind("gain=", 0) == 0) fa_set_gain_q16(static_cast<int32_t>(std::atoi(a.c_str() + 5)));
        }
        return run_sustain(fs, dir, static_cast<float>(std::atof(argv[4])), std::atof(argv[5]), bright, preset);
    }
    std::fprintf(stderr, "unknown mode\n");
    return 1;
}
