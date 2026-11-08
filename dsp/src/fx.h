/* Effects: drive/distortion (oversampled), compressor, tape delay, ambient FDN reverb.
 * All buffers are sized for the sample rate at init time (no fixed 48 kHz). */
#ifndef GS_FX_H
#define GS_FX_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct { float z1, z2; } GsBiquadState;

typedef struct {
    float lp_l, lp_r;      /* post tone filter */
    float hold_l, hold_r;  /* sample-and-hold for crush */
    float phase;
    float prev_l, prev_r;  /* last input (linear-interpolating upsampler) */
    GsBiquadState aa[2][2];/* anti-alias low-pass: 2 channels x 2 biquads */
} GsDrive;

typedef struct {
    float env_db;          /* detector state */
    float gr_db;           /* last gain reduction (for meter) */
} GsComp;

typedef struct {
    float *buf_l, *buf_r;
    int len;               /* buffer length (frames) */
    int w;
    float time_smooth;     /* samples */
    float damp_l, damp_r;
    float wow_ph, flut_ph;
} GsDelay;

/* Ambient reverb: input diffusion -> 8-line modulated feedback delay network
 * (Householder matrix), per-line damping, very long decays, mid/side width. */
#define GS_REV_LINES 8
#define GS_REV_DIFF 4
#define GS_SMEAR_STAGES 6        /* SMEAR mode: long modulated allpass diffusers per channel */
#define GS_REV_INFINITE 0.985f   /* decay values at/above this are "infinite" */
typedef struct {
    float *line[GS_REV_LINES]; int line_len; int w;
    float lp[GS_REV_LINES], hp[GS_REV_LINES];
    float lfo_ph[GS_REV_LINES];
    float *diff[2][GS_REV_DIFF]; int diff_len[2][GS_REV_DIFF]; int diff_i[2][GS_REV_DIFF];
    float *smear[2][GS_SMEAR_STAGES]; int smear_len; int smear_w;
    float smear_ph[GS_SMEAR_STAGES];
    float smear_lp[2];
    float mode_mix;    /* 0 = ambient, 1 = smear: crossfaded so switching never clicks */
    float *pre; int pre_len; int pre_w;
    float size_s;      /* smoothed size so turning the knob glides instead of clicking */
    int sr;
} GsReverb;

/* Decay time (RT60, seconds) for a 0..1 size value, and its inverse. */
float gs_reverb_rt60(float size);
float gs_reverb_size_for_rt60(float seconds);

int  gs_delay_init(GsDelay *d, int sr);
void gs_delay_free(GsDelay *d);
int  gs_reverb_init(GsReverb *r, int sr);
void gs_reverb_free(GsReverb *r);
void gs_fx_clear(GsDelay *d, GsReverb *r);

/* os: oversampling factor (1, 2 or 4) for the Tape / Fuzz shapers. */
void gs_drive_process(GsDrive *d, float *l, float *r, int n, float amt, int type, float tone, float mix, int os, int sr);
void gs_comp_process(GsComp *c, float *l, float *r, int n, float thr_db, float ratio, float atk_ms, float rel_ms,
                     float makeup_db, float mix, int sr);
void gs_delay_process(GsDelay *d, float *l, float *r, int n, float time_ms, float fb, float damp, float ping,
                      float mix, int sr);
/* mode 0 = AMBIENT (MOOD-like FDN), 1 = SMEAR (long modulated diffusion in front of the FDN:
 * transients dissolve into an ambient pad, longer and darker tail). */
void gs_reverb_process(GsReverb *rv, float *l, float *r, int n, float size, float damp, float width, float pre_ms,
                       float mod, float mix, int mode, int sr);

#ifdef __cplusplus
}
#endif
#endif
