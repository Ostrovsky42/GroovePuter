/* SPDX-License-Identifier: MIT
 * Host-only experiment (DSP-B): a monophonic adapter around Felucca's ANALOG engine.
 * The engine files (fork22k/, copied from vendor/felucca/ and changed only as FORK.md says) are Leo Kuroshita's and
 * Hügelton Instruments' (MIT, see vendor/felucca/LICENSE).
 *
 * What the host (Felucca's voice.c / mod.c) would do and this adapter does instead, because the engine has no
 * such thing of its own:
 *   - one voice (NVOICE 1), a mono part: a new note on a held key glides (slide) or restarts (no slide);
 *   - a 1/16-semitone pitch plus a fine residual (1/4096) for the exact frequency;
 *   - an ADSR (per preset, see fa_preset), Q15 amplitude ramp per control block;
 *   - accent: velocity x 1.15 only (no filter-envelope accent: ANALOG has no such path);
 *   - note events are applied at the control-block boundary (32 samples);
 *   - the playable range: notes above 0.45 FS are held at the highest playable note and counted (fa_range_clamps),
 *     never played at a wrong (aliased) pitch;
 *   - an output gain (engineOutputGain, Q16) applied to the engine's block before it is added to out[].
 * Nothing here changes the engine: the render path is eng_analog.c + dsp.c, as shipped. */
#include <math.h>
#include <stdint.h>
#include <string.h>

#define NVOICE 1
#define NPART 1
struct track;
typedef struct track track_t;

/* the engine's parameter slots (felucca_engine.h: P_E0..P_E7, P_ATK, P_REL, P_FTYPE) */
enum { P_E0, P_E1, P_E2, P_E3, P_E4, P_E5, P_E6, P_E7, P_ATK, P_REL, P_FTYPE, P_COUNT };

#include "felucca_engine.h"

struct track {
    int16_t p[P_COUNT];
    voice_t v[NVOICE];
};

#include "fe_tables.h"          /* FS, CTL, SINE, PITCH_OCT, SVF_G, TANH_Q15 (+ rate constants off 44.1 kHz) */
#include "dsp_impl.h"                /* the fork's dsp.c (FORK.md) */
#include "eng_analog_impl.h"

static track_t trk[NPART];
static vmod_t mod;

/* adapter state (float: host only, not a target path) */
static float cur_midi = 69.0f, tgt_midi = 69.0f;
static float env_val = 0.0f;
static float vel_gain = 1.0f;
static int   env_stage = 0;         /* 0 off, 1 attack, 2 decay/sustain, 3 release */
static int   sounding = 0;
static float glide_tau = 0.0f;      /* s; 0 = no glide */
static float env_attack_s = 0.003f, env_decay_s = 0.25f, env_sustain = 0.3f, env_release_s = 0.05f;
static int32_t out_gain_q16 = 28836;   /* engineOutputGain: 0.44 = mean of the four presets vs SH101 (REPORT.md); start was 0.40 */
static unsigned range_clamps = 0;

#define ADAPT_SLIDE_S    0.06f

int fa_fs(void) { return FS; }
int fa_ctl(void) { return CTL; }
unsigned fa_voice_bytes(void) { return (unsigned)sizeof(voice_t) * NVOICE; }
unsigned fa_track_bytes(void) { return (unsigned)sizeof(trk); }
unsigned fa_table_bytes(void) {
    return (unsigned)(sizeof(SINE) + sizeof(PITCH_OCT) + sizeof(SVF_G) + sizeof(TANH_Q15));
}

/* the highest MIDI note whose frequency is at most 0.45 FS (upstream's cutoff ceiling; above it notes alias) */
int fa_playable_max_midi(void)
{
    float m = 69.0f + 12.0f * log2f(0.45f * (float)FS / 440.0f);
    int k = (int)floorf(m);
    return k > 127 ? 127 : k;
}

unsigned fa_range_clamps(void) { return range_clamps; }

void fa_set_gain_q16(int32_t g) { out_gain_q16 = g; }

/* the ANALOG engine's EDIT parameters and the ATK / REL / FTYPE slots, as 0..127 values (P_FTYPE 0 LP, 1 BP, 2 HP) */
void fa_set_param(int idx, int value)
{
    if (idx >= 0 && idx < P_COUNT)
        trk[0].p[idx] = (int16_t)value;
}

/* starting points (DSP-B decision: INIT is a single saw). They set the engine's EDIT values and the adapter's ADSR.
 * 0 INIT: one saw (MIX 0), 1 DUAL SAW: the engine's default quadrature pair (MIX 64),
 * 2 DEEP BASS: SUB wave (saw + square an octave down), 3 ACID LEAD: resonant saw with drive and a short decay */
void fa_preset(int id)
{
    int16_t *p = trk[0].p;
    memset(p, 0, sizeof(trk[0].p));
    p[P_E4] = 61;                        /* CUT ~600 Hz on the 30 Hz .. 16 kHz log scale */
    p[P_E5] = 83;                        /* RES ~0.6 */
    p[P_E7] = 64;                        /* KTR */
    switch (id) {
    case 1:
        p[P_E2] = 64;
        env_attack_s = 0.003f; env_decay_s = 0.25f; env_sustain = 0.3f; env_release_s = 0.05f;
        break;
    case 2:
        p[P_E0] = 6;                     /* WAVE SUB */
        p[P_E2] = 64;                    /* MIX: the sub's level */
        p[P_E4] = 40;
        p[P_E5] = 50;
        env_attack_s = 0.003f; env_decay_s = 0.4f; env_sustain = 0.6f; env_release_s = 0.06f;
        break;
    case 3:
        p[P_E4] = 50;
        p[P_E5] = 110;
        p[P_E6] = 20;                    /* DRV */
        p[P_E7] = 90;
        env_attack_s = 0.003f; env_decay_s = 0.15f; env_sustain = 0.2f; env_release_s = 0.04f;
        break;
    default:                             /* 0 INIT: one saw */
        env_attack_s = 0.003f; env_decay_s = 0.25f; env_sustain = 0.3f; env_release_s = 0.05f;
        break;
    }
}

void fa_reset(void)
{
    memset(trk, 0, sizeof(trk));
    memset(&mod, 0, sizeof(mod));
    cur_midi = tgt_midi = 69.0f;
    env_val = 0.0f;
    env_stage = 0;
    sounding = 0;
    range_clamps = 0;
}

void fa_note_off(void)
{
    env_stage = 3;
}

/* freq_hz > 0; accent 0/1; slide 0/1 (glides only if the previous note still sounds); velocity 0..127 */
void fa_note_on(float freq_hz, int accent, int slide, int velocity)
{
    float midi = 69.0f + 12.0f * log2f(freq_hz / 440.0f);
    const float top = (float)fa_playable_max_midi();
    int was_sounding = sounding && env_val > 1e-4f;
    float g = (float)velocity / 127.0f;
    if (g < 0.05f) g = 0.05f;
    if (g > 1.0f) g = 1.0f;
    vel_gain = accent ? (g * 1.15f < 1.15f ? g * 1.15f : 1.15f) : g;
    if (midi > top) {                    /* above 0.45 FS: held at the top playable note, and counted */
        midi = top;
        range_clamps++;
    }

    tgt_midi = midi;
    glide_tau = (slide && was_sounding) ? ADAPT_SLIDE_S : 0.0f;
    if (slide && was_sounding)           /* legato: the voice keeps sounding and glides to the new pitch */
        return;
    cur_midi = midi;
    if (was_sounding) {                  /* retrigger: note_on, then the state the header keeps (keep = 0x03) */
        uint32_t ph[3] = {trk[0].v[0].ph[0], trk[0].v[0].ph[1], trk[0].v[0].ph[2]};
        int32_t s0 = trk[0].v[0].s[0], s1 = trk[0].v[0].s[1];
        analog_note_on(&trk[0], &trk[0].v[0]);
        memcpy(trk[0].v[0].ph, ph, sizeof(ph));
        trk[0].v[0].s[0] = s0;
        trk[0].v[0].s[1] = s1;
    } else {
        trk[0].v[0].age += 1;            /* the host advances it per voice start (noise seed) */
        analog_note_on(&trk[0], &trk[0].v[0]);
    }
    env_val = 0.0f;
    env_stage = 1;
    sounding = 1;
    trk[0].v[0].note = (uint8_t)(midi + 0.5f);
    trk[0].v[0].gate = 1;
    trk[0].v[0].active = 1;
}

/* the ADSR's value at the end of one control block */
static float adsr_step(void)
{
    const float blk = (float)CTL / (float)FS;
    switch (env_stage) {
    case 1:
        env_val += blk / env_attack_s;
        if (env_val >= 1.0f) { env_val = 1.0f; env_stage = 2; }
        break;
    case 2:
        env_val += (env_sustain - env_val) * (1.0f - expf(-blk / env_decay_s));
        break;
    case 3:
        env_val *= expf(-blk / env_release_s);
        if (env_val < 1e-4f) { env_val = 0.0f; env_stage = 0; sounding = 0; }
        break;
    default:
        env_val = 0.0f;
        break;
    }
    return env_val;
}

/* renders n samples (a multiple of CTL) into out[] (added) */
void fa_render(int32_t *out, unsigned n)
{
    track_t *t = &trk[0];
    voice_t *v = &t->v[0];
    unsigned done;
    for (done = 0; done < n; done += CTL) {
        float e0, e1, p16f, resid;
        int32_t p16, fine, blk[CTL];
        int i;
        if (glide_tau > 0.0f) {
            cur_midi += (tgt_midi - cur_midi) * (1.0f - expf(-(float)CTL / (float)FS / glide_tau));
            if (fabsf(tgt_midi - cur_midi) < 0.001f) { cur_midi = tgt_midi; glide_tau = 0.0f; }
        }
        e0 = env_val;
        e1 = adsr_step();
        if (!sounding) {                 /* released and faded: the voice is off */
            v->active = 0;
            v->gate = 0;
            break;
        }
        p16f = cur_midi * 16.0f;         /* 1/16 semitone, MIDI 0 = 0 */
        p16 = (int32_t)floorf(p16f + 0.5f);
        if (p16 < 0) p16 = 0;
        if (p16 > 2047) p16 = 2047;
        /* the exact pitch: the residual below the 1/16 step, as a factor on the increment (1/4096) */
        resid = (p16f - (float)p16) / 16.0f;                              /* semitones */
        fine = (int32_t)lroundf((powf(2.0f, resid / 12.0f) - 1.0f) * 4096.0f);
        mod.pitch16 = p16;
        mod.fine = fine;
        mod.inc = pitch_inc((uint32_t)p16) + (uint32_t)((int32_t)(pitch_inc((uint32_t)p16) >> 12) * fine);
        mod.amp0 = (int32_t)lroundf(e0 * vel_gain * 32767.0f);
        mod.amp1 = (int32_t)lroundf(e1 * vel_gain * 32767.0f);
        mod.cutoff = 0;
        mod.shape = 64 << 8;
        mod.envq15 = (int32_t)lroundf(e1 * 32767.0f);
        v->pitch16 = p16;
        v->pitch_cur = p16;
        v->env_out = mod.envq15;
        memset(blk, 0, sizeof(blk));
        analog_render(t, v, blk, (uint32_t)CTL, &mod);
        for (i = 0; i < CTL; i++)        /* engineOutputGain on this block only, then add */
            out[done + i] += (int32_t)(((int64_t)blk[i] * out_gain_q16) >> 16);
    }
}
