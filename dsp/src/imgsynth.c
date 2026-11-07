#include "imgsynth.h"
#include "params.h"
#include "theory.h"
#include <math.h>
#include <string.h>

const char *const GS_IMG_SCALE_NAMES[] = {
    "MIN PENT", "MAJ PENT", "MAJOR", "MINOR", "DORIAN", "BLUES", "HIRAJOSHI", "CHROMATIC"
};
static const int IMG_SCALES[GS_IMG_NUM_SCALES][12] = {
    {0, 3, 5, 7, 10, -1},
    {0, 2, 4, 7, 9, -1},
    {0, 2, 4, 5, 7, 9, 11, -1},
    {0, 2, 3, 5, 7, 8, 10, -1},
    {0, 2, 3, 5, 7, 9, 10, -1},
    {0, 3, 5, 6, 7, 10, -1},
    {0, 2, 3, 7, 8, -1},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11},
};
const int GS_IMG_GRID_STEPS[8] = {0, 8, 12, 16, 24, 32, 48, 64};

#define CHUNK 32
#define GATE 0.06f

static inline int ip(const float *p, int id) { return (int)lroundf(p[id]); }
static inline float frnd(GsImgSynth *s) { s->rng = s->rng * 1664525u + 1013904223u; return (s->rng >> 8) * (2.0f / 16777216.0f) - 1.0f; }

void gs_img_init(GsImgSynth *s)
{
    memset(s, 0, sizeof(*s));
    for (int i = 0; i <= 1024; i++) s->sine[i] = sinf(2.0f * 3.14159265f * i / 1024.0f);
    s->rng = 0xBADC0DEu;
    s->scrub = -1;
    s->last_step = -1;
    s->ui_mono_y = -1;
    s->bend = 1.0f;
}

void gs_img_stop(GsImgSynth *s)
{
    for (int i = 0; i < IMG_MAX_NOTES; i++) { s->v[i].env = 0; s->v[i].gate = 0; s->v[i].target = 0; s->v[i].ic1 = s->v[i].ic2 = 0; }
    s->mono.env = 0; s->mono.gate = 0; s->mono.target = 0; s->mono.ic1 = s->mono.ic2 = 0;
    s->last_step = -1;
}

int gs_img_build_notes(int key, int scale, int low, int range, int *out, int max)
{
    if (scale < 0 || scale >= GS_IMG_NUM_SCALES) scale = 0;
    int n = 0;
    for (int m = low; m <= low + 12 * range && n < max; m++) {
        int pc = ((m - key) % 12 + 12) % 12;
        for (int k = 0; k < 12 && IMG_SCALES[scale][k] >= 0; k++)
            if (IMG_SCALES[scale][k] == pc) { out[n++] = m; break; }
    }
    return n;
}

static float mtof(float m) { return 440.0f * powf(2.0f, (m - 69.0f) / 12.0f); }

static void voice_set_freq(GsImgVoice *v, float f, int sr)
{
    v->freq = f;
    v->inc = f / sr;
    float g = tanf(3.14159265f * fminf(f, sr * 0.45f) / sr), k = 1.0f / 18.0f;   /* Q = 18 */
    v->g = g; v->a1 = 1.0f / (1.0f + g * (g + k)); v->a2 = g * v->a1; v->a3 = g * v->a2;
    float bw = f / 18.0f;
    v->ngain = fminf(40.0f, sqrtf(sr * 0.5f / bw) * 0.35f);
}

/* intensity of one band of rows over columns [c0, c1) */
static float band_level(GsImgSynth *s, int r0, int r1, int c0, int c1, int dark)
{
    float sum = 0; int cnt = 0;
    for (int r = r0; r < r1; r++)
        for (int c = c0; c < c1; c++) { sum += s->grid[r][c]; cnt++; }
    if (!cnt) return 0;
    float v = sum / cnt;
    if (dark) v = 1.0f - v;
    v = (v - GATE) / (1.0f - GATE);
    return v < 0 ? 0 : v > 1 ? 1 : v;
}

static void cols_for(double phase, int steps, int *c0, int *c1)
{
    if (steps <= 0) {
        int c = (int)(phase * IMG_COLS); if (c >= IMG_COLS) c = IMG_COLS - 1;
        *c0 = c; *c1 = c + 1;
    } else {
        int st = (int)(phase * steps); if (st >= steps) st = steps - 1;
        *c0 = st * IMG_COLS / steps; *c1 = (st + 1) * IMG_COLS / steps;
        if (*c1 <= *c0) *c1 = *c0 + 1;
    }
}

static void trigger(GsImgVoice *v, float amp, float step_samples, float decay, int sr)
{
    v->target = amp;
    v->gate = 1;
    v->att_left = 0.004f * sr;
    float len = step_samples * (0.08f + 0.92f * decay);
    float tau = decay > 0.98f ? 1e9f : len / 4.5f;              /* decay knob at max = hold the whole step */
    v->dec = expf(-1.0f / fmaxf(tau, 1.0f));
}

static int scale_len(int scale)
{
    if (scale < 0 || scale >= GS_IMG_NUM_SCALES) scale = 0;
    int n = 0;
    while (n < 12 && IMG_SCALES[scale][n] >= 0) n++;
    return n;
}

/* PLAY-tab interval / chord strip applied to the image: scale steps to add on top of each note */
static int harmony_steps(const float *p, int *out)
{
    int ch = ip(p, P_CHORD);
    if (ch < 0) ch = 0;
    if (ch >= GS_NUM_CHORDS) ch = GS_NUM_CHORDS - 1;
    int len = scale_len(ip(p, P_IMG_SCALE));
    int open = p[P_VOICING] > 0.5f;
    const GsChordType *c = &GS_CHORDS[ch];
    for (int i = 0; i < c->n; i++) {
        int st = c->steps[i] == GS_OCT ? len : c->steps[i];
        if (open && i > 0 && (i & 1)) st += len;          /* open voicing: lift every other tone an octave */
        out[i] = st;
    }
    return c->n;
}

/* turn wanted amplitudes per note into voice gates / triggers */
static void apply_want(GsImgSynth *s, const float *want, int steps, int new_step, float step_samples, float decay, int sr)
{
    for (int b = 0; b < s->nnext; b++) {
        GsImgVoice *v = &s->v[b];
        int on = want[b] > 0;
        if (steps > 0) {
            if (new_step) {
                if (on) trigger(v, want[b], step_samples, decay, sr);
                else { v->gate = 0; v->target = 0; }
            }
        } else {
            v->gate = on;
            v->target = on ? want[b] : 0;
            v->slew = 1.0f - expf(-1.0f / (0.03f * sr));        /* held, sliding tones */
        }
        v->pan = ((b % 5) - 2) * 0.18f;
    }
}

/* spectral analysis + voice targets for the current position */
static void spectral_update(GsImgSynth *s, const float *p, int steps, int new_step, float step_samples, int sr)
{
    int N = s->nnotes, V = ip(p, P_IMG_VOICES), dark = ip(p, P_IMG_READ);
    int c0, c1; cols_for(s->phase, steps, &c0, &c1);
    float lvl[IMG_MAX_NOTES];
    if (ip(p, P_IMG_MODE) == 2) {
        /* HEIGHT: every edge / object the line crosses plays a note at its height
         * (top of the picture = high note, bottom = low note), whatever its brightness */
        float prof[IMG_ROWS], e[IMG_ROWS];
        for (int r = 0; r < IMG_ROWS; r++) {
            float sum = 0;
            for (int c = c0; c < c1; c++) sum += s->grid[r][c];
            prof[r] = sum / (c1 - c0);
        }
        for (int r = 0; r < IMG_ROWS; r++) {
            float up = prof[r > 0 ? r - 1 : 0], dn = prof[r < IMG_ROWS - 1 ? r + 1 : IMG_ROWS - 1];
            e[r] = fabsf(dn - up) * 3.0f;
        }
        for (int b = 0; b < N; b++) lvl[b] = 0;
        for (int r = 2; r < IMG_ROWS - 2; r++) {
            float v = e[r];
            if (v < 0.12f || v < e[r - 1] || v < e[r + 1] || v < e[r - 2] || v < e[r + 2]) continue;   /* local peaks only */
            int b = (int)lroundf((1.0f - (r + 0.5f) / IMG_ROWS) * (N - 1));
            if (b < 0) b = 0;
            if (b >= N) b = N - 1;
            if (v > 1) v = 1;
            if (v > lvl[b]) lvl[b] = v;
        }
    } else {
        for (int b = 0; b < N; b++) {
            int r1 = IMG_ROWS - b * IMG_ROWS / N, r0 = IMG_ROWS - (b + 1) * IMG_ROWS / N;
            if (r1 <= r0) r1 = r0 + 1;
            lvl[b] = band_level(s, r0, r1, c0, c1, dark);
        }
    }
    /* keep only the loudest V notes */
    int pick[8], np = 0;
    for (int k = 0; k < V && k < 8; k++) {
        int best = -1; float bv = 0.02f;
        for (int b = 0; b < N; b++) {
            int used = 0;
            for (int j = 0; j < np; j++) if (pick[j] == b) used = 1;
            if (!used && lvl[b] > bv) { bv = lvl[b]; best = b; }
        }
        if (best < 0) break;
        pick[np++] = best;
    }
    /* chord pad moves each note along the scale; the interval strip stacks harmony on top */
    float want[IMG_MAX_NOTES];
    for (int b = 0; b < s->nnext; b++) want[b] = 0;
    int hs[8], nh = harmony_steps(p, hs);
    for (int j = 0; j < np; j++) {
        int root = pick[j] + s->deg_shift;
        for (int h = 0; h < nh; h++) {
            int idx = root + hs[h];
            if (idx < 0 || idx >= s->nnext) continue;
            float a = lvl[pick[j]] * (h == 0 ? 1.0f : 0.8f);
            if (a > want[idx]) want[idx] = a;
        }
    }
    apply_want(s, want, steps, new_step, step_samples, p[P_IMG_DECAY], sr);
}

static void brightness_update(GsImgSynth *s, const float *p, int steps, int new_step, float step_samples, int sr)
{
    int N = s->nnotes, dark = ip(p, P_IMG_READ);
    int c0, c1; cols_for(s->phase, steps, &c0, &c1);
    float wsum = 0, ysum = 0, peak = 0;
    for (int r = 0; r < IMG_ROWS; r++) {
        float sum = 0;
        for (int c = c0; c < c1; c++) sum += s->grid[r][c];
        float v = sum / (c1 - c0);
        if (dark) v = 1.0f - v;
        v = (v - GATE) / (1.0f - GATE);
        if (v <= 0) continue;
        v = v * v;                                   /* favour the brightest rows */
        float y = 1.0f - (r + 0.5f) / IMG_ROWS;      /* 0 bottom .. 1 top */
        wsum += v; ysum += v * y;
        if (v > peak) peak = v;
    }
    GsImgVoice *v = &s->mono;
    float want[IMG_MAX_NOTES];
    for (int b = 0; b < s->nnext; b++) want[b] = 0;
    if (wsum < 0.02f) {
        if (steps > 0 && new_step) { v->gate = 0; v->target = 0; }
        if (steps == 0) { v->gate = 0; v->target = 0; }
        s->ui_mono_y = -1;
        apply_want(s, want, steps, new_step, step_samples, p[P_IMG_DECAY], sr);
        return;
    }
    float y = ysum / wsum;
    int idx = (int)lroundf(y * (N - 1)) + s->deg_shift;     /* chord pad moves the melody along the scale */
    if (idx < 0) idx = 0;
    if (idx >= s->nnext) idx = s->nnext - 1;
    float f = mtof((float)s->notes[idx]);
    /* interval strip: harmony voices on top of the melody */
    int hs[8], nh = harmony_steps(p, hs);
    for (int h = 1; h < nh; h++) {
        int k = idx + hs[h];
        if (k >= 0 && k < s->nnext) want[k] = sqrtf(peak) * 0.8f;
    }
    apply_want(s, want, steps, new_step, step_samples, p[P_IMG_DECAY], sr);
    float amp = sqrtf(peak);
    s->ui_mono_y = y;
    s->mono_note = idx;
    v->pan = 0;
    if (steps > 0) {
        if (new_step) { voice_set_freq(v, f, sr); v->freq_target = f; trigger(v, amp, step_samples, p[P_IMG_DECAY], sr); }
    } else {
        if (v->freq == 0) voice_set_freq(v, f, sr);
        v->freq_target = f;                               /* glide toward it */
        v->gate = 1; v->target = amp;
        v->slew = 1.0f - expf(-1.0f / (0.03f * sr));
    }
}

static inline float polyblep(float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

static inline float osc(GsImgSynth *s, GsImgVoice *v, int wave)
{
    float ph = v->ph, dt = v->inc * s->bend, y;
    if (dt > 0.45f) dt = 0.45f;
    switch (wave) {
    case 0: { float x = ph * 1024.0f; int i = (int)x; y = s->sine[i] + (s->sine[i + 1] - s->sine[i]) * (x - i); break; }
    case 1: y = 4.0f * fabsf(ph - 0.5f) - 1.0f; break;
    case 2: y = (2.0f * ph - 1.0f) - polyblep(ph, dt); y *= 0.7f; break;
    default: {
        y = ph < 0.5f ? 1.0f : -1.0f;
        y += polyblep(ph, dt);
        float t2 = ph + 0.5f; if (t2 >= 1.0f) t2 -= 1.0f;
        y -= polyblep(t2, dt);
        y *= 0.55f; break; }
    }
    ph += dt; if (ph >= 1.0f) ph -= floorf(ph);
    v->ph = ph;
    return y;
}

static inline float noise_band(GsImgVoice *v, float x)
{
    float v3 = x - v->ic2;
    float v1 = v->a1 * v->ic1 + v->a2 * v3;
    float v2 = v->ic2 + v->a2 * v->ic1 + v->a3 * v3;
    v->ic1 = 2 * v1 - v->ic1 + 1e-18f; v->ic2 = 2 * v2 - v->ic2 + 1e-18f;
    return v1 * (1.0f / 18.0f) * v->ngain;
}

static void render_voice(GsImgSynth *s, GsImgVoice *v, float *L, float *R, int n, int wave, int src,
                         float gain, float sweep_pan, int smooth, int sr)
{
    if (v->env < 1e-5f && v->target <= 0 && !v->gate) { v->env = 0; return; }
    float pan = v->pan + sweep_pan;
    if (pan < -1) pan = -1;
    if (pan > 1) pan = 1;
    float gl = sqrtf(0.5f * (1 - pan)) * gain, gr = sqrtf(0.5f * (1 + pan)) * gain;
    float on = src != 0 ? 1.0f : 0.0f, nz = src != 1 ? 1.0f : 0.0f;
    if (src == 2) { on = 0.7f; nz = 0.6f; }
    for (int i = 0; i < n; i++) {
        if (smooth) {
            v->env += (v->target - v->env) * v->slew;
        } else if (v->att_left > 0) {
            v->env += (v->target - v->env) / v->att_left;
            v->att_left -= 1;
        } else if (v->gate) {
            v->env *= v->dec;
        } else {
            v->env *= s->rel_mul;                          /* quick release */
        }
        float x = 0;
        if (on > 0) x += osc(s, v, wave) * on;
        else { v->ph += v->inc * s->bend; if (v->ph >= 1) v->ph -= floorf(v->ph); }
        if (nz > 0) x += noise_band(v, frnd(s)) * nz;
        x *= v->env;
        L[i] += x * gl;
        R[i] += x * gr;
    }
}

void gs_img_set_grid(GsImgSynth *s, const float *grid)
{
    memcpy(s->grid, grid, sizeof(s->grid));
    s->has_image = 1;
    s->last_step = -1;
    s->ctl_left = 0;
}

int gs_img_sync_steps(const float *p)
{
    int steps = GS_IMG_GRID_STEPS[ip(p, P_IMG_GRID)];
    return steps > 0 ? steps : 16;
}

int gs_img_active_notes(const GsImgSynth *s, int *out, int max)
{
    int n = 0;
    for (int b = 0; b < s->nnext && n < max; b++)
        if (s->v[b].target > 0 && (s->v[b].gate || s->v[b].env > 1e-4f)) out[n++] = s->notes[b];
    if (s->mono.target > 0 && s->mono.gate && n < max) {
        int dup = 0;
        for (int i = 0; i < n; i++) if (out[i] == s->notes[s->mono_note]) dup = 1;
        if (!dup) out[n++] = s->notes[s->mono_note];
    }
    return n;
}

void gs_img_process(GsImgSynth *s, const float *p, float *L, float *R, int n, int sr)
{
    if (!s->has_image) return;
    s->rel_mul = powf(0.9985f, 48000.0f / (float)sr);

    /* notes for the current key / scale / range (rebuild if changed) */
    int nn[IMG_MAX_NOTES];
    int key = ip(p, P_IMG_KEY), scl = ip(p, P_IMG_SCALE), low = ip(p, P_IMG_LOW), rng = ip(p, P_IMG_RANGE);
    int N = gs_img_build_notes(key, scl, low, rng, nn, IMG_MAX_NOTES);
    if (N < 1) return;
    /* the same scale continues 3 more octaves (capped below 10 kHz) so harmony can sit above the top note */
    int Ne = gs_img_build_notes(key, scl, low, rng + 3, nn, IMG_MAX_NOTES);
    while (Ne > N && nn[Ne - 1] > 123) Ne--;
    if (N != s->nnotes || Ne != s->nnext || memcmp(nn, s->notes, sizeof(int) * Ne)) {
        for (int i = 0; i < Ne; i++) { s->notes[i] = nn[i]; voice_set_freq(&s->v[i], mtof((float)nn[i]), sr); }
        for (int i = Ne; i < IMG_MAX_NOTES; i++) { s->v[i].gate = 0; s->v[i].target = 0; s->v[i].env = 0; }
        s->nnotes = N;
        s->nnext = Ne;
    }

    int steps = GS_IMG_GRID_STEPS[ip(p, P_IMG_GRID)];
    float sweep = p[P_IMG_SWEEP];
    if (ip(p, P_IMG_SYNC)) sweep = (steps > 0 ? steps : 16) * 15.0f / p[P_BPM];   /* one step = one 16th note */
    if (sweep < 0.25f) sweep = 0.25f;
    double inc = 1.0 / (sweep * sr);
    float step_samples = steps > 0 ? sweep * sr / steps : sweep * sr / 16;
    int mode = ip(p, P_IMG_MODE), wave = ip(p, P_IMG_WAVE), src = ip(p, P_IMG_SOURCE);
    int V = mode != 1 ? ip(p, P_IMG_VOICES) : 1;
    int hs_dummy[8], nh = harmony_steps(p, hs_dummy);
    float gain = p[P_IMG_LEVEL] * 1.8f / sqrtf((float)(V > 1 ? V : 1)) / sqrtf((float)nh);

    for (int off = 0; off < n;) {
        if (s->ctl_left <= 0) {                     /* control rate: every CHUNK samples, whatever the block size */
            if (s->scrub >= 0) s->phase = s->scrub;
            int step = steps > 0 ? (int)(s->phase * steps) : -2;
            int new_step = step != s->last_step;
            s->last_step = step;
            if (mode != 1) spectral_update(s, p, steps, new_step, step_samples, sr);
            else brightness_update(s, p, steps, new_step, step_samples, sr);
            GsImgVoice *mv = &s->mono;
            if (mode == 1 && steps == 0 && mv->freq_target > 0 && fabsf(mv->freq - mv->freq_target) > 0.01f)
                voice_set_freq(mv, mv->freq + (mv->freq_target - mv->freq) * 0.12f, sr);   /* portamento */
            s->ctl_left = CHUNK * (sr > 64000 ? (sr > 128000 ? 4 : 2) : 1);
        }
        int c = n - off < s->ctl_left ? n - off : s->ctl_left;
        s->ctl_left -= c;

        float sweep_pan = ((float)s->phase - 0.5f) * 0.6f;   /* the sound travels left -> right with the line */
        for (int b = 0; b < s->nnext; b++)              /* note voices (spectral / height) + harmony voices */
            render_voice(s, &s->v[b], L + off, R + off, c, wave, src, gain, sweep_pan, steps == 0, sr);
        if (mode == 1) {
            GsImgVoice *v = &s->mono;
            render_voice(s, v, L + off, R + off, c, wave, src, gain, sweep_pan, steps == 0, sr);
        }
        if (s->scrub < 0) {
            s->phase += inc * c;
            if (s->phase >= 1.0) s->phase -= floor(s->phase);
        }
        off += c;
    }
    s->ui_phase = (float)s->phase;
    s->ui_nnotes = s->nnotes;
    for (int b = 0; b < s->nnotes; b++) s->ui_amp[b] = mode != 1 ? s->v[b].env : (b == s->mono_note ? s->mono.env : 0);
}
