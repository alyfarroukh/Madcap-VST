/* Built-in Bauhaus-style posters (cream paper, flat circles, half circles,
 * hatched line bundles) so the image sweeper has something to play right away.
 * Pixels are 0xAABBGGRR (vita2d's default ABGR texture layout). */
#include "posters.h"
#include <math.h>
#include <string.h>

enum { S_CIRCLE, S_HALF, S_HATCH_HALF, S_RECT, S_LINES };
typedef struct {
    int type;
    float x, y, r;        /* circle centre/radius; rect x,y,w(h=r2) ; lines centre */
    float a;              /* angle (radians) for half cuts and line bundles */
    float w, h;           /* rect size / bundle length, line count */
    float sp, th;         /* bundle spacing / line thickness */
    unsigned col;
    float alpha;
} Shape;

#define CREAM   0xFFECF4FBu
#define INKC    0xFF262626u
#define MUSTARD 0xFF1FA8E3u
#define ORANGE  0xFF4F8AFFu
#define TEAL    0xFFA5C36Cu
#define CLAY    0xFF5A6FD8u

static const Shape P0[] = {        /* "SOLSTICE" */
    {S_CIRCLE, 392, 92, 118, 0, 0, 0, 0, 0, MUSTARD, 1},
    {S_HALF, 250, 138, 84, 2.5f, 0, 0, 0, 0, INKC, 0.92f},
    {S_HATCH_HALF, 250, 138, 84, 2.5f, 0, 0, 9, 2, INKC, 1},
    {S_CIRCLE, 140, 206, 24, 0, 0, 0, 0, 0, MUSTARD, 0.95f},
};
static const Shape P1[] = {        /* "SUNSET" */
    {S_CIRCLE, 205, 60, 150, 0, 0, 0, 0, 0, ORANGE, 0.95f},
    {S_CIRCLE, 300, 176, 68, 0, 0, 0, 0, 0, INKC, 0.88f},
    {S_LINES, 300, 128, 0, 0.78f, 270, 10, 12, 2.2f, INKC, 1},
};
static const Shape P2[] = {        /* "TIDE" */
    {S_RECT, 300, 122, 0, 0, 176, 134, 0, 0, TEAL, 1},
    {S_CIRCLE, 150, 108, 56, 0, 0, 0, 0, 0, INKC, 1},
    {S_LINES, 300, 116, 0, -0.36f, 340, 8, 11, 2.0f, INKC, 1},
};
static const Shape P3[] = {        /* "STEPS" */
    {S_HALF, 128, 196, 92, -1.5708f, 0, 0, 0, 0, TEAL, 1},
    {S_CIRCLE, 300, 92, 64, 0, 0, 0, 0, 0, MUSTARD, 1},
    {S_HALF, 300, 92, 64, 1.5708f, 0, 0, 0, 0, CLAY, 0.9f},
    {S_LINES, 428, 132, 0, 1.5708f, 220, 7, 12, 2.2f, INKC, 1},
    {S_CIRCLE, 448, 54, 20, 0, 0, 0, 0, 0, INKC, 1},
};
static const Shape *POSTERS[] = {P0, P1, P2, P3};
static const int POSTER_N[] = {4, 3, 3, 5};
static const char *const NAMES[] = {"SOLSTICE", "SUNSET", "TIDE", "STEPS"};

int gs_poster_count(void) { return 4; }
const char *gs_poster_name(int i) { return NAMES[(i % 4 + 4) % 4]; }

static int inside(const Shape *s, float x, float y)
{
    float dx = x - s->x, dy = y - s->y;
    switch (s->type) {
    case S_CIRCLE: return dx * dx + dy * dy <= s->r * s->r;
    case S_HALF:
        return dx * dx + dy * dy <= s->r * s->r && (dx * cosf(s->a) + dy * sinf(s->a)) >= 0;
    case S_HATCH_HALF: {
        if (dx * dx + dy * dy > s->r * s->r) return 0;
        float d = dx * cosf(s->a) + dy * sinf(s->a);
        if (d >= 0) return 0;
        float m = fmodf(-d, s->sp);
        return m < s->th;
    }
    case S_RECT: return x >= s->x && x < s->x + s->w && y >= s->y && y < s->y + s->h;
    case S_LINES: {
        float u = dx * cosf(s->a) + dy * sinf(s->a);
        float v = -dx * sinf(s->a) + dy * cosf(s->a);
        float span = s->h * s->sp;
        if (fabsf(u) > s->w / 2 || v < -span / 2 || v >= span / 2) return 0;
        return fmodf(v + span / 2, s->sp) < s->th;
    }
    }
    return 0;
}

static unsigned blend(unsigned dst, unsigned src, float a)
{
    unsigned r = (unsigned)((src & 255) * a + (dst & 255) * (1 - a));
    unsigned g = (unsigned)(((src >> 8) & 255) * a + ((dst >> 8) & 255) * (1 - a));
    unsigned b = (unsigned)(((src >> 16) & 255) * a + ((dst >> 16) & 255) * (1 - a));
    return 0xFF000000u | (b << 16) | (g << 8) | r;
}

void gs_poster_render(int idx, unsigned *px, int w, int h, int stride)
{
    idx = (idx % 4 + 4) % 4;
    const Shape *sh = POSTERS[idx];
    int ns = POSTER_N[idx];
    float sx = 512.0f / w, sy = 256.0f / h;          /* shapes are authored on a 512 x 256 canvas */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            unsigned c = CREAM;
            for (int k = 0; k < ns; k++) {
                int hits = 0;                         /* 2 x 2 supersampling for soft edges */
                for (int j = 0; j < 2; j++)
                    for (int i = 0; i < 2; i++)
                        hits += inside(&sh[k], (x + 0.25f + 0.5f * i) * sx, (y + 0.25f + 0.5f * j) * sy);
                if (hits) c = blend(c, sh[k].col, sh[k].alpha * hits / 4.0f);
            }
            px[y * stride + x] = c;
        }
}

void gs_rgba_to_grid(const unsigned *px, int w, int h, int stride, float *grid, int cols, int rows)
{
    for (int r = 0; r < rows; r++)
        for (int c = 0; c < cols; c++) {
            int x0 = c * w / cols, x1 = (c + 1) * w / cols, y0 = r * h / rows, y1 = (r + 1) * h / rows;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            float s = 0; int n = 0;
            for (int y = y0; y < y1; y++)
                for (int x = x0; x < x1; x++) {
                    unsigned p = px[y * stride + x];
                    s += 0.299f * (p & 255) + 0.587f * ((p >> 8) & 255) + 0.114f * ((p >> 16) & 255);
                    n++;
                }
            grid[r * cols + c] = s / (255.0f * n);
        }
}
