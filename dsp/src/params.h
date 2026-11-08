/* Flat parameter table shared by the engine, the plugin (APVTS), the UI and presets.
 * Ported from Madcap Vita. The looper was removed; MIDI / transport / plugin-only
 * parameters were added at the end. Parameter IDs (GsParamDesc.id) are stable:
 * never rename them, hosts store automation and presets by ID. */
#ifndef GS_PARAMS_H
#define GS_PARAMS_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
    /* GRAIN */
    P_MODE, P_SIZE, P_DENSITY, P_POSITION, P_SPRAY, P_SCAN, P_REVERSE,
    P_SPREAD, P_OCTAVES, P_DETUNE, P_GLIDE, P_TONE, P_BPM, P_GRAIN_LVL,
    /* PITCH / CHORD */
    P_KEY, P_SCALE, P_SAMPLE_ROOT, P_OCTAVE, P_CHORD, P_VOICING,
    /* SLICE */
    P_SLICES, P_SLICE_MODE, P_SLICE_SEL, P_SLICE_SPAN,
    /* FX: drive */
    P_DRV_AMT, P_DRV_TYPE, P_DRV_TONE, P_DRV_MIX,
    /* FX: compressor */
    P_CMP_THR, P_CMP_RATIO, P_CMP_ATK, P_CMP_REL, P_CMP_MAKEUP, P_CMP_MIX,
    /* FX: delay */
    P_DLY_TIME, P_DLY_DIV, P_DLY_FB, P_DLY_DAMP, P_DLY_PING, P_DLY_MIX,
    /* FX: reverb */
    P_REV_SIZE, P_REV_DAMP, P_REV_WIDTH, P_REV_PRE, P_REV_MIX,
    /* chain order */
    P_FX_ORDER,
    /* MASTER */
    P_MASTER,
    /* sample trim (manual cut, 0..1 of the source) */
    P_TRIM_IN, P_TRIM_OUT,
    /* effect on/off switches */
    P_DRV_ON, P_CMP_ON, P_DLY_ON, P_REV_ON,
    P_REV_MOD,
    /* input select + image sweeper */
    P_SRC, P_IMG_SOURCE, P_IMG_MODE, P_IMG_READ, P_IMG_KEY, P_IMG_SCALE, P_IMG_LOW, P_IMG_RANGE,
    P_IMG_GRID, P_IMG_VOICES, P_IMG_WAVE, P_IMG_DECAY, P_IMG_SWEEP, P_IMG_SYNC, P_IMG_LEVEL,
    /* arpeggiator */
    P_ARP_ON, P_ARP_MODE, P_ARP_RATE, P_ARP_OCT, P_ARP_GATE, P_ARP_SWING, P_ARP_ATTACK, P_ARP_RELEASE,
    P_ARP_START, P_ARP_WALK, P_ARP_GRAINS, P_ARP_LEVEL, P_ARP_LEN, P_ARP_PATTERN,
    P_IMG_DRY,
    P_GRAIN_ON,
    /* ---- added for the desktop plugin ---- */
    P_GRAIN_HOLD,     /* freeze the grain read position */
    P_GRAIN_INTERP,   /* 0 linear, 1 cubic */
    P_IMG_HOLD,       /* freeze the 6 s live buffer */
    P_DRV_OS,         /* drive oversampling 0 = 2x, 1 = 4x */
    P_DEG1, P_DEG2, P_DEG3, P_DEG4, P_DEG5, P_DEG6, P_DEG7,   /* chord pads I..VII */
    P_MIDI_MODE,      /* 0 degree, 1 root */
    P_LATCH,
    P_VEL_AMT,
    P_MOD_DEST,
    P_PLAY,           /* transport PLAY / STOP */
    P_FOLLOW_HOST,    /* host stop also stops Madcap */
    P_TEMPO_SRC,      /* 0 host, 1 manual (BPM parameter) */
    P_PREVIEW_LOOP,
    P_IMG_RES,        /* image analysis grid: max columns 64 .. 2048 */
    P_DRY_LVL,        /* PLAY tab: the plain input (no grains / arp / FX), mixed before the master */
    P_WET_LVL,        /* PLAY tab: everything Madcap generates (grains, arp, FX) */
    P_REV_MODE,       /* reverb: 0 ambient, 1 smear */
    P_COUNT
};

enum { PAGE_SAMPLE, PAGE_IMAGE, PAGE_GRAIN, PAGE_ARP, PAGE_FX, PAGE_PLAY, PAGE_GLOBAL, PAGE_COUNT };

enum { FMT_FLOAT, FMT_PCT, FMT_MS, FMT_INT, FMT_DB, FMT_ENUM, FMT_BIPOLAR, FMT_RATIO, FMT_HZ, FMT_CENTS, FMT_SEC };

typedef struct {
    const char *id;            /* stable parameter ID */
    const char *name;
    int page;
    float min, max, def;
    int fmt;
    int discrete;              /* 1 = integer steps */
    const char *const *labels; /* for FMT_ENUM */
    float curve;               /* >1 = value = min + (max-min) * norm^curve */
    int smooth;                /* 1 = continuous, smoothed on the audio thread */
} GsParamDesc;

extern const GsParamDesc GS_PARAMS[P_COUNT];
extern const char *const GS_PARAM_SHORT[P_COUNT];   /* knob labels */

extern const char *const GS_MODE_NAMES[];
#define GS_NUM_MODES 6
enum { MODE_CLOUD, MODE_MOSAIC, MODE_ARP, MODE_STRUM, MODE_TUNNEL, MODE_GLIDE };

extern const char *const GS_FX_NAMES[4];   /* DRV CMP DLY REV */
extern const int GS_FX_ORDERS[24][4];
extern const char *const GS_FX_ORDER_LABELS[24];
/* Index of the permutation that equals order[4], or -1. */
int gs_fx_order_index(const int order[4]);

extern const char *const GS_DLY_DIV_LABELS[8];
extern const float GS_DLY_DIV_BEATS[8];
extern const int GS_SLICE_COUNTS[5];
extern const char *const GS_MOD_DEST_LABELS[];
#define GS_NUM_MOD_DESTS 10
/* Parameter controlled by the mod wheel for a MOD_DEST value (-1 = none). */
int gs_mod_dest_param(int dest);

/* Number of labels of an enum parameter (max - min + 1). */
int gs_param_num_steps(int id);
/* Format a value for display. */
void gs_param_format(int id, float v, char *buf, int buflen);
/* Parse user text back to a value (inverse of gs_param_format, lenient). */
float gs_param_parse(int id, const char *text);
/* Map 0..1 <-> value (honours curve and discreteness). */
float gs_param_from_norm(int id, float n);
float gs_param_to_norm(int id, float v);
float gs_param_clamp(int id, float v);
/* Look up a parameter by its stable ID, -1 if unknown. */
int gs_param_find(const char *id);

#ifdef __cplusplus
}
#endif
#endif
