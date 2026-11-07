#include "engine.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define PI_F 3.14159265f

static inline float frand(GsEngine *e) /* 0..1 */
{
    e->rng = e->rng * 1664525u + 1013904223u;
    return (e->rng >> 8) * (1.0f / 16777216.0f);
}
static inline float brand(GsEngine *e) { return frand(e) * 2.0f - 1.0f; } /* -1..1 */
static inline int ip(const GsEngine *e, int id) { return (int)lroundf(e->p[id]); }

void gs_engine_reset_params(GsEngine *e)
{
    for (int i = 0; i < P_COUNT; i++) e->p[i] = GS_PARAMS[i].def;
    e->degree_mask = 1;
}

int gs_engine_init(GsEngine *e, int sr)
{
    memset(e, 0, sizeof(*e));
    if (sr < 8000) sr = 8000;
    if (sr > 768000) sr = 768000;
    e->sr = sr;
    e->rng = 0x1234567u;
    e->arp_dir = 1;
    e->playing = 1;
    e->gate = 1;
    e->gate_prev = 1;
    e->gate_env = 1.0f;
    for (int i = 0; i <= GS_WIN_N + 1; i++) {
        int k = i > GS_WIN_N ? GS_WIN_N : i;
        e->win[i] = 0.5f - 0.5f * cosf(2.0f * PI_F * k / GS_WIN_N);   /* Hann */
    }
    gs_engine_reset_params(e);
    if (gs_delay_init(&e->dly, sr)) return -1;
    if (gs_reverb_init(&e->rev, sr)) return -1;
    gs_img_init(&e->img);
    e->live_len = GS_LIVE_SECONDS * sr;
    e->live = calloc((size_t)e->live_len, sizeof(float));
    if (!e->live) return -1;
    gs_arp_reset(&e->arp);
    e->out_gain = 0.0f;
    e->lim_g = 1.0f; e->lim_hold = 1.0f;
    e->lim_la = (int)(0.0015f * sr + 0.5f);                  /* 1.5 ms look-ahead */
    if (e->lim_la < 1) e->lim_la = 1;
    if (e->lim_la > GS_LIM_MAX) e->lim_la = GS_LIM_MAX;
    e->bounds[0] = 0; e->bounds[1] = 1; e->nslices = 1;
    e->host_ppq = -1;
    e->ui_preview_pos = -1;
    return 0;
}

void gs_engine_free(GsEngine *e)
{
    free(e->live);
    gs_delay_free(&e->dly);
    gs_reverb_free(&e->rev);
    memset(e, 0, sizeof(*e));
}

int gs_engine_latency(const GsEngine *e) { return e->lim_la; }

void gs_engine_set_host(GsEngine *e, int playing, double ppq, double bar_start_ppq)
{
    e->host_playing = playing && ppq >= 0;
    e->host_ppq = ppq;
    e->host_bar_ppq = bar_start_ppq >= 0 && bar_start_ppq <= ppq ? bar_start_ppq : 0;
}

/* ------------------------------------------------------------ source swap */
const float *gs_engine_swap_source(GsEngine *e, const float *buf, int len, const GsSlicer *sl)
{
    const float *old = e->src;
    if (!buf || len < 1) { buf = NULL; len = 0; }
    e->src = buf;
    e->src_len = len;
    if (sl) e->slicer = *sl;
    else e->slicer.n_onsets = 0;
    e->slicer.len = len;
    if (!e->live_mode) {
        /* grains and arp notes point into the old buffer: drop them */
        e->ngrains = 0;
        memset(e->arp.v, 0, sizeof e->arp.v);
        e->gsrc = buf; e->gsrc_len = len;
    }
    e->playhead = 0;
    e->preview_pos = 0;
    return old;
}

void gs_engine_region(const GsEngine *e, int *s, int *en)
{
    *s = e->region_s; *en = e->region_e;
}

/* --------------------------------------------------------------- chords */
static void update_voices(GsEngine *e, int n)
{
    float tgt[GS_MAX_VOICES];
    int nv = gs_build_chord(ip(e, P_SCALE), e->degree_mask, ip(e, P_CHORD), e->p[P_VOICING], tgt);
    if (nv < 1) { nv = 1; tgt[0] = 0; }
    int glide_mode = ip(e, P_MODE) == MODE_GLIDE;
    for (int i = 0; i < nv; i++) {
        if (i >= e->nvoices) e->voice_cur[i] = glide_mode && e->nvoices > 0 ? e->voice_cur[e->nvoices - 1] : tgt[i];
        e->voice_tgt[i] = tgt[i];
    }
    e->nvoices = nv;
    float gms = glide_mode ? e->p[P_GLIDE] : 0.0f;
    float k = gms > 1.0f ? 1.0f - expf(-(float)n / (gms * 0.001f * e->sr)) : 1.0f;
    for (int i = 0; i < nv; i++) e->voice_cur[i] += (e->voice_tgt[i] - e->voice_cur[i]) * k;
    if (e->arp_idx >= nv) e->arp_idx = 0;
}

static void update_region(GsEngine *e)
{
    int cnt = GS_SLICE_COUNTS[ip(e, P_SLICES)];
    e->slicer.len = e->src_len;
    /* manual cut: everything happens between the IN and OUT points */
    float tin = e->p[P_TRIM_IN], tout = e->p[P_TRIM_OUT];
    if (tin < 0) tin = 0;
    if (tout > 1) tout = 1;
    if (tout < tin) { float t = tin; tin = tout; tout = t; }
    int a = (int)((double)tin * e->src_len), b = (int)((double)tout * e->src_len);
    if (b - a < 1024) { b = a + 1024; if (b > e->src_len) { b = e->src_len; a = b - 1024; if (a < 0) a = 0; } }
    e->trim_s = a; e->trim_e = b;
    gs_slicer_bounds(&e->slicer, cnt, ip(e, P_SLICE_MODE), a, b, e->bounds);
    e->nslices = cnt;
    int sel = ip(e, P_SLICE_SEL); if (sel >= cnt) sel = cnt - 1; if (sel < 0) sel = 0;
    int span = ip(e, P_SLICE_SPAN); if (span < 1) span = 1;
    int end = sel + span; if (end > cnt) end = cnt;
    e->region_s = e->bounds[sel];
    e->region_e = e->bounds[end];
    if (e->region_e - e->region_s < 256) {           /* tiny region guard */
        e->region_s = a; e->region_e = b;
    }
    if (e->region_s < 0) e->region_s = 0;
    if (e->region_e > e->src_len) e->region_e = e->src_len;
}

/* ------------------------------------------------------------- spawning */
static void spawn(GsEngine *e, int offset, int voice, float len_mul, float extra_semis, float amp_mul, float force_rev)
{
    if (e->ngrains >= GS_MAX_GRAINS || !e->gsrc || e->gsrc_len < 512 || !e->gate) return;
    int rs = e->live_mode ? 0 : e->region_s, re = e->live_mode ? e->gsrc_len : e->region_e, rl = re - rs;
    if (rl < 256 || rs < 0 || re > e->gsrc_len) return;

    float semis = e->voice_cur[voice % e->nvoices];
    /* sample: transpose from the sample's root; image: from the image sweeper's key */
    int t = ip(e, P_KEY) - (e->live_mode ? ip(e, P_IMG_KEY) : ip(e, P_SAMPLE_ROOT));
    t = ((t % 12) + 12) % 12; if (t > 6) t -= 12;
    semis += t + 12.0f * ip(e, P_OCTAVE) + extra_semis;
    if (frand(e) < e->p[P_OCTAVES]) semis += frand(e) < 0.6f ? 12.0f : -12.0f;
    semis += brand(e) * e->p[P_DETUNE] * 0.01f;
    float rate = powf(2.0f, semis / 12.0f);
    if (frand(e) < fmaxf(e->p[P_REVERSE], force_rev)) rate = -rate;

    float pos = e->p[P_POSITION] + e->playhead + brand(e) * 0.5f * e->p[P_SPRAY];
    pos -= floorf(pos);

    GsGrain *g = &e->grains[e->ngrains++];
    g->rs = rs; g->re = re;
    g->rate = rate;
    g->len = (int)(e->p[P_SIZE] * len_mul * 0.001f * e->sr);
    if (g->len < 64) g->len = 64;
    if (e->live_mode) {
        /* live buffer: POSITION = how far back from "now" the grain starts (0 = most recent).
         * Keep enough distance that a forward grain doesn't run into the write head. */
        float need = g->len * fabsf(rate) + 512;
        if (need > rl * 0.5f) need = rl * 0.5f;
        float back = need + pos * (rl - need - 512);
        double p0 = (double)e->live_w - back;
        while (p0 < 0) p0 += rl;
        g->pos = p0;
    } else {
        g->pos = rs + (double)pos * rl;
    }
    if (g->pos >= re || g->pos < rs) g->pos = rs;   /* float rounding guard */
    g->age = 0;
    g->delay = offset;

    /* overlap-aware gain so density changes don't blow up the level */
    float dens = e->p[P_DENSITY];
    float overlap = dens * (g->len / (float)e->sr) * amp_mul;
    float amp = e->p[P_GRAIN_LVL] * 1.1f / sqrtf(fmaxf(1.0f, overlap));
    float pan = brand(e) * e->p[P_SPREAD];
    float a = (pan + 1.0f) * 0.25f * PI_F;
    g->gl = cosf(a) * amp;
    g->gr = sinf(a) * amp;
}

static float steps_per_beat(GsEngine *e)
{
    static const float opts[] = {0.5f, 1, 2, 3, 4, 6, 8};
    float want = e->p[P_DENSITY] * 60.0f / e->p[P_BPM];
    float best = opts[0];
    for (int i = 0; i < 7; i++) if (fabsf(opts[i] - want) < fabsf(best - want)) best = opts[i];
    return best;
}

/* beats between spawns for the tempo-synced modes, 0 for free-running ones */
static double synced_interval_beats(GsEngine *e)
{
    switch (ip(e, P_MODE)) {
    case MODE_MOSAIC:
    case MODE_ARP:    return 1.0 / steps_per_beat(e);
    case MODE_STRUM:  return e->p[P_DENSITY] < 6 ? 2.0 : 1.0;
    default:          return 0.0;
    }
}

static void schedule(GsEngine *e, int n)
{
    int mode = ip(e, P_MODE);
    float beat = 60.0f / e->p[P_BPM] * e->sr;
    if (e->spawn_t > 4.0 * e->sr * 60.0) e->spawn_t = 0;   /* sanity after big tempo changes */
    while (e->spawn_t < n) {
        int off = (int)e->spawn_t; if (off < 0) off = 0;
        int nv = e->nvoices;
        double next;
        switch (mode) {
        default:
        case MODE_CLOUD:
        case MODE_GLIDE:
            spawn(e, off, (int)(frand(e) * nv), 1.0f, 0, 1.0f, 0);
            next = e->sr / e->p[P_DENSITY] * (0.5 + frand(e));
            break;
        case MODE_MOSAIC: {
            float spb = steps_per_beat(e);
            /* mosaic: rhythmic, random voices, occasional double hits */
            spawn(e, off, (int)(frand(e) * nv), 1.0f, 0, 1.0f, 0);
            if (frand(e) < 0.25f) spawn(e, off, (int)(frand(e) * nv), 0.5f, 12, 1.0f, 0);
            next = beat / spb;
            break; }
        case MODE_ARP: {
            float spb = steps_per_beat(e);
            spawn(e, off, e->arp_idx, 1.0f, 0, 1.0f, 0);
            if (nv > 1) {
                e->arp_idx += e->arp_dir;
                if (e->arp_idx >= nv) { e->arp_idx = nv - 2 < 0 ? 0 : nv - 2; e->arp_dir = -1; }
                if (e->arp_idx < 0) { e->arp_idx = nv > 1 ? 1 : 0; e->arp_dir = 1; }
            } else e->arp_idx = 0;
            next = beat / spb;
            break; }
        case MODE_STRUM: {
            /* every beat: strum all chord tones upward with a short stagger */
            float stagger = (0.02f + 0.05f * (1.0f - e->p[P_DENSITY] / 60.0f)) * e->sr;
            for (int v = 0; v < nv; v++)
                spawn(e, off + (int)(v * stagger), v, 2.0f, 0, (float)nv * 0.5f, 0);
            next = e->p[P_DENSITY] < 6 ? beat * 2 : beat;
            break; }
        case MODE_TUNNEL:
            /* long, slow, half-reversed grains with a sub octave: a drone tunnel */
            spawn(e, off, e->step_count % (nv > 0 ? nv : 1), 4.0f, frand(e) < 0.5f ? -12.0f : 0.0f, 1.5f, 0.5f);
            next = e->sr / e->p[P_DENSITY] * (0.7 + 0.6 * frand(e)) * 1.5;
            break;
        }
        if (next < 1.0) next = 1.0;
        e->step_count++;
        e->spawn_t += next;
    }
    e->spawn_t -= n;
}

/* --------------------------------------------------------------- render */
static inline float hermite(float xm1, float x0, float x1, float x2, float t)
{
    float c = (x1 - xm1) * 0.5f;
    float v = x0 - x1;
    float w = c + v;
    float a = w + v + (x2 - x0) * 0.5f;
    float b = w + a;
    return (((a * t) - b) * t + c) * t + x0;
}

static void render_grains(GsEngine *e, float *L, float *R, int n, float bend)
{
    const float *src = e->gsrc;
    int cubic = ip(e, P_GRAIN_INTERP) == 1;
    int w = 0;
    for (int gi = 0; gi < e->ngrains; gi++) {
        GsGrain *g = &e->grains[gi];
        int i = g->delay > 0 ? g->delay : 0;
        if (i >= n) { g->delay -= n; e->grains[w++] = *g; continue; }
        g->delay = 0;
        int start = i;
        double pos = g->pos;
        int rs = g->rs, re = g->re, rl = re - rs;
        if (!src || rs < 0 || re > e->gsrc_len || rl <= 4) continue;  /* never read outside the buffer */
        float winc = (float)GS_WIN_N / g->len;
        float wp = g->age * winc;
        int remain = g->len - g->age;
        int end = i + remain < n ? i + remain : n;
        double step = (double)g->rate * bend;
        for (; i < end; i++) {
            int i0 = (int)pos;
            if (i0 >= re) i0 = re - 1;
            if (i0 < rs) i0 = rs;
            float fr = (float)(pos - i0);
            int i1 = i0 + 1; if (i1 >= re) i1 = rs;
            float s;
            if (cubic) {
                int im = i0 - 1; if (im < rs) im = re - 1;
                int i2 = i1 + 1; if (i2 >= re) i2 = rs;
                s = hermite(src[im], src[i0], src[i1], src[i2], fr);
            } else {
                s = src[i0] + (src[i1] - src[i0]) * fr;
            }
            int wi = (int)wp;
            if (wi >= GS_WIN_N) wi = GS_WIN_N - 1;
            float wv = e->win[wi] + (e->win[wi + 1] - e->win[wi]) * (wp - wi);
            s *= wv;
            L[i] += s * g->gl;
            R[i] += s * g->gr;
            wp += winc;
            pos += step;
            if (pos >= re) pos -= rl;
            else if (pos < rs) pos += rl;
            if (pos >= re || pos < rs) pos = rs;          /* extreme rate guard */
        }
        g->age += end - start;
        g->pos = pos;
        if (g->age < g->len) e->grains[w++] = *g;   /* keep live grains, compacting the array */
    }
    e->ngrains = w;
}

static void tone_filter(GsEngine *e, float *L, float *R, int n)
{
    float t = e->p[P_TONE];
    if (fabsf(t) < 0.02f) { e->svf_ic1[0] = e->svf_ic1[1] = e->svf_ic2[0] = e->svf_ic2[1] = 0; return; }
    int hp = t > 0;
    float fc = hp ? 20.0f * powf(150.0f, t) : 20000.0f * powf(300.0f / 20000.0f, -t);
    if (fc > 0.45f * e->sr) fc = 0.45f * e->sr;
    float g = tanf(PI_F * fc / e->sr), k = 1.4142f;
    float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
    float *ch[2] = {L, R};
    for (int c = 0; c < 2; c++) {
        float ic1 = e->svf_ic1[c], ic2 = e->svf_ic2[c];
        float *x = ch[c];
        for (int i = 0; i < n; i++) {
            float v0 = x[i];
            float v3 = v0 - ic2;
            float v1 = a1 * ic1 + a2 * v3;
            float v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
            x[i] = hp ? v0 - k * v1 - v2 : v2;
        }
        e->svf_ic1[c] = ic1 + 1e-18f; e->svf_ic2[c] = ic2 + 1e-18f;
    }
}

static void fx_chain(GsEngine *e, float *L, float *R, int n)
{
    int order = ip(e, P_FX_ORDER);
    if (order < 0) order = 0;
    if (order > 23) order = 23;
    const float *p = e->p;
    for (int s = 0; s < 4; s++) {
        switch (GS_FX_ORDERS[order][s]) {
        case 0: if (ip(e, P_DRV_ON)) gs_drive_process(&e->drv, L, R, n, p[P_DRV_AMT], ip(e, P_DRV_TYPE), p[P_DRV_TONE],
                                                      p[P_DRV_MIX], ip(e, P_DRV_OS) ? 4 : 2, e->sr);
                break;
        case 1: if (!ip(e, P_CMP_ON)) { e->comp_gr = 0; break; }
                gs_comp_process(&e->cmp, L, R, n, p[P_CMP_THR], p[P_CMP_RATIO], p[P_CMP_ATK], p[P_CMP_REL],
                                p[P_CMP_MAKEUP], p[P_CMP_MIX], e->sr);
                e->comp_gr = e->cmp.gr_db; break;
        case 2: {
            if (!ip(e, P_DLY_ON)) break;
            int div = ip(e, P_DLY_DIV);
            if (div < 0 || div > 7) div = 0;
            float ms = div > 0 ? GS_DLY_DIV_BEATS[div] * 60000.0f / p[P_BPM] : p[P_DLY_TIME];
            if (ms > 2000.0f) ms = 2000.0f;
            gs_delay_process(&e->dly, L, R, n, ms, p[P_DLY_FB], p[P_DLY_DAMP], p[P_DLY_PING], p[P_DLY_MIX], e->sr);
            break; }
        case 3: if (ip(e, P_REV_ON)) gs_reverb_process(&e->rev, L, R, n, p[P_REV_SIZE], p[P_REV_DAMP], p[P_REV_WIDTH],
                                                       p[P_REV_PRE], p[P_REV_MOD], p[P_REV_MIX], e->sr);
                break;
        }
    }
}

/* dry audition of the cut (works while the transport is stopped) */
static void preview(GsEngine *e, float *L, float *R, int n)
{
    if (!e->preview_on || !e->src || e->src_len < 2) { e->ui_preview_pos = -1; return; }
    float tin = e->p[P_TRIM_IN], tout = e->p[P_TRIM_OUT];
    if (tout < tin) { float t = tin; tin = tout; tout = t; }
    int a = (int)((double)tin * e->src_len), b = (int)((double)tout * e->src_len);
    if (a < 0) a = 0;
    if (a > e->src_len - 1) a = e->src_len - 1;
    if (b > e->src_len) b = e->src_len;
    if (b <= a) b = a + 1;
    double p = e->preview_pos;
    if (p < a || p >= b) p = a;
    for (int i = 0; i < n; i++) {
        float s = e->src[(int)p] * 0.8f;
        L[i] += s; R[i] += s;
        p += 1.0;
        if (p >= b) {
            p = a;
            if (!e->preview_loop) { e->preview_on = 0; break; }
        }
    }
    e->preview_pos = p;
    e->ui_preview_pos = e->preview_on ? (float)(p / e->src_len) : -1;
}

static void transport_stop_now(GsEngine *e)
{
    e->ngrains = 0;
    e->spawn_t = 0;
    gs_img_stop(&e->img);
    gs_arp_reset(&e->arp);
    gs_fx_clear(&e->dly, &e->rev);
    memset(&e->drv, 0, sizeof(e->drv));
    memset(&e->cmp, 0, sizeof(e->cmp));
    e->svf_ic1[0] = e->svf_ic1[1] = e->svf_ic2[0] = e->svf_ic2[1] = 0;
    e->out_gain = 0;
    e->peak_l = e->peak_r = 0;
    memset(e->lim_l, 0, sizeof e->lim_l); memset(e->lim_r, 0, sizeof e->lim_r);
    e->lim_g = 1; e->lim_hold = 1; e->lim_hold_t = 0;
    e->ui_ngrains = 0;
}

void gs_engine_panic(GsEngine *e)
{
    transport_stop_now(e);
    e->run_state = 0;
}

/* master gain (one-pole toward target) + look-ahead peak limiter + safety clip */
static void master_out(GsEngine *e, const float *L, const float *R, float *outL, float *outR, int n, float target)
{
    const int la = e->lim_la;
    const float thr = 0.89f;                                   /* -1 dBFS ceiling */
    const float att = 1.0f - expf(-1.0f / fmaxf(1.0f, la * 0.2f));
    const float rel = 1.0f - expf(-1.0f / (0.150f * e->sr));   /* 150 ms */
    const float gk = 1.0f - expf(-1.0f / (0.005f * e->sr));    /* 5 ms gain glide */
    float pl = e->peak_l * expf(-(float)n / (0.05f * e->sr));
    float pr = e->peak_r * expf(-(float)n / (0.05f * e->sr));
    for (int i = 0; i < n; i++) {
        e->out_gain += (target - e->out_gain) * gk;
        float a = L[i] * e->out_gain, b = R[i] * e->out_gain;
        if (!(fabsf(a) <= 1e4f)) a = 0;   /* NaN / Inf guard */
        if (!(fabsf(b) <= 1e4f)) b = 0;
        /* look-ahead: decide the gain from the incoming sample, apply it to the delayed one */
        float pk = fmaxf(fabsf(a), fabsf(b));
        float need = pk > thr ? thr / pk : 1.0f;
        if (need < e->lim_hold) { e->lim_hold = need; e->lim_hold_t = la; }
        else if (e->lim_hold_t > 0) e->lim_hold_t--;
        else e->lim_hold = 1.0f;
        float tg = e->lim_hold;
        e->lim_g += (tg - e->lim_g) * (tg < e->lim_g ? att : rel);
        int p = e->lim_pos;
        float da = e->lim_l[p], db = e->lim_r[p];
        e->lim_l[p] = a; e->lim_r[p] = b;
        if (++p >= la) p = 0;
        e->lim_pos = p;
        a = da * e->lim_g; b = db * e->lim_g;
        /* safety: anything that still slips through is soft-clipped, never above 0 dBFS */
        if (fabsf(a) > 0.95f) a = (a > 0 ? 1 : -1) * (0.95f + 0.05f * tanhf((fabsf(a) - 0.95f) * 20.0f));
        if (fabsf(b) > 0.95f) b = (b > 0 ? 1 : -1) * (0.95f + 0.05f * tanhf((fabsf(b) - 0.95f) * 20.0f));
        if (a > 1.0f) a = 1.0f;
        if (a < -1.0f) a = -1.0f;
        if (b > 1.0f) b = 1.0f;
        if (b < -1.0f) b = -1.0f;
        outL[i] = a; outR[i] = b;
        float fa = fabsf(a), fb = fabsf(b);
        if (fa > pl) pl = fa;
        if (fb > pr) pr = fb;
    }
    e->peak_l = pl; e->peak_r = pr;
}

static void host_sync(GsEngine *e, int src_sel)
{
    double beat = 60.0 / e->p[P_BPM] * e->sr;
    double ppq = e->host_ppq, inbar = ppq - e->host_bar_ppq;
    if (inbar < 0) inbar = ppq;
    if (ip(e, P_ARP_ON)) gs_arp_sync(&e->arp, e->p, inbar, beat);

    double ib = synced_interval_beats(e);
    if (ib > 0 && ip(e, P_GRAIN_ON)) {
        double next = ceil(ppq / ib - 1e-9) * ib;
        double desired = (next - ppq) * beat, I = ib * beat;
        double d = fabs(e->spawn_t - desired);
        if (d > 1.0 && fabs(d - I) > 2.0) e->spawn_t = desired;
    }

    if (src_sel == 1 && ip(e, P_IMG_SYNC) && e->img.scrub < 0) {
        double sweep_beats = gs_img_sync_steps(e->p) * 0.25;
        double ph = fmod(ppq, sweep_beats) / sweep_beats;
        if (ph < 0) ph += 1.0;
        double diff = fabs(ph - e->img.phase);
        if (diff > 0.5) diff = 1.0 - diff;
        if (diff > 0.002) e->img.phase = ph;
    }
}

static void process_block(GsEngine *e, float *outL, float *outR, int n)
{
    float *L = e->bus_l, *R = e->bus_r;
    memset(L, 0, sizeof(float) * n);
    memset(R, 0, sizeof(float) * n);

    /* transport: STOP cuts everything (grains, arp, FX tails) after a short fade */
    int want = e->playing;
    if (!want && !e->run_state) {
        preview(e, L, R, n);
        master_out(e, L, R, outL, outR, n, 1.0f);
        e->ui_ngrains = 0;
        return;
    }
    if (want && !e->run_state) {
        e->run_state = 1;
        e->spawn_t = 0;
        e->out_gain = 0;
        memset(e->lim_l, 0, sizeof e->lim_l); memset(e->lim_r, 0, sizeof e->lim_r);
    }

    /* only one input at a time: the sample (granular) or the image sweeper */
    int src_sel = ip(e, P_SRC);
    if (src_sel != e->last_src) {
        /* grains and arp notes point into the old input's buffer: drop them on every switch */
        e->ngrains = 0;
        gs_arp_reset(&e->arp);
        if (src_sel == 0) gs_img_stop(&e->img);
        e->last_src = src_sel;
    }
    int arp_on = ip(e, P_ARP_ON);
    if (arp_on && !e->arp_was_on) gs_arp_reset(&e->arp);
    e->arp_was_on = arp_on;

    /* MIDI gate: a fresh note restarts free-running clocks so it speaks right away */
    if (e->gate && !e->gate_prev && !e->host_playing) {
        e->spawn_t = 0;
        e->arp.t_next = 0;
    }
    e->gate_prev = e->gate;

    if (e->host_playing) host_sync(e, src_sel);

    float bend = powf(2.0f, e->bend_semis / 12.0f);
    int grains_on = ip(e, P_GRAIN_ON);

    if (src_sel == 1) {
        /* IMAGE input: the sweeper plays into a rolling 6 s live buffer; grains and the
         * arpeggiator read from it (HOLD freezes it), DRY sets how much plain sweeper you hear */
        float *il = e->img_l, *ir = e->img_r;
        memset(il, 0, sizeof(float) * n); memset(ir, 0, sizeof(float) * n);
        int dm = e->degree_mask, d = 0;                     /* lowest held chord pad = scale-step shift */
        while (d < 7 && !((dm >> d) & 1)) d++;
        e->img.deg_shift = d < 7 ? d : 0;
        e->img.bend = bend;
        gs_img_process(&e->img, e->p, il, ir, n, e->sr);
        /* gate envelope (latch off + no notes = silent sweeper) */
        float gt = e->gate ? 1.0f : 0.0f;
        float gk = 1.0f - expf(-1.0f / ((e->gate ? 0.005f : 0.08f) * e->sr));
        for (int i = 0; i < n; i++) {
            e->gate_env += (gt - e->gate_env) * gk;
            il[i] *= e->gate_env; ir[i] *= e->gate_env;
        }
        int frozen = ip(e, P_IMG_HOLD);
        if (!frozen) {
            int bin_len = e->live_len / GS_LIVE_ENV;
            for (int i = 0; i < n; i++) {
                float v = 0.5f * (il[i] + ir[i]);
                int w = e->live_w;
                e->live[w] = v;
                int bin = w / bin_len; if (bin >= GS_LIVE_ENV) bin = GS_LIVE_ENV - 1;
                if (w % bin_len == 0) e->live_env[bin] = 0;
                if (fabsf(v) > e->live_env[bin]) e->live_env[bin] = fabsf(v);
                if (++w >= e->live_len) w = 0;
                e->live_w = w;
            }
        }
        e->live_mode = 1;
        e->gsrc = e->live; e->gsrc_len = e->live_len;
        update_voices(e, n);
        if (!ip(e, P_GRAIN_HOLD)) { e->playhead += e->p[P_SCAN] * n / (float)e->live_len; e->playhead -= floorf(e->playhead); }
        if (grains_on) { schedule(e, n); render_grains(e, L, R, n, bend); }
        else e->ngrains = 0;
        if (arp_on) {
            float gg = e->p[P_ARP_GRAINS];
            for (int i = 0; i < n; i++) { L[i] *= gg; R[i] *= gg; }
            int t = ip(e, P_KEY) - ip(e, P_IMG_KEY);
            t = ((t % 12) + 12) % 12; if (t > 6) t -= 12;
            gs_arp_process(&e->arp, e->p, e->live, 0, e->live_len, e->live_w, e->voice_tgt, e->nvoices,
                           (float)t + 12.0f * ip(e, P_OCTAVE), e->gate, bend, L, R, n, e->sr);
        }
        float dry = e->p[P_IMG_DRY];
        for (int i = 0; i < n; i++) { L[i] += il[i] * dry; R[i] += ir[i] * dry; }
        tone_filter(e, L, R, n);
    } else if (e->src && e->src_len >= 512) {
        e->live_mode = 0;
        e->gsrc = e->src; e->gsrc_len = e->src_len;
        update_region(e);
        update_voices(e, n);
        int rl = e->region_e - e->region_s;
        if (!ip(e, P_GRAIN_HOLD) && rl > 0) {
            e->playhead += e->p[P_SCAN] * n / (float)rl;
            e->playhead -= floorf(e->playhead);
        }
        if (grains_on) {
            schedule(e, n);
            render_grains(e, L, R, n, bend);
        } else e->ngrains = 0;                      /* grain engine switched off on the GRAIN tab */

        /* arpeggiator: plays the region as notes of the current chord, before tone / FX */
        if (arp_on) {
            float gg = e->p[P_ARP_GRAINS];
            for (int i = 0; i < n; i++) { L[i] *= gg; R[i] *= gg; }
            int t = ip(e, P_KEY) - ip(e, P_SAMPLE_ROOT);
            t = ((t % 12) + 12) % 12; if (t > 6) t -= 12;
            gs_arp_process(&e->arp, e->p, e->src, e->region_s, e->region_e, -1, e->voice_tgt, e->nvoices,
                           (float)t + 12.0f * ip(e, P_OCTAVE), e->gate, bend, L, R, n, e->sr);
        }
        tone_filter(e, L, R, n);
    } else {
        e->ngrains = 0;
    }

    fx_chain(e, L, R, n);
    preview(e, L, R, n);

    /* master: -40..+18 dB, then a look-ahead peak limiter so turning it up makes it louder, not crunchier */
    float mdb = e->p[P_MASTER];
    float target = (!want || mdb <= -39.5f) ? 0.0f : powf(10.0f, mdb * 0.05f);
    master_out(e, L, R, outL, outR, n, target);
    if (!want && e->out_gain < 0.001f) { e->run_state = 0; transport_stop_now(e); }

    /* visualisation snapshot */
    int nv = e->ngrains < GS_UI_GRAINS ? e->ngrains : GS_UI_GRAINS;
    float inv = e->gsrc_len > 0 ? 1.0f / e->gsrc_len : 0;
    for (int i = 0; i < nv; i++) {
        const GsGrain *g = &e->grains[i];
        e->ui_grain_pos[i] = (float)g->pos * inv;
        int wi = (int)((float)g->age * GS_WIN_N / (g->len > 0 ? g->len : 1));
        if (wi < 0) wi = 0;
        if (wi > GS_WIN_N) wi = GS_WIN_N;
        e->ui_grain_amp[i] = g->delay > 0 ? 0.0f : e->win[wi];
    }
    e->ui_ngrains = nv;
    e->ui_playhead = e->playhead;
}

void gs_engine_process(GsEngine *e, float *out_l, float *out_r, int n)
{
    double beat = 60.0 / (e->p[P_BPM] > 1 ? e->p[P_BPM] : 120) * e->sr;
    while (n > 0) {
        int c = n > GS_BLOCK_MAX ? GS_BLOCK_MAX : n;
        process_block(e, out_l, out_r, c);
        if (e->host_ppq >= 0) e->host_ppq += c / beat;
        out_l += c; out_r += c; n -= c;
    }
}
