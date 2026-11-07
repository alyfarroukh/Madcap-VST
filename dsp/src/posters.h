#ifndef GS_POSTERS_H
#define GS_POSTERS_H
#ifdef __cplusplus
extern "C" {
#endif
int  gs_poster_count(void);
const char *gs_poster_name(int i);
/* Draw poster i into a 0xAABBGGRR pixel buffer of w x h (stride in pixels). */
void gs_poster_render(int i, unsigned *px, int w, int h, int stride);
/* Average luminance (0..1) into a rows x cols grid (row 0 = top). */
void gs_rgba_to_grid(const unsigned *px, int w, int h, int stride, float *grid, int cols, int rows);
#ifdef __cplusplus
}
#endif
#endif
