/* Sample arpeggiator: plays the cut/slice region of the sample as pitched notes
 * stepping through the current chord (from the PLAY tab), before the FX chain. */
#ifndef GS_ARP_H
#define GS_ARP_H

#define ARP_VOICES 10
#define ARP_MAX_NOTES 40
#define ARP_STEPS 16

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int active;
    double pos;
    float rate, env, att_inc, rel_mul, pan;
    int gate_left, releasing;
    int s, e;                 /* region it loops in */
} GsArpVoice;

typedef struct {
    GsArpVoice v[ARP_VOICES];
    double t_next;            /* samples until the next step */
    int step;                 /* pattern position */
    int idx, dir;             /* position in the note list */
    float walk;               /* accumulated start offset */
    unsigned rng;
    /* read by the UI */
    int ui_step;
    int ui_idx;
    int ui_nnotes;
    float ui_notes[ARP_MAX_NOTES];            /* semitones of the note list */
    int ui_trig;                              /* increments on every trigger */
} GsArp;

extern const char *const GS_ARP_MODE_NAMES[];
extern const char *const GS_ARP_RATE_NAMES[];

void gs_arp_reset(GsArp *a);
/* p: engine params; chord: semitones relative to the key (sorted); transpose: semitones from key to sample;
 * the region [s, e) of src is played. Adds into L/R. */
/* live_head >= 0: src is a rolling live buffer and notes start relative to its write head.
 * allow_new = 0: keep stepping but start no new notes (gate closed).
 * bend: pitch-bend playback-rate multiplier applied to all sounding notes. */
void gs_arp_process(GsArp *a, const float *p, const float *src, int s, int e, int live_head,
                    const float *chord, int nchord, float transpose, int allow_new, float bend,
                    float *L, float *R, int n, int sr);

/* Duration of one step in beats for the RATE parameter value. */
float gs_arp_step_beats(int rate);
/* Lock the step clock to the host: ppq_in_bar = beats since the last bar start,
 * beat_samples = samples per beat. Small drifts are corrected; jumps resync. */
void gs_arp_sync(GsArp *a, const float *p, double ppq_in_bar, double beat_samples);

#ifdef __cplusplus
}
#endif

#endif
