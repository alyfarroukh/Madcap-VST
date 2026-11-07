#include "arp.h"
#include "params.h"
#include <math.h>
#include <string.h>

const char *const GS_ARP_MODE_NAMES[] = {"UP", "DOWN", "UP/DN", "RAND", "STAB"};
const char *const GS_ARP_RATE_NAMES[] = {"1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32"};
static const float RATE_BEATS[6] = {1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f};

static inline int ip(const float *p, int id) { return (int)lroundf(p[id]); }

void gs_arp_reset(GsArp *a)
{
    memset(a->v, 0, sizeof a->v);
    a->t_next = 0;
    a->step = 0;
    a->idx = -1;
    a->dir = 1;
    a->walk = 0;
    if (!a->rng) a->rng = 0x2468ACEu;
}

static float frand(GsArp *a) { a->rng = a->rng * 1664525u + 1013904223u; return (a->rng >> 8) * (1.0f / 16777216.0f); }

static void start_voice(GsArp *a, const float *p, int s, int e, int live_head, float semi, float pan, float gate_samples, int sr)
{
    /* take a free voice, or steal the quietest one */
    GsArpVoice *v = NULL;
    for (int i = 0; i < ARP_VOICES; i++) if (!a->v[i].active) { v = &a->v[i]; break; }
    if (!v) {
        float lo = 2; for (int i = 0; i < ARP_VOICES; i++) if (a->v[i].env < lo) { lo = a->v[i].env; v = &a->v[i]; }
    }
    int len = e - s;
    float st = p[P_ARP_START] + a->walk;
    st -= floorf(st);
    v->active = 1;
    v->s = s; v->e = e;
    if (live_head >= 0) {
        /* live buffer: START = how far back from "now" (0 = just played), leave room ahead of the note */
        float rate = powf(2.0f, semi / 12.0f);
        float need = gate_samples * rate + 512;
        if (need > len * 0.6f) need = len * 0.6f;
        double p0 = (double)live_head - need - st * (len - need - 512);
        while (p0 < s) p0 += len;
        v->pos = p0;
    } else {
        v->pos = s + (double)st * len;
    }
    if (v->pos >= e || v->pos < s) v->pos = s;      /* float rounding guard: never start outside [s, e) */
    v->rate = powf(2.0f, semi / 12.0f);
    v->env = 0;
    v->att_inc = 1.0f / fmaxf(1.0f, p[P_ARP_ATTACK] * 0.001f * sr);
    v->rel_mul = expf(-6.9f / fmaxf(1.0f, p[P_ARP_RELEASE] * 0.001f * sr));   /* -60 dB over the release time */
    v->gate_left = (int)gate_samples;
    v->releasing = 0;
    v->pan = pan;
}

float gs_arp_step_beats(int rate)
{
    if (rate < 0) rate = 0;
    if (rate > 5) rate = 5;
    return RATE_BEATS[rate];
}

void gs_arp_sync(GsArp *a, const float *p, double ppq, double beat_samples)
{
    if (beat_samples <= 1.0 || ppq < 0) return;
    double L = gs_arp_step_beats(ip(p, P_ARP_RATE));
    double sw = p[P_ARP_SWING] * 0.5;
    double u = ppq / L;                              /* position in steps since the bar start */
    long pair = (long)floor(u * 0.5);
    double within = u - 2.0 * pair;                  /* 0..2 inside a swung pair */
    long next; double until;                         /* index of the next step and beats until it */
    if (within < 1.0 + sw) { next = 2 * pair + 1; until = (1.0 + sw - within) * L; }
    else { next = 2 * pair + 2; until = (2.0 - within) * L; }
    double t = until * beat_samples;
    int want = (int)(next % 16384);
    if (a->step == want) { a->t_next = t; return; }                 /* in step: trim drift */
    if (a->step == (want + 1) % 16384 && t < 64) return;             /* we fired a hair early */
    if ((a->step + 1) % 16384 == want && a->t_next < 64) { a->t_next = 0; return; } /* a hair late */
    a->step = want;                                                  /* jump / loop / start: resync */
    a->t_next = t;
}

void gs_arp_process(GsArp *a, const float *p, const float *src, int s, int e, int live_head,
                    const float *chord, int nchord, float transpose, int allow_new, float bend,
                    float *L, float *R, int n, int sr)
{
    if (!src || e - s < 64 || nchord < 1) return;

    /* note list: the chord spread over N octaves, low -> high */
    int oct = ip(p, P_ARP_OCT), N = 0;
    float notes[ARP_MAX_NOTES];
    for (int o = 0; o < oct; o++)
        for (int i = 0; i < nchord && N < ARP_MAX_NOTES; i++) notes[N++] = chord[i] + 12.0f * o;
    for (int i = 1; i < N; i++) {                       /* sort (voicing can interleave octaves) */
        float x = notes[i]; int j = i - 1;
        while (j >= 0 && notes[j] > x) { notes[j + 1] = notes[j]; j--; }
        notes[j + 1] = x;
    }
    a->ui_nnotes = N;
    for (int i = 0; i < N; i++) a->ui_notes[i] = notes[i];
    if (a->idx >= N) a->idx = N - 1;

    float beat = 60.0f / p[P_BPM] * sr;
    float step_len = beat * RATE_BEATS[ip(p, P_ARP_RATE)];
    float swing = p[P_ARP_SWING] * 0.5f;
    int mode = ip(p, P_ARP_MODE), len = ip(p, P_ARP_LEN);
    unsigned pattern = (unsigned)lroundf(p[P_ARP_PATTERN]);
    float level = p[P_ARP_LEVEL] * 0.75f;

    /* step scheduling (sample accurate within the block) */
    int off = 0;
    while (off < n) {
        int until = (int)ceil(a->t_next);
        if (until < 0) until = 0;
        int chunk = until < n - off ? until : n - off;

        /* render voices for this chunk */
        for (int vi = 0; vi < ARP_VOICES; vi++) {
            GsArpVoice *v = &a->v[vi];
            if (!v->active) continue;
            float gl = sqrtf(0.5f * (1 - v->pan)) * level, gr = sqrtf(0.5f * (1 + v->pan)) * level;
            int rl = v->e - v->s;
            for (int i = off; i < off + chunk; i++) {
                if (!v->releasing) {
                    v->env += v->att_inc; if (v->env > 1) v->env = 1;
                    if (--v->gate_left <= 0) v->releasing = 1;
                } else {
                    v->env *= v->rel_mul;
                    if (v->env < 1e-4f) { v->active = 0; break; }
                }
                int i0 = (int)v->pos; float fr = (float)(v->pos - i0);
                int i1 = i0 + 1; if (i1 >= v->e) i1 = v->s;
                float x = (src[i0] + (src[i1] - src[i0]) * fr) * v->env;
                L[i] += x * gl; R[i] += x * gr;
                v->pos += v->rate * bend;
                if (v->pos >= v->e) v->pos -= rl;
                if (v->pos >= v->e || v->pos < v->s) v->pos = v->s;           /* short slices loop while the note is held */
            }
        }
        off += chunk;
        a->t_next -= chunk;
        if (off >= n && a->t_next > 0) break;

        if (a->t_next <= 0) {
            /* a new step */
            int st = a->step % (len < 1 ? 1 : len);
            a->ui_step = st;
            int on = (pattern >> st) & 1;
            float dur = step_len * ((a->step & 1) ? (1.0f - swing) : (1.0f + swing));
            float gate = dur * p[P_ARP_GATE];
            if (on && N > 0 && allow_new) {
                if (mode == 4) {                           /* STAB: the whole chord at once */
                    for (int i = 0; i < nchord && i < ARP_VOICES; i++)
                        start_voice(a, p, s, e, live_head, chord[i] + transpose, ((i % 3) - 1) * 0.35f, gate, sr);
                    a->idx = 0;
                } else {
                    if (mode == 0) a->idx = (a->idx + 1) % N;
                    else if (mode == 1) a->idx = a->idx <= 0 ? N - 1 : a->idx - 1;
                    else if (mode == 2) {
                        if (N == 1) a->idx = 0;
                        else {
                            a->idx += a->dir;
                            if (a->idx >= N) { a->idx = N - 2; a->dir = -1; }
                            if (a->idx < 0) { a->idx = 1; a->dir = 1; }
                        }
                    } else a->idx = (int)(frand(a) * N) % N;
                    start_voice(a, p, s, e, live_head, notes[a->idx] + transpose, ((a->idx % 3) - 1) * 0.3f, gate, sr);
                }
                a->walk += p[P_ARP_WALK];
                a->walk -= floorf(a->walk);
                a->ui_idx = a->idx;
                a->ui_trig++;
            }
            a->step = (a->step + 1) % 16384;
            a->t_next += dur;
        }
    }
}
