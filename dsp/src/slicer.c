#include "slicer.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define HOP 512

void gs_slicer_analyse(GsSlicer *s, const float *src, int len, int sr)
{
    s->len = len;
    s->n_onsets = 0;
    int frames = len / HOP;
    if (frames < 4) return;
    float *flux = malloc(sizeof(float) * frames);
    if (!flux) return;

    /* log-energy rise per hop (cheap onset detection function) */
    float prev = -100.0f;
    for (int f = 0; f < frames; f++) {
        double e = 0;
        const float *p = src + f * HOP;
        for (int i = 0; i < HOP; i++) e += p[i] * p[i];
        float le = 10.0f * log10f((float)(e / HOP) + 1e-10f);
        float d = le - prev;
        flux[f] = d > 0 ? d : 0;
        prev = le;
    }
    flux[0] = 0;

    int min_gap = (int)(0.08f * sr / HOP) + 1;
    int last = -min_gap;
    float strength[GS_MAX_ONSETS];
    for (int f = 2; f < frames - 1 && s->n_onsets < GS_MAX_ONSETS; f++) {
        /* adaptive threshold: local mean over ~0.25 s window */
        int w = (int)(0.125f * sr / HOP) + 1, a = f - w < 0 ? 0 : f - w, b = f + w >= frames ? frames - 1 : f + w;
        float m = 0;
        for (int k = a; k <= b; k++) m += flux[k];
        m /= (b - a + 1);
        if (flux[f] > 3.0f && flux[f] > m * 2.0f && flux[f] >= flux[f - 1] && flux[f] >= flux[f + 1] && f - last >= min_gap) {
            s->onset_pos[s->n_onsets] = f * HOP;
            strength[s->n_onsets] = flux[f];
            s->n_onsets++;
            last = f;
        }
    }
    free(flux);

    /* sort by strength desc */
    for (int i = 1; i < s->n_onsets; i++) {
        float sv = strength[i]; int pv = s->onset_pos[i]; int j = i - 1;
        while (j >= 0 && strength[j] < sv) { strength[j + 1] = strength[j]; s->onset_pos[j + 1] = s->onset_pos[j]; j--; }
        strength[j + 1] = sv; s->onset_pos[j + 1] = pv;
    }
}

static int cmp_int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; }

void gs_slicer_bounds(const GsSlicer *s, int count, int mode, int start, int end, int *bounds)
{
    if (count < 1) count = 1;
    if (count > GS_MAX_SLICES) count = GS_MAX_SLICES;
    if (start < 0) start = 0;
    if (end <= start) end = start + 1;
    int len = end - start;
    if (mode == 0 || s->n_onsets == 0 || count == 1) {
        for (int i = 0; i <= count; i++) bounds[i] = start + (int)((long long)len * i / count);
        return;
    }
    int tmp[GS_MAX_SLICES + 1];
    int n = 0;
    tmp[n++] = start;
    int edge = len / 100;
    for (int i = 0; i < s->n_onsets && n < count; i++) {
        int p = s->onset_pos[i];
        if (p < start + edge || p > end - edge) continue;   /* only hits inside the cut */
        tmp[n++] = p;
    }
    qsort(tmp, n, sizeof(int), cmp_int);
    /* not enough transients: split the longest slices in half */
    while (n < count) {
        int bi = 0, bl = -1;
        for (int i = 0; i < n; i++) {
            int e = (i + 1 < n) ? tmp[i + 1] : end;
            if (e - tmp[i] > bl) { bl = e - tmp[i]; bi = i; }
        }
        int e = (bi + 1 < n) ? tmp[bi + 1] : end;
        memmove(tmp + bi + 2, tmp + bi + 1, sizeof(int) * (n - bi - 1));
        tmp[bi + 1] = (tmp[bi] + e) / 2;
        n++;
    }
    for (int i = 0; i < count; i++) bounds[i] = tmp[i];
    bounds[count] = end;
}
