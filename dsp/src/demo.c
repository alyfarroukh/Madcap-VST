/* Built-in demo "voice": a sung C3 vowel that morphs a -> o -> e -> a, so the
 * app makes sound before you have copied any samples to the Vita. */
#include "demo.h"
#include <math.h>
#include <stdlib.h>

int gs_demo_voice(float **out, int sr, float seconds)
{
    int n = (int)(sr * seconds);
    float *b = malloc(sizeof(float) * n);
    if (!b) { *out = 0; return -1; }
    static const float V[4][3] = {{730, 1090, 2440}, {570, 840, 2410}, {530, 1840, 2480}, {730, 1090, 2440}};
    const float BW[3] = {90, 110, 160};
    float st[3][2] = {{0}};
    double ph = 0;
    unsigned rng = 0x5EED1234u;
    float peak = 0;
    for (int i = 0; i < n; i++) {
        double t = (double)i / sr;
        double f0 = 130.81 * (1 + 0.012 * sin(2 * 3.14159265 * 5.2 * t) * fmin(1.0, t));
        ph += f0 / sr; if (ph >= 1) ph -= 1;
        float x = (float)(ph * 2 - 1) + ((float)((rng = rng * 1664525u + 1013904223u) >> 22) / 1023.0f - 0.5f) * 0.04f; /* saw + breath */
        float m = (float)(t / seconds) * 3.0f; int vi = (int)m; if (vi > 2) vi = 2; float fr = m - vi;
        float y = 0;
        for (int k = 0; k < 3; k++) {
            float F = V[vi][k] + (V[vi + 1][k] - V[vi][k]) * fr;
            float rr = expf(-3.14159265f * BW[k] / sr), th = 2 * 3.14159265f * F / sr;
            float o = x * (1 - rr) + 2 * rr * cosf(th) * st[k][0] - rr * rr * st[k][1];
            st[k][1] = st[k][0]; st[k][0] = o;
            y += o / (k + 1);
        }
        float env = fminf(1.0f, (float)t * 8) * fminf(1.0f, (float)(seconds - t) * 4);
        b[i] = y * env;
        if (fabsf(b[i]) > peak) peak = fabsf(b[i]);
    }
    if (peak > 0) for (int i = 0; i < n; i++) b[i] *= 0.89f / peak;
    *out = b;
    return n;
}
