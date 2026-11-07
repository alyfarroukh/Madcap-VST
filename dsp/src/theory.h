/* Music theory helpers: scales, diatonic chords, intervals, voicings. */
#ifndef GS_THEORY_H
#define GS_THEORY_H

#ifdef __cplusplus
extern "C" {
#endif

#define GS_MAX_VOICES 16
#define GS_NUM_DEGREES 7

typedef struct {
    const char *name;
    int len;          /* notes per octave (5 or 7) */
    int semis[7];
} GsScale;

typedef struct {
    const char *name;
    int n;
    int steps[6];     /* scale steps above the degree; GS_OCT = one octave */
} GsChordType;

#define GS_OCT 99

extern const char *const GS_NOTE_NAMES[12];
extern const GsScale GS_SCALES[];
extern const int GS_NUM_SCALES;
extern const GsChordType GS_CHORDS[];
extern const int GS_NUM_CHORDS;

/* Semitone offset (from the key root) of an arbitrary scale step (can be negative). */
int gs_scale_step_semis(int scale, int step);

/* Build the set of pitches (semitones relative to key root) for every degree
 * set in degree_mask, using chord type and voicing (0..1). Returns count. */
int gs_build_chord(int scale, int degree_mask, int chord, float voicing, float *out_semis);

/* "C maj", "A min", ... for display. */
void gs_degree_name(int key, int scale, int degree, char *buf, int buflen);

/* MIDI note -> nearest scale degree (0..6) of key/scale. Degrees beyond the
 * scale length (pentatonic: 5) wrap. Ties go to the lower degree. */
int gs_note_to_degree(int key, int scale, int midi_note);
/* MIDI note -> OCTAVE parameter (-2..+1); C4 (60) .. B4 = 0. */
int gs_note_to_octave(int midi_note);

#ifdef __cplusplus
}
#endif
#endif
