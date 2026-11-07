#ifndef GS_DEMO_H
#define GS_DEMO_H
#ifdef __cplusplus
extern "C" {
#endif
/* Synthesise a sung-vowel demo sample. Returns frames, *out is malloc'd. */
int gs_demo_voice(float **out, int sr, float seconds);
#ifdef __cplusplus
}
#endif
#endif
