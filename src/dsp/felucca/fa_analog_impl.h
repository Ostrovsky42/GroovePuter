/* SPDX-License-Identifier: MIT
 * GroovePuter experiment (not the product image): a one-voice instance of Felucca's ANALOG engine, as a monophonic
 * adapter for IMonoSynthVoice. The engine files (felucca_engine.h, dsp_impl.h, eng_analog_impl.h, fe_tables.h) are
 * Felucca-Engines' (MIT, Leo Kuroshita and Hügelton Instruments, see LICENSE.felucca), built for 22050 Hz here.
 *
 * Same adapter as the host experiment (experiments/felucca_analog/fa_analog.c), now per instance: two Synth voices
 * and engine switching each own their state. The host does here what Felucca's voice.c does: one voice, the ADSR
 * (per preset), the pitch with its 1/4096 residual, glide on slide, accent as amplitude only, and the output gain
 * (engineOutputGain, Q16, 0.44 calibrated in DSP-B). Notes above 0.45 FS are held at the top playable note and counted.
 *
 * Only compiled into the experimental image (GROOVEPUTER_FELUCCA_ANALOG). Included from felucca_analog_c.c. */
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define NVOICE 1
#define NPART 1
struct track;
typedef struct track track_t;

enum { P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_ATK, P_REL, P_FTYPE, P_COUNT };

#include "felucca_engine.h"

struct track {
    int16_t p[P_COUNT];
    voice_t v[NVOICE];
};

#include "fe_tables.h"     /* FS 22050, CTL 32 */
#include "dsp_impl.h"
#include "eng_analog_impl.h"

#ifndef NELEM
#define NELEM(a) (sizeof(a) / sizeof((a)[0]))
#endif

#define FA_ADAPT_SLIDE_S 0.06f

typedef struct fa_inst {
    track_t trk;
    vmod_t mod;
    float cur_midi, tgt_midi;
    float env_val, vel_gain, glide_tau;
    int env_stage, sounding;
    float env_attack_s, env_decay_s, env_sustain, env_release_s;
    int32_t out_gain_q16;
    unsigned range_clamps;
} fa_inst;

size_t fa_inst_bytes(void) { return sizeof(fa_inst); }

static int fa_playable_max_midi(void)
{
    float m = 69.0f + 12.0f * log2f(0.45f * (float)FS / 440.0f);
    int k = (int)floorf(m);
    return k > 127 ? 127 : k;
}

void fa_init(fa_inst *f)
{
    memset(f, 0, sizeof(*f));
    f->cur_midi = f->tgt_midi = 69.0f;
    f->env_attack_s = 0.003f;
    f->env_decay_s = 0.25f;
    f->env_sustain = 0.3f;
    f->env_release_s = 0.05f;
    f->out_gain_q16 = 28836;              /* engineOutputGain 0.44 */
}

void fa_set_param(fa_inst *f, int idx, int value)
{
    if (idx >= 0 && idx < P_COUNT)
        f->trk.p[idx] = (int16_t)value;
}

/* the library's 12 ANALOG presets (eng_analog_impl.h), loaded into this instance. Time mapping of ATK / DEC / REL is
 * the adapter's assumption (the library names the envelope values but not their times): 0..127 -> 2 ms .. 0.5 s
 * attack, 30 ms .. 1 s decay, 20 ms .. 0.8 s release; SUS is the level (0..1). */
int fa_preset_count(void) { return (int)NELEM(ANALOG_PRESETS); }

void fa_load_preset(fa_inst *f, int idx)
{
    const preset_t *pr;
    int k;
    if (idx < 0 || idx >= (int)NELEM(ANALOG_PRESETS))
        return;
    pr = &ANALOG_PRESETS[idx];
    for (k = 0; k < 8; k++)
        f->trk.p[P_E0 + k] = pr->e[k];
    f->env_attack_s = 0.002f + pr->env[0] / 127.0f * 0.5f;
    f->env_decay_s = 0.03f + pr->env[1] / 127.0f * 1.0f;
    f->env_sustain = pr->env[2] / 127.0f;
    f->env_release_s = 0.02f + pr->env[3] / 127.0f * 0.8f;
}

int fa_get_param(const fa_inst *f, int idx) { return f->trk.p[idx]; }
float fa_get_decay_s(const fa_inst *f) { return f->env_decay_s; }

void fa_set_decay_s(fa_inst *f, float seconds)
{
    f->env_decay_s = seconds;
}

void fa_preset(fa_inst *f, int id)
{
    int16_t *p = f->trk.p;
    memset(p, 0, sizeof(f->trk.p));
    p[P_E4] = 61;
    p[P_E5] = 83;
    p[P_E7] = 64;
    switch (id) {
    case 1: p[P_E2] = 64; break;                                   /* DUAL SAW */
    case 2:                                                         /* DEEP BASS */
        p[P_E0] = 6; p[P_E2] = 64; p[P_E4] = 40; p[P_E5] = 50;
        f->env_decay_s = 0.4f; f->env_sustain = 0.6f; f->env_release_s = 0.06f;
        return;
    case 3:                                                         /* ACID LEAD */
        p[P_E4] = 50; p[P_E5] = 110; p[P_E6] = 20; p[P_E7] = 90;
        f->env_decay_s = 0.15f; f->env_sustain = 0.2f; f->env_release_s = 0.04f;
        return;
    default: break;                                                 /* INIT: one saw */
    }
    f->env_decay_s = 0.25f;
    f->env_sustain = 0.3f;
    f->env_release_s = 0.05f;
}

void fa_note_off(fa_inst *f)
{
    f->env_stage = 3;
}

void fa_note_on(fa_inst *f, float freq_hz, int accent, int slide, int velocity)
{
    float midi = 69.0f + 12.0f * log2f(freq_hz / 440.0f);
    const float top = (float)fa_playable_max_midi();
    int was_sounding = f->sounding && f->env_val > 1e-4f;
    float g = (float)velocity / 127.0f;
    if (g < 0.05f) g = 0.05f;
    if (g > 1.0f) g = 1.0f;
    f->vel_gain = accent ? (g * 1.15f < 1.15f ? g * 1.15f : 1.15f) : g;
    if (midi > top) {
        midi = top;
        f->range_clamps++;
    }
    f->tgt_midi = midi;
    f->glide_tau = (slide && was_sounding) ? FA_ADAPT_SLIDE_S : 0.0f;
    if (slide && was_sounding)
        return;
    f->cur_midi = midi;
    if (was_sounding) {
        uint32_t ph[3] = {f->trk.v[0].ph[0], f->trk.v[0].ph[1], f->trk.v[0].ph[2]};
        int32_t s0 = f->trk.v[0].s[0], s1 = f->trk.v[0].s[1];
        analog_note_on(&f->trk, &f->trk.v[0]);
        memcpy(f->trk.v[0].ph, ph, sizeof(ph));
        f->trk.v[0].s[0] = s0;
        f->trk.v[0].s[1] = s1;
    } else {
        f->trk.v[0].age += 1;
        analog_note_on(&f->trk, &f->trk.v[0]);
    }
    f->env_val = 0.0f;
    f->env_stage = 1;
    f->sounding = 1;
    f->trk.v[0].note = (uint8_t)(midi + 0.5f);
    f->trk.v[0].gate = 1;
    f->trk.v[0].active = 1;
}

static float fa_adsr_step(fa_inst *f)
{
    const float blk = (float)CTL / (float)FS;
    switch (f->env_stage) {
    case 1:
        f->env_val += blk / f->env_attack_s;
        if (f->env_val >= 1.0f) { f->env_val = 1.0f; f->env_stage = 2; }
        break;
    case 2:
        f->env_val += (f->env_sustain - f->env_val) * (1.0f - expf(-blk / f->env_decay_s));
        break;
    case 3:
        f->env_val *= expf(-blk / f->env_release_s);
        if (f->env_val < 1e-4f) { f->env_val = 0.0f; f->env_stage = 0; f->sounding = 0; }
        break;
    default:
        f->env_val = 0.0f;
        break;
    }
    return f->env_val;
}

/* one control block (CTL samples) into out[0..CTL): the engine's output, gained, as int32 (full scale 32768) */
void fa_render_block(fa_inst *f, int32_t *out)
{
    track_t *t = &f->trk;
    voice_t *v = &t->v[0];
    float e0, e1, p16f, resid;
    int32_t p16, fine;
    int i;
    memset(out, 0, sizeof(int32_t) * CTL);
    if (f->glide_tau > 0.0f) {
        f->cur_midi += (f->tgt_midi - f->cur_midi) * (1.0f - expf(-(float)CTL / (float)FS / f->glide_tau));
        if (fabsf(f->tgt_midi - f->cur_midi) < 0.001f) { f->cur_midi = f->tgt_midi; f->glide_tau = 0.0f; }
    }
    e0 = f->env_val;
    e1 = fa_adsr_step(f);
    if (!f->sounding) {
        v->active = 0;
        v->gate = 0;
        return;
    }
    p16f = f->cur_midi * 16.0f;
    p16 = (int32_t)floorf(p16f + 0.5f);
    if (p16 < 0) p16 = 0;
    if (p16 > 2047) p16 = 2047;
    resid = (p16f - (float)p16) / 16.0f;
    fine = (int32_t)lroundf((powf(2.0f, resid / 12.0f) - 1.0f) * 4096.0f);
    f->mod.pitch16 = p16;
    f->mod.fine = fine;
    f->mod.inc = pitch_inc((uint32_t)p16) + (uint32_t)((int32_t)(pitch_inc((uint32_t)p16) >> 12) * fine);
    f->mod.amp0 = (int32_t)lroundf(e0 * f->vel_gain * 32767.0f);
    f->mod.amp1 = (int32_t)lroundf(e1 * f->vel_gain * 32767.0f);
    f->mod.cutoff = 0;
    f->mod.shape = 64 << 8;
    f->mod.envq15 = (int32_t)lroundf(e1 * 32767.0f);
    v->pitch16 = p16;
    v->pitch_cur = p16;
    v->env_out = f->mod.envq15;
    analog_render(t, v, out, (uint32_t)CTL, &f->mod);
    for (i = 0; i < CTL; i++)
        out[i] = (int32_t)(((int64_t)out[i] * f->out_gain_q16) >> 16);
}

unsigned fa_range_clamps(const fa_inst *f) { return f->range_clamps; }
