#include "Presets.h"

extern "C" {
#include "params.h"
}

namespace madcap {

namespace {
constexpr double lane(unsigned bits) { return (double)bits; }
}

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        { "Choir Cloud", {
            {P_SRC, 0}, {P_MODE, MODE_CLOUD}, {P_SIZE, 320}, {P_DENSITY, 18}, {P_SPRAY, 0.35f}, {P_SCAN, 0.1f},
            {P_REVERSE, 0.3f}, {P_SPREAD, 0.9f}, {P_OCTAVES, 0.2f}, {P_DETUNE, 12}, {P_CHORD, 5}, {P_VOICING, 0.7f},
            {P_CMP_ON, 1}, {P_DLY_ON, 1}, {P_DLY_DIV, 4}, {P_DLY_MIX, 0.2f}, {P_REV_ON, 1}, {P_REV_SIZE, 0.75f},
            {P_REV_MIX, 0.5f}, {P_REV_MOD, 0.6f} } },
        { "Glass Mosaic", {
            {P_SRC, 0}, {P_MODE, MODE_MOSAIC}, {P_SIZE, 90}, {P_DENSITY, 24}, {P_SPRAY, 0.5f}, {P_OCTAVES, 0.35f},
            {P_TONE, 0.35f}, {P_CHORD, 8}, {P_VOICING, 0.6f}, {P_DLY_ON, 1}, {P_DLY_DIV, 2}, {P_DLY_FB, 0.5f},
            {P_DLY_MIX, 0.35f}, {P_DLY_PING, 1.0f}, {P_REV_SIZE, 0.55f}, {P_REV_MIX, 0.35f} } },
        { "Tunnel Drone", {
            {P_SRC, 0}, {P_MODE, MODE_TUNNEL}, {P_SIZE, 900}, {P_DENSITY, 4}, {P_SPRAY, 0.25f}, {P_SCAN, 0.05f},
            {P_REVERSE, 0.5f}, {P_CHORD, 4}, {P_OCTAVE, -1}, {P_TONE, -0.4f}, {P_REV_SIZE, 0.9f}, {P_REV_MIX, 0.6f},
            {P_REV_MOD, 0.8f}, {P_DLY_ON, 0} } },
        { "Strum Pad", {
            {P_SRC, 0}, {P_MODE, MODE_STRUM}, {P_DENSITY, 4}, {P_SIZE, 400}, {P_CHORD, 8}, {P_VOICING, 0.6f},
            {P_DLY_DIV, 4}, {P_DLY_MIX, 0.3f}, {P_REV_SIZE, 0.7f}, {P_REV_MIX, 0.45f} } },
        { "Arp Shimmer", {
            {P_SRC, 0}, {P_MODE, MODE_ARP}, {P_DENSITY, 16}, {P_SIZE, 120}, {P_CHORD, 9}, {P_OCTAVES, 0.3f},
            {P_TONE, 0.2f}, {P_REV_SIZE, 0.8f}, {P_REV_MIX, 0.5f}, {P_DLY_DIV, 3}, {P_DLY_MIX, 0.25f} } },
        { "Glide Choir", {
            {P_SRC, 0}, {P_MODE, MODE_GLIDE}, {P_GLIDE, 900}, {P_SIZE, 450}, {P_DENSITY, 10}, {P_CHORD, 6},
            {P_VOICING, 0.8f}, {P_SPRAY, 0.3f}, {P_REV_SIZE, 0.7f}, {P_REV_MIX, 0.45f} } },
        { "Infinite Haze", {
            {P_SRC, 0}, {P_MODE, MODE_CLOUD}, {P_SIZE, 700}, {P_DENSITY, 8}, {P_SPRAY, 0.6f}, {P_TONE, -0.3f},
            {P_REV_ON, 1}, {P_REV_SIZE, 1.0f}, {P_REV_MIX, 0.7f}, {P_REV_WIDTH, 1.5f}, {P_REV_MOD, 0.9f},
            {P_REV_DAMP, 0.5f}, {P_DLY_ON, 0}, {P_MASTER, 3} } },
        { "Tape Crush Pulse", {
            {P_SRC, 0}, {P_MODE, MODE_MOSAIC}, {P_SIZE, 140}, {P_DENSITY, 20}, {P_CHORD, 5}, {P_DRV_ON, 1},
            {P_DRV_TYPE, 2}, {P_DRV_AMT, 0.45f}, {P_DRV_MIX, 0.7f}, {P_DLY_DIV, 3}, {P_DLY_PING, 1}, {P_DLY_FB, 0.6f},
            {P_DLY_MIX, 0.4f}, {P_FX_ORDER, 2} } },
        { "Stab Arp", {
            {P_SRC, 0}, {P_ARP_ON, 1}, {P_ARP_MODE, 4}, {P_ARP_RATE, 1}, {P_ARP_GRAINS, 0.15f}, {P_ARP_GATE, 0.35f},
            {P_ARP_PATTERN, lane(0b1011011010110110u)}, {P_CHORD, 5}, {P_VOICING, 0.4f}, {P_DLY_DIV, 3},
            {P_DLY_MIX, 0.3f}, {P_REV_MIX, 0.3f} } },
        { "Sixteenth Walk", {
            {P_SRC, 0}, {P_ARP_ON, 1}, {P_ARP_MODE, 2}, {P_ARP_RATE, 3}, {P_ARP_OCT, 2}, {P_ARP_GATE, 0.4f},
            {P_ARP_SWING, 0.2f}, {P_ARP_WALK, 0.1f}, {P_ARP_START, 0.2f}, {P_CHORD, 8}, {P_ARP_GRAINS, 0.3f} } },
        { "Image Pent Melody", {
            {P_SRC, 1}, {P_IMG_MODE, 1}, {P_IMG_SCALE, 0}, {P_IMG_KEY, 9}, {P_IMG_GRID, 16}, {P_IMG_SOURCE, 1},
            {P_IMG_WAVE, 1}, {P_IMG_READ, 1}, {P_IMG_SWEEP, 8}, {P_IMG_DECAY, 0.6f}, {P_IMG_DRY, 0.9f},
            {P_GRAIN_LVL, 0.5f}, {P_CHORD, 0}, {P_REV_SIZE, 0.7f}, {P_REV_MIX, 0.45f} }, 1 },
        { "Height Sketch", {
            {P_SRC, 1}, {P_IMG_MODE, 2}, {P_IMG_GRID, 32}, {P_IMG_WAVE, 0}, {P_IMG_VOICES, 4}, {P_IMG_DECAY, 0.4f},
            {P_IMG_READ, 1}, {P_IMG_SWEEP, 10}, {P_IMG_DRY, 1.0f}, {P_GRAIN_LVL, 0.4f}, {P_CHORD, 0},
            {P_DLY_DIV, 2}, {P_DLY_MIX, 0.3f} }, 2 },
        { "Spectral Noise Field", {
            {P_SRC, 1}, {P_IMG_MODE, 0}, {P_IMG_SOURCE, 0}, {P_IMG_GRID, 0}, {P_IMG_VOICES, 6}, {P_IMG_READ, 1},
            {P_IMG_SWEEP, 20}, {P_IMG_DRY, 1.0f}, {P_GRAIN_ON, 1}, {P_MODE, MODE_TUNNEL}, {P_GRAIN_LVL, 0.6f},
            {P_REV_SIZE, 0.92f}, {P_REV_MIX, 0.6f}, {P_CHORD, 0} }, 0 },
        { "Image Grain Cloud", {
            {P_SRC, 1}, {P_IMG_MODE, 0}, {P_IMG_SOURCE, 2}, {P_IMG_GRID, 12}, {P_IMG_READ, 1}, {P_IMG_DRY, 0.3f},
            {P_MODE, MODE_CLOUD}, {P_SIZE, 260}, {P_DENSITY, 16}, {P_GRAIN_LVL, 1.0f}, {P_CHORD, 4},
            {P_REV_SIZE, 0.75f}, {P_REV_MIX, 0.5f} }, 3 },
    };
    return presets;
}

bool presetKeepsParameter(int p)
{
    switch (p) {
        case P_PLAY: case P_FOLLOW_HOST: case P_TEMPO_SRC: case P_BPM:
        case P_MIDI_MODE: case P_LATCH: case P_VEL_AMT: case P_MOD_DEST:
        case P_TRIM_IN: case P_TRIM_OUT: case P_SAMPLE_ROOT: case P_PREVIEW_LOOP:
            return true;
        default:
            return false;
    }
}

juce::File userPresetDirectory()
{
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile("Application Support");
   #endif
    return base.getChildFile("Madcap").getChildFile("Presets");
}

juce::Array<juce::File> userPresetFiles()
{
    auto dir = userPresetDirectory();
    juce::Array<juce::File> files;
    if (dir.isDirectory())
        files = dir.findChildFiles(juce::File::findFiles, false, juce::String("*") + presetExtension());
    files.sort();
    return files;
}

} // namespace madcap
