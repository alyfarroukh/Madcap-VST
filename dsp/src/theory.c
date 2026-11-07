#include "theory.h"
#include <stdio.h>
#include <string.h>

const char *const GS_NOTE_NAMES[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};

const GsScale GS_SCALES[] = {
    {"Major",      7, {0,2,4,5,7,9,11}},
    {"Minor",      7, {0,2,3,5,7,8,10}},
    {"Dorian",     7, {0,2,3,5,7,9,10}},
    {"Phrygian",   7, {0,1,3,5,7,8,10}},
    {"Lydian",     7, {0,2,4,6,7,9,11}},
    {"Mixolydian", 7, {0,2,4,5,7,9,10}},
    {"Harm Minor", 7, {0,2,3,5,7,8,11}},
    {"Pent Major", 5, {0,2,4,7,9}},
    {"Pent Minor", 5, {0,3,5,7,10}},
};
const int GS_NUM_SCALES = sizeof(GS_SCALES) / sizeof(GS_SCALES[0]);

/* Interval types first (root + one interval), then chords. */
const GsChordType GS_CHORDS[] = {
    {"Unison", 1, {0}},
    {"Octave", 2, {0, GS_OCT}},
    {"3rd",    2, {0, 2}},
    {"4th",    2, {0, 3}},
    {"5th",    2, {0, 4}},
    {"Triad",  3, {0, 2, 4}},
    {"Sus2",   3, {0, 1, 4}},
    {"Sus4",   3, {0, 3, 4}},
    {"7th",    4, {0, 2, 4, 6}},
    {"9th",    5, {0, 2, 4, 6, 8}},
};
const int GS_NUM_CHORDS = sizeof(GS_CHORDS) / sizeof(GS_CHORDS[0]);

int gs_scale_step_semis(int scale, int step)
{
    if (scale < 0) scale = 0;
    if (scale >= GS_NUM_SCALES) scale = GS_NUM_SCALES - 1;
    const GsScale *s = &GS_SCALES[scale];
    int oct = step >= 0 ? step / s->len : -((-step + s->len - 1) / s->len);
    int idx = step - oct * s->len;
    return s->semis[idx] + 12 * oct;
}

static int add_unique(float *out, int n, float v)
{
    for (int i = 0; i < n; i++)
        if (out[i] == v) return n;
    if (n >= GS_MAX_VOICES) return n;
    out[n] = v;
    return n + 1;
}

int gs_build_chord(int scale, int degree_mask, int chord, float voicing, float *out)
{
    if (chord < 0) chord = 0;
    if (chord >= GS_NUM_CHORDS) chord = GS_NUM_CHORDS - 1;
    if (scale < 0) scale = 0;
    if (scale >= GS_NUM_SCALES) scale = GS_NUM_SCALES - 1;
    if (degree_mask == 0) degree_mask = 1;
    const GsChordType *c = &GS_CHORDS[chord];
    int len = GS_SCALES[scale].len;
    int n = 0;

    for (int d = 0; d < GS_NUM_DEGREES; d++) {
        if (!(degree_mask & (1 << d))) continue;
        int root = gs_scale_step_semis(scale, d);
        for (int i = 0; i < c->n; i++) {
            int st = c->steps[i] == GS_OCT ? len : c->steps[i];
            int semi = gs_scale_step_semis(scale, d + st);
            /* voicing > .5 : open voicing, lift every other upper tone an octave */
            if (voicing > 0.5f && i > 0 && (i & 1)) semi += 12;
            n = add_unique(out, n, (float)semi);
        }
        if (voicing > 0.25f) n = add_unique(out, n, (float)(root - 12)); /* sub root */
        if (voicing > 0.75f) n = add_unique(out, n, (float)(root + 24)); /* high root */
    }
    /* sort ascending (insertion sort, n <= 16) */
    for (int i = 1; i < n; i++) {
        float v = out[i]; int j = i - 1;
        while (j >= 0 && out[j] > v) { out[j + 1] = out[j]; j--; }
        out[j + 1] = v;
    }
    return n;
}

void gs_degree_name(int key, int scale, int degree, char *buf, int buflen)
{
    int r = gs_scale_step_semis(scale, degree);
    int third = gs_scale_step_semis(scale, degree + 2) - r;
    int fifth = gs_scale_step_semis(scale, degree + 4) - r;
    const char *q = "";
    if (third == 4 && fifth == 7) q = "maj";
    else if (third == 3 && fifth == 7) q = "min";
    else if (third == 3 && fifth == 6) q = "dim";
    else if (third == 4 && fifth == 8) q = "aug";
    else if (fifth == 7) q = "5";
    snprintf(buf, buflen, "%s %s", GS_NOTE_NAMES[((key + r) % 12 + 12) % 12], q);
}

int gs_note_to_degree(int key, int scale, int midi_note)
{
    if (scale < 0) scale = 0;
    if (scale >= GS_NUM_SCALES) scale = GS_NUM_SCALES - 1;
    const GsScale *s = &GS_SCALES[scale];
    int pc = ((midi_note - key) % 12 + 12) % 12;
    int best = 0, bd = 99;
    for (int i = 0; i < s->len; i++) {
        int d = pc - s->semis[i];
        if (d < 0) d = -d;
        if (12 - d < d) d = 12 - d;          /* circular distance */
        if (d < bd) { bd = d; best = i; }
    }
    return best % GS_NUM_DEGREES;
}

int gs_note_to_octave(int midi_note)
{
    int o = midi_note / 12 - 5;
    if (o < -2) o = -2;
    if (o > 1) o = 1;
    return o;
}
