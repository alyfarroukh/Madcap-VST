#include "fx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define DENORM 1e-18f
#define PI_F 3.14159265f

static inline float fast_tanh(float x)
{
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static inline float db2lin(float db) { return powf(10.0f, db * 0.05f); }

/* The Vita tuned its one-pole coefficients at 48 kHz: convert them so the
 * filters sound the same at any rate. */
static inline float rate_adapt(float a48, int sr)
{
    if (a48 >= 1.0f) return 1.0f;
    if (a48 <= 0.0f) return 0.0f;
    return 1.0f - powf(1.0f - a48, 48000.0f / (float)sr);
}

/* Wet/dry crossfade that keeps the dry signal full until mix passes 50%. */
static inline void mix_gains(float mix, float *dry, float *wet)
{
    *dry = mix < 0.5f ? 1.0f : 2.0f * (1.0f - mix);
    *wet = mix > 0.5f ? 1.0f : 2.0f * mix;
}

/* ---------------------------------------------------------------- drive */
typedef struct { float b0, b1, b2, a1, a2; } Biquad;

static Biquad lowpass(float fc, float q, float fs)
{
    float w = 2.0f * PI_F * fc / fs, c = cosf(w), s = sinf(w), al = s / (2.0f * q);
    float a0 = 1.0f + al;
    Biquad b;
    b.b0 = (1.0f - c) * 0.5f / a0;
    b.b1 = (1.0f - c) / a0;
    b.b2 = b.b0;
    b.a1 = -2.0f * c / a0;
    b.a2 = (1.0f - al) / a0;
    return b;
}

static inline float biquad(const Biquad *b, GsBiquadState *st, float x)
{
    float y = b->b0 * x + st->z1;           /* transposed direct form II */
    st->z1 = b->b1 * x - b->a1 * y + st->z2;
    st->z2 = b->b2 * x - b->a2 * y;
    return y;
}

static inline float shape(int type, float u)
{
    if (type == 0) return fast_tanh(u);                         /* tape: smooth symmetric saturation */
    u *= 1.5f;                                                  /* fuzz: asymmetric, harder */
    float y = u > 0 ? 1.0f - expf(-u) : -(1.0f - expf(u * 1.6f)) * 0.7f;
    y = y > 0.8f ? 0.8f : y < -0.8f ? -0.8f : y;
    return y * 1.2f;
}

void gs_drive_process(GsDrive *d, float *l, float *r, int n, float amt, int type, float tone, float mix, int os, int sr)
{
    if (mix <= 0.001f) return;
    float g = db2lin(amt * 36.0f);
    float trim = db2lin(-amt * 14.0f);
    float fc = 800.0f * powf(22.0f, tone);
    if (fc > 0.45f * sr) fc = 0.45f * sr;
    float a = 1.0f - expf(-2.0f * PI_F * fc / sr);
    float bits = 16.0f - amt * 13.0f;
    float q = powf(2.0f, bits - 1.0f);
    float hold_step = (1.0f / (1.0f + amt * 30.0f)) * (48000.0f / sr);   /* same crush rate at any sample rate */
    if (os != 2 && os != 4) os = 1;
    if (type == 2) os = 1;                         /* crush aliases on purpose */
    Biquad s1 = {0}, s2 = {0};
    if (os > 1) {                                  /* 4th-order Butterworth at 0.45 * base rate, run at os * sr */
        s1 = lowpass(0.45f * sr, 0.5412f, (float)(sr * os));
        s2 = lowpass(0.45f * sr, 1.3066f, (float)(sr * os));
    }
    float inv_os = 1.0f / os;
    for (int i = 0; i < n; i++) {
        float xl = l[i], xr = r[i], yl, yr;
        if (type == 2) {                           /* crush: bit depth + sample rate reduction */
            d->phase += hold_step;
            if (d->phase >= 1.0f) {
                d->phase -= floorf(d->phase);
                d->hold_l = floorf(xl * q + 0.5f) / q;
                d->hold_r = floorf(xr * q + 0.5f) / q;
            }
            yl = d->hold_l; yr = d->hold_r;
        } else if (os == 1) {
            yl = shape(type, xl * g) * trim;
            yr = shape(type, xr * g) * trim;
        } else {
            yl = yr = 0;
            for (int k = 1; k <= os; k++) {        /* linear-interpolated upsample -> shape -> AA filter */
                float t = k * inv_os;
                float ul = d->prev_l + (xl - d->prev_l) * t, ur = d->prev_r + (xr - d->prev_r) * t;
                float vl = shape(type, ul * g), vr = shape(type, ur * g);
                vl = biquad(&s2, &d->aa[0][1], biquad(&s1, &d->aa[0][0], vl));
                vr = biquad(&s2, &d->aa[1][1], biquad(&s1, &d->aa[1][0], vr));
                yl = vl; yr = vr;                  /* decimate: keep the last sub-sample */
            }
            yl *= trim; yr *= trim;
        }
        d->prev_l = xl; d->prev_r = xr;
        d->lp_l += a * (yl - d->lp_l) + DENORM;
        d->lp_r += a * (yr - d->lp_r) + DENORM;
        l[i] = xl + (d->lp_l - xl) * mix;
        r[i] = xr + (d->lp_r - xr) * mix;
    }
}

/* ----------------------------------------------------------- compressor */
void gs_comp_process(GsComp *c, float *l, float *r, int n, float thr, float ratio, float atk_ms, float rel_ms,
                     float makeup_db, float mix, int sr)
{
    if (mix <= 0.001f) { c->gr_db = 0; return; }
    float ka = expf(-1.0f / (fmaxf(atk_ms, 0.01f) * 0.001f * sr));
    float kr = expf(-1.0f / (fmaxf(rel_ms, 1.0f) * 0.001f * sr));
    const float knee = 6.0f;
    float slope = 1.0f - 1.0f / fmaxf(ratio, 1.0f);
    float max_gr = 0;
    for (int i = 0; i < n; i++) {
        float pk = fmaxf(fabsf(l[i]), fabsf(r[i]));
        float in_db = 20.0f * log10f(pk + 1e-9f);
        float k = in_db > c->env_db ? ka : kr;
        c->env_db = in_db + k * (c->env_db - in_db);
        if (c->env_db < -200.0f) c->env_db = -200.0f;
        float over = c->env_db - thr, gr;
        if (over <= -knee / 2) gr = 0;
        else if (over >= knee / 2) gr = over * slope;
        else { float t = over + knee / 2; gr = slope * t * t / (2 * knee); }
        if (gr > max_gr) max_gr = gr;
        float g = db2lin(makeup_db - gr);
        float wl = l[i] * g, wr = r[i] * g;
        l[i] += (wl - l[i]) * mix;
        r[i] += (wr - r[i]) * mix;
    }
    c->gr_db = max_gr;
}

/* ---------------------------------------------------------------- delay */
int gs_delay_init(GsDelay *d, int sr)
{
    memset(d, 0, sizeof(*d));
    d->len = (int)(2.1f * sr) + 64;                /* 2000 ms + wow/flutter headroom */
    d->buf_l = calloc((size_t)d->len, sizeof(float));
    d->buf_r = calloc((size_t)d->len, sizeof(float));
    d->time_smooth = 0.375f * sr;
    return (d->buf_l && d->buf_r) ? 0 : -1;
}
void gs_delay_free(GsDelay *d) { free(d->buf_l); free(d->buf_r); d->buf_l = d->buf_r = 0; d->len = 0; }

static inline float rd_frac(const float *b, int len, int w, float dly)
{
    float p = (float)w - dly;
    while (p < 0) p += len;
    int i0 = (int)p; float fr = p - i0;
    if (i0 >= len) i0 -= len;
    int i1 = i0 + 1; if (i1 >= len) i1 -= len;
    return b[i0] + (b[i1] - b[i0]) * fr;
}

static inline float lfo_sin(float ph)   /* ph 0..1 -> cheap sine */
{
    float x = ph * 2.0f - 1.0f;          /* -1..1 */
    return 4.0f * x * (1.0f - fabsf(x)); /* parabola approximation */
}

void gs_delay_process(GsDelay *d, float *l, float *r, int n, float time_ms, float fb, float damp, float ping,
                      float mix, int sr)
{
    if (!d->buf_l) return;
    float target = time_ms * 0.001f * sr;
    float wow_depth = 0.0007f * sr, flut_depth = 0.00012f * sr;   /* gentle tape wow / flutter */
    float maxd = d->len - 8 - wow_depth - flut_depth;
    if (target > maxd) target = maxd;
    if (target < 4) target = 4;
    float dryg, wetg; mix_gains(mix, &dryg, &wetg);
    float a = rate_adapt(1.0f - damp * 0.85f, sr);
    float glide = rate_adapt(0.0004f, sr);        /* tape-like time glide */
    float wow_inc = 0.55f / sr, flut_inc = 5.7f / sr;
    for (int i = 0; i < n; i++) {
        d->time_smooth += (target - d->time_smooth) * glide;
        d->wow_ph += wow_inc; if (d->wow_ph >= 1.0f) d->wow_ph -= 1.0f;
        d->flut_ph += flut_inc; if (d->flut_ph >= 1.0f) d->flut_ph -= 1.0f;
        float t = d->time_smooth + (lfo_sin(d->wow_ph) + 1.0f) * 0.5f * wow_depth
                                 + (lfo_sin(d->flut_ph) + 1.0f) * 0.5f * flut_depth;
        float dl = rd_frac(d->buf_l, d->len, d->w, t);
        float dr = rd_frac(d->buf_r, d->len, d->w, t);
        d->damp_l += a * (dl - d->damp_l) + DENORM;
        d->damp_r += a * (dr - d->damp_r) + DENORM;
        float fl = d->damp_l * fb, fr = d->damp_r * fb;
        float mono = 0.5f * (l[i] + r[i]);
        float wl = l[i] * (1 - ping) + mono * ping + fr * ping + fl * (1 - ping);
        float wr = r[i] * (1 - ping) + fl * ping + fr * (1 - ping);
        d->buf_l[d->w] = fast_tanh(wl);
        d->buf_r[d->w] = fast_tanh(wr);
        if (++d->w >= d->len) d->w = 0;
        l[i] = l[i] * dryg + dl * wetg;
        r[i] = r[i] * dryg + dr * wetg;
    }
}

/* --------------------------------------------------------------- reverb */
/* Line lengths in ms (mutually prime-ish), scaled by size. */
static const float LINE_MS[GS_REV_LINES] = {41.3f, 49.7f, 58.1f, 65.9f, 74.3f, 83.9f, 93.1f, 102.7f};
static const int DIFF_TUNE[GS_REV_DIFF] = {142, 107, 379, 277};

float gs_reverb_rt60(float size)
{
    return 0.8f * powf(75.0f, size);   /* 0.8 s .. 60 s */
}

float gs_reverb_size_for_rt60(float seconds)
{
    if (seconds <= 0.8f) return 0.0f;
    float s = logf(seconds / 0.8f) / logf(75.0f);
    return s > 1.0f ? 1.0f : s;
}

int gs_reverb_init(GsReverb *r, int sr)
{
    memset(r, 0, sizeof(*r));
    r->sr = sr;
    /* longest line: 102.7 ms * 2.0 (size scale) + 2 x 3.5 ms modulation */
    r->line_len = (int)(0.22f * sr) + 32;
    for (int i = 0; i < GS_REV_LINES; i++) {
        r->line[i] = calloc((size_t)r->line_len, sizeof(float));
        if (!r->line[i]) return -1;
        r->lfo_ph[i] = i / (float)GS_REV_LINES;
    }
    float sc = sr / 44100.0f;
    for (int c = 0; c < 2; c++)
        for (int k = 0; k < GS_REV_DIFF; k++) {
            int len = (int)((DIFF_TUNE[k] + (c ? 17 : 0)) * sc);
            r->diff_len[c][k] = len < 1 ? 1 : len;
            r->diff[c][k] = calloc((size_t)r->diff_len[c][k], sizeof(float));
            if (!r->diff[c][k]) return -1;
        }
    r->pre_len = (int)(0.2f * sr) + 4;
    r->pre = calloc((size_t)r->pre_len * 2, sizeof(float));
    r->size_s = 0.7f;
    return r->pre ? 0 : -1;
}

void gs_reverb_free(GsReverb *r)
{
    for (int i = 0; i < GS_REV_LINES; i++) free(r->line[i]);
    for (int c = 0; c < 2; c++) for (int k = 0; k < GS_REV_DIFF; k++) free(r->diff[c][k]);
    free(r->pre);
    memset(r, 0, sizeof(*r));
}

void gs_fx_clear(GsDelay *d, GsReverb *r)
{
    if (d->buf_l) memset(d->buf_l, 0, sizeof(float) * d->len);
    if (d->buf_r) memset(d->buf_r, 0, sizeof(float) * d->len);
    d->damp_l = d->damp_r = 0;
    for (int i = 0; i < GS_REV_LINES; i++) {
        if (r->line[i]) memset(r->line[i], 0, sizeof(float) * r->line_len);
        r->lp[i] = r->hp[i] = 0;
    }
    for (int c = 0; c < 2; c++) for (int k = 0; k < GS_REV_DIFF; k++)
        if (r->diff[c][k]) memset(r->diff[c][k], 0, sizeof(float) * r->diff_len[c][k]);
    if (r->pre) memset(r->pre, 0, sizeof(float) * r->pre_len * 2);
}

void gs_reverb_process(GsReverb *rv, float *l, float *r, int n, float size, float damp, float width, float pre_ms,
                       float mod, float mix, int sr)
{
    if (mix <= 0.001f || !rv->pre) return;
    float dryg, wetg; mix_gains(mix, &dryg, &wetg);

    /* glide size (~100 ms); derive line lengths and feedback gains for the target decay */
    rv->size_s += (size - rv->size_s) * (1.0f - expf(-(float)n / (0.1f * sr)));
    float sz = rv->size_s;
    float scale = 0.7f + 1.3f * sz;                 /* bigger space as decay grows */
    float rt = gs_reverb_rt60(sz);
    float len[GS_REV_LINES], g[GS_REV_LINES], dep0[GS_REV_LINES], dep1[GS_REV_LINES];
    float depth = mod * 0.0035f * sr;               /* up to ~3.5 ms of pitch wobble */
    float rate = 0.6f / sr * n;                     /* lfo advance per block (cycles) */
    for (int i = 0; i < GS_REV_LINES; i++) {
        len[i] = LINE_MS[i] * 0.001f * sr * scale;
        if (len[i] > rv->line_len - 2 * depth - 8) len[i] = rv->line_len - 2 * depth - 8;
        g[i] = powf(10.0f, -3.0f * len[i] / (rt * sr));
        if (sz >= GS_REV_INFINITE) g[i] = 0.9995f;  /* top of the knob: near-infinite wash */
        float ph0 = rv->lfo_ph[i];
        float ph1 = ph0 + rate * (0.55f + 0.13f * i);
        ph1 -= floorf(ph1);
        dep0[i] = (lfo_sin(ph0) + 1.0f) * depth;
        dep1[i] = (lfo_sin(ph1) + 1.0f) * depth;
        rv->lfo_ph[i] = ph1;
    }
    float a_lp = rate_adapt(1.0f - damp * 0.92f, sr);   /* damping: darker tails */
    float a_hp = rate_adapt(0.0035f, sr);               /* ~25 Hz: keep the low end from piling up */
    int pre = (int)(pre_ms * 0.001f * sr);
    if (pre >= rv->pre_len) pre = rv->pre_len - 1;
    if (pre < 0) pre = 0;
    float wl = 0.32f * wetg;
    float inv_n = 1.0f / n;
    const int L = rv->line_len;

    for (int s = 0; s < n; s++) {
        /* pre-delay (stereo, interleaved) */
        rv->pre[rv->pre_w * 2] = l[s];
        rv->pre[rv->pre_w * 2 + 1] = r[s];
        int rp = rv->pre_w - pre; if (rp < 0) rp += rv->pre_len;
        float in[2] = {rv->pre[rp * 2], rv->pre[rp * 2 + 1]};
        if (++rv->pre_w >= rv->pre_len) rv->pre_w = 0;

        /* input diffusion: smear transients into a cloud */
        for (int c = 0; c < 2; c++) {
            float x = in[c];
            for (int k = 0; k < GS_REV_DIFF; k++) {
                float *b = rv->diff[c][k]; int idx = rv->diff_i[c][k];
                float d = b[idx];
                float v = x + d * 0.62f;
                b[idx] = v;
                x = d - v * 0.62f;
                if (++idx >= rv->diff_len[c][k]) idx = 0;
                rv->diff_i[c][k] = idx;
            }
            in[c] = x;
        }

        /* read modulated lines */
        float t = s * inv_n, y[GS_REV_LINES], sum = 0;
        for (int i = 0; i < GS_REV_LINES; i++) {
            float dl = len[i] + dep0[i] + (dep1[i] - dep0[i]) * t;
            float p = (float)rv->w - dl;
            if (p < 0) p += L;
            int i0 = (int)p; float fr = p - i0;
            if (i0 >= L) i0 -= L;                   /* float rounding at the wrap */
            if (i0 < 0) i0 = 0;
            int i1 = i0 + 1; if (i1 >= L) i1 = 0;
            float v = rv->line[i][i0] + (rv->line[i][i1] - rv->line[i][i0]) * fr;
            rv->lp[i] += a_lp * (v - rv->lp[i]) + DENORM;      /* damping */
            rv->hp[i] += a_hp * (rv->lp[i] - rv->hp[i]) + DENORM;
            y[i] = (rv->lp[i] - rv->hp[i]) * g[i];
            sum += y[i];
        }
        /* Householder feedback matrix, inject input (L -> even lines, R -> odd) */
        float hh = sum * (2.0f / GS_REV_LINES);
        for (int i = 0; i < GS_REV_LINES; i++) {
            float v = y[i] - hh + in[i & 1] * 0.5f;
            rv->line[i][rv->w] = fast_tanh(v * 0.5f) * 2.0f;  /* soft safety limit */
        }
        if (++rv->w >= L) rv->w = 0;

        /* stereo out with mid/side width (0 = mono, 1 = natural, 1.5 = extra wide) */
        float ol = (y[0] - y[2] + y[4] - y[6]) + 0.5f * (y[1] + y[5]);
        float orr = (y[1] - y[3] + y[5] - y[7]) + 0.5f * (y[2] + y[6]);
        float mid = 0.5f * (ol + orr), side = 0.5f * (ol - orr) * width;
        l[s] = l[s] * dryg + (mid + side) * wl;
        r[s] = r[s] * dryg + (mid - side) * wl;
    }
}
