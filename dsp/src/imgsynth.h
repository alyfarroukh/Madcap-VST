/* Image noise sweeper: a vertical scan line sweeps left -> right across an
 * image; each column's pixels become sound (spectral chords or a brightness
 * melody), as notes, pitched noise, or both. */
#ifndef GS_IMGSYNTH_H
#define GS_IMGSYNTH_H

#define IMG_COLS 256
#define IMG_ROWS 128
#define IMG_MAX_NOTES 128

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float ph, inc;            /* oscillator */
    float env, target;        /* envelope */
    float att_left;           /* attack samples remaining */
    float dec;                /* per-sample decay multiplier after the attack */
    float slew;               /* smooth mode: per-sample slew toward target */
    int gate;
    float freq, freq_target;
    float g, a1, a2, a3, ic1, ic2, ngain;   /* noise band-pass (TPT SVF) */
    float pan;
} GsImgVoice;

typedef struct {
    float grid[IMG_ROWS][IMG_COLS];      /* luminance 0..1, row 0 = top */
    int has_image;
    int ctl_left;                        /* samples until the next control-rate analysis */
    float bend;                          /* pitch-bend frequency multiplier */
    float rel_mul;                       /* quick-release multiplier for the current sample rate */

    double phase;                        /* sweep position 0..1 */
    int last_step;
    float scrub;                           /* >= 0: finger is holding the scan line here */
    unsigned rng;

    int notes[IMG_MAX_NOTES], nnotes;    /* MIDI notes, low -> high (nnotes = the picture's range) */
    int nnext;                           /* notes[] continues up to here, for harmony above the range */
    int deg_shift;                       /* PLAY-tab chord pad: move the melody this many scale steps */
    GsImgVoice v[IMG_MAX_NOTES];         /* spectral: one voice per note */
    GsImgVoice mono;                     /* brightness mode */
    int mono_note;
    float sine[1025];

    /* read by the UI */
    float ui_phase;
    int ui_nnotes;
    float ui_amp[IMG_MAX_NOTES];
    float ui_mono_y;                     /* 0 bottom .. 1 top, <0 silent */
} GsImgSynth;

extern const char *const GS_IMG_SCALE_NAMES[];
#define GS_IMG_NUM_SCALES 8
extern const int GS_IMG_GRID_STEPS[8];     /* 0 = smooth */

void gs_img_init(GsImgSynth *s);
void gs_img_stop(GsImgSynth *s);
/* Copy a new IMG_ROWS x IMG_COLS luminance grid in (row 0 = top). Audio thread. */
void gs_img_set_grid(GsImgSynth *s, const float *grid);
/* Fill out[] with the MIDI notes for key/scale/low note/range; returns count. */
int  gs_img_build_notes(int key, int scale, int low, int range, int *out, int max);
/* Render (adds into L/R). p = engine parameter array. */
void gs_img_process(GsImgSynth *s, const float *p, float *L, float *R, int n, int sr);
/* Steps per sweep for the GRID parameter (Smooth counts as 16 for sync). */
int  gs_img_sync_steps(const float *p);
/* MIDI notes currently gated on (note voices + bright-mode melody). Returns count. */
int  gs_img_active_notes(const GsImgSynth *s, int *out, int max);

#ifdef __cplusplus
}
#endif

#endif
