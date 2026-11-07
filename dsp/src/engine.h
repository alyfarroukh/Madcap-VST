/* Madcap engine (ported from Madcap Vita): source -> slicer region -> granular cloud
 * + arpeggiator -> tone -> FX chain (drive / comp / delay / reverb, reorderable)
 * -> master -> look-ahead limiter.
 *
 * Desktop changes vs. the Vita:
 *  - any sample rate (buffers sized at init), any block size (1 .. anything)
 *  - no fixed 256-sample blocks: everything block-size dependent is time-based
 *  - the looper is gone; source swaps are a plain audio-thread call
 *    (the C++ wrapper owns the lock-free hand-off and memory)
 *  - MIDI gate (latch off), pitch bend, host tempo / bar sync
 *  - everything here is touched by the audio thread only; the wrapper copies the
 *    ui_* fields into atomics for the editor. */
#ifndef GS_ENGINE_H
#define GS_ENGINE_H

#include "params.h"
#include "theory.h"
#include "slicer.h"
#include "fx.h"
#include "imgsynth.h"
#include "arp.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GS_MAX_GRAINS 128
#define GS_BLOCK_MAX 256          /* internal sub-block size; any host block size is split */
#define GS_MAX_SRC_SECONDS 60
#define GS_LIVE_SECONDS 6
#define GS_LIM_MAX 1024           /* look-ahead buffer capacity (frames) */
#define GS_WIN_N 1024
#define GS_UI_GRAINS 64
#define GS_LIVE_ENV 256

typedef struct {
    double pos;       /* read position in source frames */
    float rate;       /* signed playback increment */
    int len, age, delay;
    float gl, gr;     /* pan * amplitude */
    int rs, re;       /* region it reads from */
} GsGrain;

typedef struct {
    int sr;
    float p[P_COUNT];             /* parameter values (written by the wrapper each slice) */
    int degree_mask;              /* which scale degrees (chords) are active */
    int playing;                  /* transport: 1 = play, 0 = stop everything */
    int run_state;
    int gate;                     /* MIDI gate: 0 = no new grains / notes (latch off, nothing held) */
    float gate_env;               /* smoothed gate for the image sweeper */
    int gate_prev;
    float bend_semis;             /* pitch bend, semitones */
    int preview_on, preview_loop; /* audition the cut section dry */
    double preview_pos;

    /* host sync (set by gs_engine_set_host before each process call) */
    int host_playing;
    double host_ppq, host_bar_ppq;

    /* source (not owned) */
    const float *src; int src_len;
    GsSlicer slicer;
    int bounds[GS_MAX_SLICES + 1]; int nslices;
    int region_s, region_e;
    int trim_s, trim_e;

    /* granular */
    GsGrain grains[GS_MAX_GRAINS];
    int ngrains;
    double spawn_t;
    int step_count;
    float playhead;
    float voice_tgt[GS_MAX_VOICES], voice_cur[GS_MAX_VOICES];
    int nvoices, arp_idx, arp_dir;
    unsigned rng;
    float svf_ic1[2], svf_ic2[2];
    float win[GS_WIN_N + 2];

    /* image sweeper (used when Input = IMAGE) */
    GsImgSynth img;
    int last_src;
    float *live; int live_len; int live_w;  /* rolling recording of the sweeper for grains / arp */
    float img_l[GS_BLOCK_MAX], img_r[GS_BLOCK_MAX];
    int live_mode;                          /* grains currently read the live buffer */
    const float *gsrc; int gsrc_len;        /* what the grains read from */
    float live_env[GS_LIVE_ENV];            /* peak envelope of the live buffer (UI) */

    /* arpeggiator */
    GsArp arp;
    int arp_was_on;

    /* fx */
    GsDrive drv; GsComp cmp; GsDelay dly; GsReverb rev;
    float out_gain;
    int lim_la;                             /* look-ahead (frames) = reported latency */
    float lim_l[GS_LIM_MAX], lim_r[GS_LIM_MAX], lim_g, lim_hold;
    int lim_pos, lim_hold_t;

    /* scratch */
    float bus_l[GS_BLOCK_MAX], bus_r[GS_BLOCK_MAX];

    /* meters / visualisation (copied out by the wrapper) */
    float peak_l, peak_r, comp_gr, ui_playhead;
    int ui_ngrains;
    float ui_grain_pos[GS_UI_GRAINS];
    float ui_grain_amp[GS_UI_GRAINS];
    float ui_preview_pos;
} GsEngine;

/* Allocates the delay / reverb / live buffers for sr. Returns 0 on success. Not real-time. */
int  gs_engine_init(GsEngine *e, int sr);
void gs_engine_free(GsEngine *e);
void gs_engine_reset_params(GsEngine *e);

/* Latency added by the limiter look-ahead (frames). */
int  gs_engine_latency(const GsEngine *e);

/* Host position for the next gs_engine_process call. ppq < 0 = unknown. */
void gs_engine_set_host(GsEngine *e, int playing, double ppq, double bar_start_ppq);

/* Render n frames of stereo float audio (any n >= 0). Audio thread. */
void gs_engine_process(GsEngine *e, float *out_l, float *out_r, int n);

/* Swap in a new mono source at the engine's sample rate (audio thread, no allocation).
 * The engine does not own buf; returns the previous source pointer so the caller can
 * retire it once this call has returned. sl may be NULL (no transient analysis). */
const float *gs_engine_swap_source(GsEngine *e, const float *buf, int len, const GsSlicer *sl);

/* Current granular region in frames. */
void gs_engine_region(const GsEngine *e, int *s, int *en);

/* Stop everything right now (grains, arp, image voices, FX tails). Audio thread. */
void gs_engine_panic(GsEngine *e);

#ifdef __cplusplus
}
#endif
#endif
