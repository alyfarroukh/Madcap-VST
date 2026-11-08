#include "params.h"
#include "theory.h"
#include "fx.h"
#include "imgsynth.h"
#include "arp.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const GS_MODE_NAMES[] = {"Cloud", "Mosaic", "Arp", "Strum", "Tunnel", "Glide"};
static const char *const ONOFF[] = {"Off", "On"};
static const char *const SRC_LABELS[] = {"Sample", "Image"};
static const char *const IMG_SOURCE_LABELS[] = {"Noise", "Notes", "Noise+Notes"};
static const char *const IMG_MODE_LABELS[] = {"Spectral", "Bright", "Height"};
static const char *const IMG_READ_LABELS[] = {"Light", "Dark"};
static const char *const IMG_WAVE_LABELS[] = {"Sine", "Tri", "Saw", "Square"};
static const char *const DRV_TYPES[] = {"Tape", "Fuzz", "Crush"};
static const char *const DRV_OS_LABELS[] = {"2x", "4x"};
static const char *const SLICE_MODES[] = {"Grid", "Hits"};
static const char *const SLICE_COUNT_LABELS[] = {"1", "2", "4", "8", "16"};
static const char *const INTERP_LABELS[] = {"Linear", "Cubic"};
static const char *const MIDI_MODE_LABELS[] = {"Degree", "Root"};
static const char *const TEMPO_SRC_LABELS[] = {"Host", "Manual"};
static const char *const PLAY_LABELS[] = {"Stop", "Play"};
static const char *const IMG_RES_LABELS[] = {"64", "128", "256", "512", "1024", "2048"};
static const char *const REV_MODE_LABELS[] = {"Ambient", "Smear"};
const char *const GS_MOD_DEST_LABELS[] = {"None", "Spray", "Position", "Size", "Density", "Scan", "Tone",
                                          "Reverb Mix", "Delay Fdbk", "Drive"};
const int GS_SLICE_COUNTS[5] = {1, 2, 4, 8, 16};
const char *const GS_DLY_DIV_LABELS[8] = {"Free", "1/16", "1/8", "1/8.", "1/4", "1/4.", "1/2", "1 bar"};
const float GS_DLY_DIV_BEATS[8] = {0, 0.25f, 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 4.0f};
const char *const GS_FX_NAMES[4] = {"DRV", "CMP", "DLY", "REV"};

const int GS_FX_ORDERS[24][4] = {
    {0,1,2,3},{0,1,3,2},{0,2,1,3},{0,2,3,1},{0,3,1,2},{0,3,2,1},
    {1,0,2,3},{1,0,3,2},{1,2,0,3},{1,2,3,0},{1,3,0,2},{1,3,2,0},
    {2,0,1,3},{2,0,3,1},{2,1,0,3},{2,1,3,0},{2,3,0,1},{2,3,1,0},
    {3,0,1,2},{3,0,2,1},{3,1,0,2},{3,1,2,0},{3,2,0,1},{3,2,1,0}
};
const char *const GS_FX_ORDER_LABELS[24] = {
    "DRV>CMP>DLY>REV", "DRV>CMP>REV>DLY", "DRV>DLY>CMP>REV", "DRV>DLY>REV>CMP",
    "DRV>REV>CMP>DLY", "DRV>REV>DLY>CMP", "CMP>DRV>DLY>REV", "CMP>DRV>REV>DLY",
    "CMP>DLY>DRV>REV", "CMP>DLY>REV>DRV", "CMP>REV>DRV>DLY", "CMP>REV>DLY>DRV",
    "DLY>DRV>CMP>REV", "DLY>DRV>REV>CMP", "DLY>CMP>DRV>REV", "DLY>CMP>REV>DRV",
    "DLY>REV>DRV>CMP", "DLY>REV>CMP>DRV", "REV>DRV>CMP>DLY", "REV>DRV>DLY>CMP",
    "REV>CMP>DRV>DLY", "REV>CMP>DLY>DRV", "REV>DLY>DRV>CMP", "REV>DLY>CMP>DRV"
};

int gs_fx_order_index(const int order[4])
{
    for (int i = 0; i < 24; i++)
        if (!memcmp(GS_FX_ORDERS[i], order, sizeof(int) * 4)) return i;
    return -1;
}

static const char *const SCALE_LABELS[] = {
    "Major", "Minor", "Dorian", "Phrygian", "Lydian", "Mixolydian", "Harm Minor", "Pent Major", "Pent Minor"
};
static const char *const CHORD_LABELS[] = {
    "Unison", "Octave", "3rd", "4th", "5th", "Triad", "Sus2", "Sus4", "7th", "9th"
};

/* id, name, page, min, max, default, format, discrete, labels, curve, smooth */
#define C(id, name, page, mn, mx, df, fmt, cur) {id, name, page, mn, mx, df, fmt, 0, 0, cur, 1}
#define D(id, name, page, mn, mx, df, fmt, lab) {id, name, page, mn, mx, df, fmt, 1, lab, 1, 0}

const GsParamDesc GS_PARAMS[P_COUNT] = {
    [P_MODE]       = D("grain_mode",   "Grain Mode",   PAGE_GRAIN, 0, 5, 0, FMT_ENUM, GS_MODE_NAMES),
    [P_SIZE]       = C("grain_size",   "Grain Size",   PAGE_GRAIN, 10, 1000, 180, FMT_MS, 2.5f),
    [P_DENSITY]    = C("grain_density","Density",      PAGE_GRAIN, 1, 60, 14, FMT_HZ, 2),
    [P_POSITION]   = C("grain_pos",    "Position",     PAGE_GRAIN, 0, 1, 0, FMT_PCT, 1),
    [P_SPRAY]      = C("grain_spray",  "Spray",        PAGE_GRAIN, 0, 1, 0.15f, FMT_PCT, 1.5f),
    [P_SCAN]       = C("grain_scan",   "Scan Speed",   PAGE_GRAIN, -1, 1, 0.25f, FMT_BIPOLAR, 1),
    [P_REVERSE]    = C("grain_reverse","Reverse",      PAGE_GRAIN, 0, 1, 0.2f, FMT_PCT, 1),
    [P_SPREAD]     = C("grain_stereo", "Stereo",       PAGE_GRAIN, 0, 1, 0.7f, FMT_PCT, 1),
    [P_OCTAVES]    = C("grain_octjump","Octave Jump",  PAGE_GRAIN, 0, 1, 0.15f, FMT_PCT, 1),
    [P_DETUNE]     = C("grain_detune", "Detune",       PAGE_GRAIN, 0, 50, 6, FMT_CENTS, 1.5f),
    [P_GLIDE]      = C("grain_glide",  "Glide",        PAGE_GRAIN, 0, 2000, 250, FMT_MS, 2),
    [P_TONE]       = C("grain_tone",   "Tone LP/HP",   PAGE_GRAIN, -1, 1, 0, FMT_BIPOLAR, 1),
    [P_BPM]        = C("bpm",          "Tempo",        PAGE_GLOBAL, 30, 300, 120, FMT_FLOAT, 1),
    [P_GRAIN_LVL]  = C("grain_level",  "Grain Level",  PAGE_GRAIN, 0, 1.5f, 1.0f, FMT_PCT, 1),

    [P_KEY]        = D("key",          "Key",          PAGE_PLAY, 0, 11, 0, FMT_ENUM, GS_NOTE_NAMES),
    [P_SCALE]      = D("scale",        "Scale",        PAGE_PLAY, 0, 8, 1, FMT_ENUM, SCALE_LABELS),
    [P_SAMPLE_ROOT]= D("sample_root",  "Sample Root",  PAGE_PLAY, 0, 11, 0, FMT_ENUM, GS_NOTE_NAMES),
    [P_OCTAVE]     = D("octave",       "Octave",       PAGE_PLAY, -2, 1, 0, FMT_INT, 0),
    [P_CHORD]      = D("chord",        "Chord Type",   PAGE_PLAY, 0, 9, 5, FMT_ENUM, CHORD_LABELS),
    [P_VOICING]    = C("voicing",      "Voicing",      PAGE_PLAY, 0, 1, 0.3f, FMT_PCT, 1),

    [P_SLICES]     = D("slices",       "Slices",       PAGE_SAMPLE, 0, 4, 0, FMT_ENUM, SLICE_COUNT_LABELS),
    [P_SLICE_MODE] = D("slice_mode",   "Slice Mode",   PAGE_SAMPLE, 0, 1, 0, FMT_ENUM, SLICE_MODES),
    [P_SLICE_SEL]  = D("slice_sel",    "Slice",        PAGE_SAMPLE, 0, 15, 0, FMT_INT, 0),
    [P_SLICE_SPAN] = D("slice_span",   "Slice Span",   PAGE_SAMPLE, 1, 16, 16, FMT_INT, 0),

    [P_DRV_AMT]    = C("drv_amount",   "Drive",        PAGE_FX, 0, 1, 0.25f, FMT_PCT, 1),
    [P_DRV_TYPE]   = D("drv_type",     "Drive Type",   PAGE_FX, 0, 2, 0, FMT_ENUM, DRV_TYPES),
    [P_DRV_TONE]   = C("drv_tone",     "Drive Tone",   PAGE_FX, 0, 1, 0.6f, FMT_PCT, 1),
    [P_DRV_MIX]    = C("drv_mix",      "Drive Mix",    PAGE_FX, 0, 1, 0.6f, FMT_PCT, 1),
    [P_CMP_THR]    = C("cmp_thresh",   "Comp Thresh",  PAGE_FX, -40, 0, -18, FMT_DB, 1),
    [P_CMP_RATIO]  = C("cmp_ratio",    "Comp Ratio",   PAGE_FX, 1, 20, 3, FMT_RATIO, 2),
    [P_CMP_ATK]    = C("cmp_attack",   "Comp Attack",  PAGE_FX, 0.1f, 100, 10, FMT_MS, 3),
    [P_CMP_REL]    = C("cmp_release",  "Comp Release", PAGE_FX, 10, 1000, 150, FMT_MS, 2),
    [P_CMP_MAKEUP] = C("cmp_makeup",   "Comp Makeup",  PAGE_FX, 0, 24, 3, FMT_DB, 1),
    [P_CMP_MIX]    = C("cmp_mix",      "Comp Mix",     PAGE_FX, 0, 1, 1, FMT_PCT, 1),
    [P_DLY_TIME]   = C("dly_time",     "Delay Time",   PAGE_FX, 10, 2000, 375, FMT_MS, 2),
    [P_DLY_DIV]    = D("dly_sync",     "Delay Sync",   PAGE_FX, 0, 7, 3, FMT_ENUM, GS_DLY_DIV_LABELS),
    [P_DLY_FB]     = C("dly_feedback", "Delay Fdbk",   PAGE_FX, 0, 0.98f, 0.45f, FMT_PCT, 1),
    [P_DLY_DAMP]   = C("dly_damp",     "Delay Damp",   PAGE_FX, 0, 1, 0.4f, FMT_PCT, 1),
    [P_DLY_PING]   = C("dly_pingpong", "Ping-Pong",    PAGE_FX, 0, 1, 0.6f, FMT_PCT, 1),
    [P_DLY_MIX]    = C("dly_mix",      "Delay Mix",    PAGE_FX, 0, 1, 0.3f, FMT_PCT, 1),
    [P_REV_SIZE]   = C("rev_decay",    "Reverb Decay", PAGE_FX, 0, 1, 0.6f, FMT_SEC, 1),
    [P_REV_DAMP]   = C("rev_damp",     "Reverb Damp",  PAGE_FX, 0, 1, 0.35f, FMT_PCT, 1),
    [P_REV_WIDTH]  = C("rev_width",    "Reverb Width", PAGE_FX, 0, 1.5f, 1.2f, FMT_PCT, 1),
    [P_REV_PRE]    = C("rev_predelay", "Pre-Delay",    PAGE_FX, 0, 200, 30, FMT_MS, 1),
    [P_REV_MIX]    = C("rev_mix",      "Reverb Mix",   PAGE_FX, 0, 1, 0.4f, FMT_PCT, 1),
    [P_FX_ORDER]   = D("fx_order",     "FX Order",     PAGE_FX, 0, 23, 0, FMT_ENUM, GS_FX_ORDER_LABELS),

    [P_MASTER]     = C("master",       "Master",       PAGE_PLAY, -40, 18, 0, FMT_DB, 1),

    [P_TRIM_IN]    = C("cut_in",       "Cut In",       PAGE_SAMPLE, 0, 1, 0, FMT_PCT, 1),
    [P_TRIM_OUT]   = C("cut_out",      "Cut Out",      PAGE_SAMPLE, 0, 1, 1, FMT_PCT, 1),
    [P_DRV_ON]     = D("drv_on",       "Drive On",     PAGE_FX, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_CMP_ON]     = D("cmp_on",       "Comp On",      PAGE_FX, 0, 1, 1, FMT_ENUM, ONOFF),
    [P_DLY_ON]     = D("dly_on",       "Delay On",     PAGE_FX, 0, 1, 1, FMT_ENUM, ONOFF),
    [P_REV_ON]     = D("rev_on",       "Reverb On",    PAGE_FX, 0, 1, 1, FMT_ENUM, ONOFF),
    [P_REV_MOD]    = C("rev_mod",      "Reverb Mod",   PAGE_FX, 0, 1, 0.5f, FMT_PCT, 1),

    [P_SRC]        = D("input",        "Input",        PAGE_SAMPLE, 0, 1, 0, FMT_ENUM, SRC_LABELS),
    [P_IMG_SOURCE] = D("img_source",   "Sound Source", PAGE_IMAGE, 0, 2, 1, FMT_ENUM, IMG_SOURCE_LABELS),
    [P_IMG_MODE]   = D("img_mode",     "Image Mode",   PAGE_IMAGE, 0, 2, 0, FMT_ENUM, IMG_MODE_LABELS),
    [P_IMG_READ]   = D("img_listen",   "Listen To",    PAGE_IMAGE, 0, 1, 1, FMT_ENUM, IMG_READ_LABELS),
    [P_IMG_KEY]    = D("img_key",      "Image Key",    PAGE_IMAGE, 0, 11, 9, FMT_ENUM, GS_NOTE_NAMES),
    [P_IMG_SCALE]  = D("img_scale",    "Image Scale",  PAGE_IMAGE, 0, 7, 0, FMT_ENUM, GS_IMG_SCALE_NAMES),
    [P_IMG_LOW]    = D("img_low",      "Lowest Note",  PAGE_IMAGE, 24, 72, 45, FMT_INT, 0),
    [P_IMG_RANGE]  = D("img_range",    "Range",        PAGE_IMAGE, 1, 5, 3, FMT_INT, 0),
    [P_IMG_GRID]   = D("img_steps",    "Grid Steps",   PAGE_IMAGE, 0, 128, 16, FMT_INT, 0),
    [P_IMG_VOICES] = D("img_voices",   "Voices/Step",  PAGE_IMAGE, 1, 8, 4, FMT_INT, 0),
    [P_IMG_WAVE]   = D("img_wave",     "Waveform",     PAGE_IMAGE, 0, 3, 1, FMT_ENUM, IMG_WAVE_LABELS),
    [P_IMG_DECAY]  = C("img_decay",    "Note Decay",   PAGE_IMAGE, 0, 1, 0.55f, FMT_PCT, 1),
    [P_IMG_SWEEP]  = C("img_sweep",    "Sweep Time",   PAGE_IMAGE, 1, 60, 8, FMT_SEC, 2),
    [P_IMG_SYNC]   = D("img_sync",     "Sweep Sync",   PAGE_IMAGE, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_IMG_LEVEL]  = C("img_level",    "Image Level",  PAGE_IMAGE, 0, 1.5f, 0.8f, FMT_PCT, 1),

    [P_ARP_ON]      = D("arp_on",      "Arp On",       PAGE_ARP, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_ARP_MODE]    = D("arp_pattern_mode", "Arp Pattern", PAGE_ARP, 0, 4, 0, FMT_ENUM, GS_ARP_MODE_NAMES),
    [P_ARP_RATE]    = D("arp_rate",    "Arp Rate",     PAGE_ARP, 0, 5, 3, FMT_ENUM, GS_ARP_RATE_NAMES),
    [P_ARP_OCT]     = D("arp_oct",     "Arp Octaves",  PAGE_ARP, 1, 4, 2, FMT_INT, 0),
    [P_ARP_GATE]    = C("arp_gate",    "Arp Gate",     PAGE_ARP, 0.05f, 1, 0.6f, FMT_PCT, 1),
    [P_ARP_SWING]   = C("arp_swing",   "Arp Swing",    PAGE_ARP, 0, 0.6f, 0, FMT_PCT, 1),
    [P_ARP_ATTACK]  = C("arp_attack",  "Arp Attack",   PAGE_ARP, 1, 300, 4, FMT_MS, 2.5f),
    [P_ARP_RELEASE] = C("arp_release", "Arp Release",  PAGE_ARP, 5, 1500, 120, FMT_MS, 2.5f),
    [P_ARP_START]   = C("arp_start",   "Arp Start",    PAGE_ARP, 0, 1, 0, FMT_PCT, 1),
    [P_ARP_WALK]    = C("arp_walk",    "Arp Walk",     PAGE_ARP, 0, 0.5f, 0, FMT_PCT, 1.5f),
    [P_ARP_GRAINS]  = C("arp_grains",  "Grains Under", PAGE_ARP, 0, 1, 0.25f, FMT_PCT, 1),
    [P_ARP_LEVEL]   = C("arp_level",   "Arp Level",    PAGE_ARP, 0, 1.5f, 1.0f, FMT_PCT, 1),
    [P_ARP_LEN]     = D("arp_length",  "Arp Length",   PAGE_ARP, 1, 16, 16, FMT_INT, 0),
    [P_ARP_PATTERN] = D("arp_lane",    "Arp Lane",     PAGE_ARP, 0, 65535, 65535, FMT_INT, 0),
    [P_IMG_DRY]     = C("img_dry",     "Image Dry",    PAGE_IMAGE, 0, 1.5f, 0.5f, FMT_PCT, 1),
    [P_GRAIN_ON]    = D("grain_on",    "Grains On",    PAGE_GRAIN, 0, 1, 1, FMT_ENUM, ONOFF),

    [P_GRAIN_HOLD]  = D("grain_hold",  "Grain Hold",   PAGE_GRAIN, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_GRAIN_INTERP]= D("grain_interp","Interpolation",PAGE_GRAIN, 0, 1, 1, FMT_ENUM, INTERP_LABELS),
    [P_IMG_HOLD]    = D("img_hold",    "Image Hold",   PAGE_IMAGE, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_DRV_OS]      = D("drv_os",      "Drive Oversample", PAGE_FX, 0, 1, 1, FMT_ENUM, DRV_OS_LABELS),
    [P_DEG1]        = D("deg1",        "Chord I",      PAGE_PLAY, 0, 1, 1, FMT_ENUM, ONOFF),
    [P_DEG2]        = D("deg2",        "Chord II",     PAGE_PLAY, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_DEG3]        = D("deg3",        "Chord III",    PAGE_PLAY, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_DEG4]        = D("deg4",        "Chord IV",     PAGE_PLAY, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_DEG5]        = D("deg5",        "Chord V",      PAGE_PLAY, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_DEG6]        = D("deg6",        "Chord VI",     PAGE_PLAY, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_DEG7]        = D("deg7",        "Chord VII",    PAGE_PLAY, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_MIDI_MODE]   = D("midi_mode",   "MIDI Mode",    PAGE_PLAY, 0, 1, 0, FMT_ENUM, MIDI_MODE_LABELS),
    [P_LATCH]       = D("latch",       "Latch",        PAGE_PLAY, 0, 1, 1, FMT_ENUM, ONOFF),
    [P_VEL_AMT]     = C("vel_amount",  "Velocity Amount", PAGE_PLAY, 0, 1, 0.3f, FMT_PCT, 1),
    [P_MOD_DEST]    = D("modwheel_dest","Mod Wheel",   PAGE_PLAY, 0, 9, 1, FMT_ENUM, GS_MOD_DEST_LABELS),
    [P_PLAY]        = D("play",        "Play",         PAGE_GLOBAL, 0, 1, 1, FMT_ENUM, PLAY_LABELS),
    [P_FOLLOW_HOST] = D("follow_host", "Follow Host Transport", PAGE_GLOBAL, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_TEMPO_SRC]   = D("tempo_source","Tempo Source", PAGE_GLOBAL, 0, 1, 0, FMT_ENUM, TEMPO_SRC_LABELS),
    [P_PREVIEW_LOOP]= D("preview_loop","Audition Loop",PAGE_SAMPLE, 0, 1, 0, FMT_ENUM, ONOFF),
    [P_IMG_RES]     = D("img_res",     "Grid Resolution", PAGE_IMAGE, 0, 5, 3, FMT_ENUM, IMG_RES_LABELS),
    [P_DRY_LVL]     = C("dry",         "Dry",          PAGE_PLAY, 0, 1.5f, 0, FMT_PCT, 1),
    [P_WET_LVL]     = C("wet",         "Wet",          PAGE_PLAY, 0, 1.5f, 1, FMT_PCT, 1),
    [P_REV_MODE]    = D("rev_mode",    "Reverb Mode",  PAGE_FX, 0, 1, 0, FMT_ENUM, REV_MODE_LABELS),
};

const char *const GS_PARAM_SHORT[P_COUNT] = {
    [P_MODE] = "MODE", [P_SIZE] = "SIZE", [P_DENSITY] = "DENSITY", [P_POSITION] = "POSITION", [P_SPRAY] = "SPRAY",
    [P_SCAN] = "SCAN", [P_REVERSE] = "REVERSE", [P_SPREAD] = "STEREO", [P_OCTAVES] = "OCT JUMP", [P_DETUNE] = "DETUNE",
    [P_GLIDE] = "GLIDE", [P_TONE] = "TONE", [P_BPM] = "BPM", [P_GRAIN_LVL] = "LEVEL",
    [P_KEY] = "KEY", [P_SCALE] = "SCALE", [P_SAMPLE_ROOT] = "ROOT", [P_OCTAVE] = "OCT", [P_CHORD] = "CHORD",
    [P_VOICING] = "VOICING", [P_SLICES] = "SLICES", [P_SLICE_MODE] = "SLICE BY", [P_SLICE_SEL] = "SLICE",
    [P_SLICE_SPAN] = "SPAN", [P_DRV_AMT] = "AMOUNT", [P_DRV_TYPE] = "TYPE", [P_DRV_TONE] = "TONE",
    [P_DRV_MIX] = "MIX", [P_CMP_THR] = "THRESH", [P_CMP_RATIO] = "RATIO", [P_CMP_ATK] = "ATTACK",
    [P_CMP_REL] = "RELEASE", [P_CMP_MAKEUP] = "MAKEUP", [P_CMP_MIX] = "MIX", [P_DLY_TIME] = "TIME",
    [P_DLY_DIV] = "SYNC", [P_DLY_FB] = "FDBK", [P_DLY_DAMP] = "DAMP", [P_DLY_PING] = "PING",
    [P_DLY_MIX] = "MIX", [P_REV_SIZE] = "DECAY", [P_REV_DAMP] = "DAMP", [P_REV_WIDTH] = "WIDTH",
    [P_REV_PRE] = "PRE", [P_REV_MIX] = "MIX", [P_FX_ORDER] = "ORDER", [P_MASTER] = "MASTER", [P_TRIM_IN] = "IN",
    [P_TRIM_OUT] = "OUT", [P_DRV_ON] = "ON", [P_CMP_ON] = "ON", [P_DLY_ON] = "ON", [P_REV_ON] = "ON", [P_REV_MOD] = "MOD",
    [P_SRC] = "INPUT", [P_IMG_SOURCE] = "SOUND SOURCE", [P_IMG_MODE] = "MODE", [P_IMG_READ] = "LISTEN TO",
    [P_IMG_KEY] = "KEY", [P_IMG_SCALE] = "SCALE", [P_IMG_LOW] = "LOW", [P_IMG_RANGE] = "RANGE",
    [P_IMG_GRID] = "STEPS", [P_IMG_VOICES] = "VOICES", [P_IMG_WAVE] = "WAVE", [P_IMG_DECAY] = "DECAY",
    [P_IMG_SWEEP] = "SWEEP", [P_IMG_SYNC] = "SYNC", [P_IMG_LEVEL] = "LEVEL",
    [P_ARP_ON] = "ARP", [P_ARP_MODE] = "PATTERN", [P_ARP_RATE] = "RATE", [P_ARP_OCT] = "OCT", [P_ARP_GATE] = "GATE",
    [P_ARP_SWING] = "SWING", [P_ARP_ATTACK] = "ATTACK", [P_ARP_RELEASE] = "RELEASE", [P_ARP_START] = "START",
    [P_ARP_WALK] = "WALK", [P_ARP_GRAINS] = "GRAINS UNDER", [P_ARP_LEVEL] = "LEVEL", [P_ARP_LEN] = "LENGTH",
    [P_ARP_PATTERN] = "LANE", [P_IMG_DRY] = "DRY", [P_GRAIN_ON] = "GRAINS",
    [P_GRAIN_HOLD] = "HOLD", [P_GRAIN_INTERP] = "INTERP", [P_IMG_HOLD] = "HOLD", [P_DRV_OS] = "OS",
    [P_DEG1] = "I", [P_DEG2] = "II", [P_DEG3] = "III", [P_DEG4] = "IV", [P_DEG5] = "V", [P_DEG6] = "VI", [P_DEG7] = "VII",
    [P_MIDI_MODE] = "MIDI", [P_LATCH] = "LATCH", [P_VEL_AMT] = "VELOCITY", [P_MOD_DEST] = "MOD WHEEL",
    [P_PLAY] = "PLAY", [P_FOLLOW_HOST] = "FOLLOW HOST", [P_TEMPO_SRC] = "TEMPO", [P_PREVIEW_LOOP] = "LOOP", [P_IMG_RES] = "GRID RES", [P_DRY_LVL] = "DRY", [P_WET_LVL] = "WET", [P_REV_MODE] = "MODE",
};

int gs_mod_dest_param(int dest)
{
    static const int map[GS_NUM_MOD_DESTS] = {-1, P_SPRAY, P_POSITION, P_SIZE, P_DENSITY, P_SCAN, P_TONE,
                                              P_REV_MIX, P_DLY_FB, P_DRV_AMT};
    return (dest >= 0 && dest < GS_NUM_MOD_DESTS) ? map[dest] : -1;
}

int gs_param_num_steps(int id)
{
    const GsParamDesc *d = &GS_PARAMS[id];
    return (int)lroundf(d->max - d->min) + 1;
}

int gs_param_find(const char *id)
{
    for (int i = 0; i < P_COUNT; i++)
        if (GS_PARAMS[i].id && !strcmp(GS_PARAMS[i].id, id)) return i;
    return -1;
}

float gs_param_clamp(int id, float v)
{
    const GsParamDesc *d = &GS_PARAMS[id];
    if (!(v == v)) v = d->def;                /* NaN */
    if (d->discrete) v = floorf(v + 0.5f);
    if (v < d->min) v = d->min;
    if (v > d->max) v = d->max;
    return v;
}

float gs_param_from_norm(int id, float n)
{
    const GsParamDesc *d = &GS_PARAMS[id];
    if (!(n == n)) n = 0;
    if (n < 0) n = 0;
    if (n > 1) n = 1;
    if (d->curve != 1.0f) n = powf(n, d->curve);
    return gs_param_clamp(id, d->min + n * (d->max - d->min));
}

float gs_param_to_norm(int id, float v)
{
    const GsParamDesc *d = &GS_PARAMS[id];
    float n = (v - d->min) / (d->max - d->min);
    if (!(n == n)) n = 0;
    if (n < 0) n = 0;
    if (n > 1) n = 1;
    if (d->curve != 1.0f) n = powf(n, 1.0f / d->curve);
    return n;
}

void gs_param_format(int id, float v, char *buf, int buflen)
{
    const GsParamDesc *d = &GS_PARAMS[id];
    if (buflen <= 0) return;
    if (id == P_SLICE_SEL) { snprintf(buf, buflen, "%d", (int)lroundf(v) + 1); return; }  /* 1-based */
    if (id == P_IMG_LOW) {                                    /* MIDI note name */
        int m = (int)lroundf(v);
        snprintf(buf, buflen, "%s%d", GS_NOTE_NAMES[((m % 12) + 12) % 12], m / 12 - 1);
        return;
    }
    if (id == P_IMG_RANGE) { snprintf(buf, buflen, "%d oct", (int)lroundf(v)); return; }
    if (id == P_IMG_GRID) {
        int st = (int)lroundf(v);
        if (st <= 0) snprintf(buf, buflen, "Smooth");
        else snprintf(buf, buflen, "%d", st);
        return;
    }
    if (id == P_OCTAVE) { snprintf(buf, buflen, "%+d", (int)lroundf(v)); return; }
    if (id == P_BPM) { snprintf(buf, buflen, "%.1f BPM", v); return; }
    if (id == P_REV_SIZE) {                                   /* show the decay time */
        if (v >= GS_REV_INFINITE) snprintf(buf, buflen, "Infinite");
        else snprintf(buf, buflen, "%.1f s", gs_reverb_rt60(v));
        return;
    }
    switch (d->fmt) {
    case FMT_PCT:    snprintf(buf, buflen, "%d%%", (int)lroundf(v * 100)); break;
    case FMT_MS:     if (v < 10) snprintf(buf, buflen, "%.1f ms", v);
                     else snprintf(buf, buflen, "%d ms", (int)lroundf(v));
                     break;
    case FMT_INT:    snprintf(buf, buflen, "%d", (int)lroundf(v)); break;
    case FMT_DB:     snprintf(buf, buflen, "%.1f dB", v); break;
    case FMT_ENUM: {
        int i = (int)lroundf(v - d->min);
        int n = gs_param_num_steps(id);
        if (i < 0) i = 0;
        if (i >= n) i = n - 1;
        snprintf(buf, buflen, "%s", d->labels[i]);
        break; }
    case FMT_BIPOLAR: snprintf(buf, buflen, "%+d", (int)lroundf(v * 100)); break;
    case FMT_RATIO:  snprintf(buf, buflen, "%.1f:1", v); break;
    case FMT_HZ:     snprintf(buf, buflen, "%.1f Hz", v); break;
    case FMT_CENTS:  snprintf(buf, buflen, "%d ct", (int)lroundf(v)); break;
    case FMT_SEC:    snprintf(buf, buflen, "%.1f s", v); break;
    default:         snprintf(buf, buflen, "%.2f", v); break;
    }
}

static int ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
        a++; b++;
    }
    return *a == *b;
}

static void trim_copy(const char *in, char *out, int max)
{
    while (*in && isspace((unsigned char)*in)) in++;
    int n = 0;
    while (in[n] && n < max - 1) { out[n] = in[n]; n++; }
    out[n] = 0;
    while (n > 0 && isspace((unsigned char)out[n - 1])) out[--n] = 0;
}

float gs_param_parse(int id, const char *text)
{
    const GsParamDesc *d = &GS_PARAMS[id];
    char t[64];
    trim_copy(text ? text : "", t, sizeof t);
    if (d->fmt == FMT_ENUM) {
        int n = gs_param_num_steps(id);
        for (int i = 0; i < n; i++)
            if (ieq(t, d->labels[i])) return d->min + i;
        if (n == 2 && (ieq(t, "on") || ieq(t, "yes") || ieq(t, "true"))) return d->min + 1;
        if (n == 2 && (ieq(t, "off") || ieq(t, "no") || ieq(t, "false"))) return d->min;
        char *end;
        double x = strtod(t, &end);
        if (end != t) return gs_param_clamp(id, (float)x);
        return d->def;
    }
    if (id == P_IMG_LOW) {
        for (int pc = 11; pc >= 0; pc--) {                     /* longest names first ("C#" before "C") */
            size_t l = strlen(GS_NOTE_NAMES[pc]);
            if (!strncmp(t, GS_NOTE_NAMES[pc], l) && t[l] && (isdigit((unsigned char)t[l]) || t[l] == '-')) {
                int oct = atoi(t + l);
                return gs_param_clamp(id, (float)((oct + 1) * 12 + pc));
            }
        }
    }
    if (id == P_REV_SIZE && (ieq(t, "inf") || ieq(t, "infinite"))) return 1.0f;
    if (id == P_IMG_GRID && (ieq(t, "smooth") || ieq(t, "off"))) return 0.0f;
    char *end;
    double x = strtod(t, &end);
    if (end == t) return d->def;
    float v = (float)x;
    if (id == P_SLICE_SEL) v -= 1.0f;
    else if (id == P_REV_SIZE) v = gs_reverb_size_for_rt60(v);
    else if (d->fmt == FMT_PCT || d->fmt == FMT_BIPOLAR) {
        /* "45%" or "45" -> 0.45; a plain value inside the native range is taken as-is ("0.45") */
        int has_pct = strchr(t, '%') != NULL;
        if (has_pct || fabsf(v) > fmaxf(fabsf(d->max), fabsf(d->min)) || d->fmt == FMT_BIPOLAR) v *= 0.01f;
    }
    return gs_param_clamp(id, v);
}
