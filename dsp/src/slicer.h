#ifndef GS_SLICER_H
#define GS_SLICER_H
#ifdef __cplusplus
extern "C" {
#endif

#define GS_MAX_SLICES 16
#define GS_MAX_ONSETS 256

typedef struct {
    int len;                       /* source length in frames */
    int n_onsets;
    int onset_pos[GS_MAX_ONSETS];  /* sorted by strength, strongest first */
} GsSlicer;

/* Analyse the source for transients (call off the audio thread). */
void gs_slicer_analyse(GsSlicer *s, const float *src, int len, int sr);

/* Fill bounds[0..count] with slice boundaries (frames) inside [start, end).
 * mode 0 = grid, 1 = transient. */
void gs_slicer_bounds(const GsSlicer *s, int count, int mode, int start, int end, int *bounds);

#ifdef __cplusplus
}
#endif
#endif
