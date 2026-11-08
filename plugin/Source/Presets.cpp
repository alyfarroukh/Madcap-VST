#include "Presets.h"

extern "C" {
#include "params.h"
}

namespace madcap {

namespace {
constexpr double lane(unsigned bits) { return (double)bits; }
}

const char* presetCategoryName(int category) { return category == PresetImage ? "Image" : "Sample"; }

const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets = {
        // ------------------------------------------------------------------ SAMPLE
        { "Choir Cloud", PresetSample, {
            {P_MODE, MODE_CLOUD}, {P_SIZE, 320}, {P_DENSITY, 18}, {P_SPRAY, 0.35}, {P_SCAN, 0.1},
            {P_REVERSE, 0.3}, {P_SPREAD, 0.9}, {P_OCTAVES, 0.2}, {P_DETUNE, 12},
            {P_CMP_ON, 1}, {P_DLY_ON, 1}, {P_DLY_DIV, 4}, {P_DLY_MIX, 0.2}, {P_REV_ON, 1}, {P_REV_SIZE, 0.75},
            {P_REV_MIX, 0.5}, {P_REV_MOD, 0.6} } },
        { "Glass Mosaic", PresetSample, {
            {P_MODE, MODE_MOSAIC}, {P_SIZE, 90}, {P_DENSITY, 24}, {P_SPRAY, 0.5}, {P_OCTAVES, 0.35},
            {P_TONE, 0.35}, {P_DLY_ON, 1}, {P_DLY_DIV, 2}, {P_DLY_FB, 0.5},
            {P_DLY_MIX, 0.35}, {P_DLY_PING, 1.0}, {P_REV_SIZE, 0.55}, {P_REV_MIX, 0.35} } },
        { "Tunnel Drone", PresetSample, {
            {P_MODE, MODE_TUNNEL}, {P_SIZE, 900}, {P_DENSITY, 4}, {P_SPRAY, 0.25}, {P_SCAN, 0.05},
            {P_REVERSE, 0.5}, {P_TONE, -0.4}, {P_REV_SIZE, 0.9}, {P_REV_MIX, 0.6},
            {P_REV_MOD, 0.8}, {P_DLY_ON, 0} } },
        { "Strum Pad", PresetSample, {
            {P_MODE, MODE_STRUM}, {P_DENSITY, 4}, {P_SIZE, 400},
            {P_DLY_DIV, 4}, {P_DLY_MIX, 0.3}, {P_REV_SIZE, 0.7}, {P_REV_MIX, 0.45} } },
        { "Arp Shimmer", PresetSample, {
            {P_MODE, MODE_ARP}, {P_DENSITY, 16}, {P_SIZE, 120}, {P_OCTAVES, 0.3},
            {P_TONE, 0.2}, {P_REV_SIZE, 0.8}, {P_REV_MIX, 0.5}, {P_DLY_DIV, 3}, {P_DLY_MIX, 0.25} } },
        { "Glide Choir", PresetSample, {
            {P_MODE, MODE_GLIDE}, {P_GLIDE, 900}, {P_SIZE, 450}, {P_DENSITY, 10},
            {P_SPRAY, 0.3}, {P_REV_SIZE, 0.7}, {P_REV_MIX, 0.45} } },
        { "Infinite Haze", PresetSample, {
            {P_MODE, MODE_CLOUD}, {P_SIZE, 700}, {P_DENSITY, 8}, {P_SPRAY, 0.6}, {P_TONE, -0.3},
            {P_REV_ON, 1}, {P_REV_SIZE, 1.0}, {P_REV_MIX, 0.7}, {P_REV_WIDTH, 1.5}, {P_REV_MOD, 0.9},
            {P_REV_DAMP, 0.5}, {P_DLY_ON, 0} } },
        { "Smear Choir", PresetSample, {
            {P_MODE, MODE_CLOUD}, {P_SIZE, 500}, {P_DENSITY, 12}, {P_SPRAY, 0.45}, {P_DETUNE, 14},
            {P_REV_ON, 1}, {P_REV_MODE, 1}, {P_REV_SIZE, 0.8}, {P_REV_MIX, 0.75}, {P_REV_MOD, 0.7},
            {P_REV_WIDTH, 1.4}, {P_DLY_ON, 0}, {P_CMP_ON, 1} } },
        { "Tape Crush Pulse", PresetSample, {
            {P_MODE, MODE_MOSAIC}, {P_SIZE, 140}, {P_DENSITY, 20}, {P_DRV_ON, 1},
            {P_DRV_TYPE, 2}, {P_DRV_AMT, 0.45}, {P_DRV_MIX, 0.7}, {P_DLY_DIV, 3}, {P_DLY_PING, 1}, {P_DLY_FB, 0.6},
            {P_DLY_MIX, 0.4}, {P_FX_ORDER, 2} } },
        { "Stab Arp", PresetSample, {
            {P_ARP_ON, 1}, {P_ARP_MODE, 4}, {P_ARP_RATE, 1}, {P_ARP_GRAINS, 0.15}, {P_ARP_GATE, 0.35},
            {P_ARP_PATTERN, lane(0b1011011010110110u)}, {P_DLY_DIV, 3},
            {P_DLY_MIX, 0.3}, {P_REV_MIX, 0.3} } },
        { "Sixteenth Walk", PresetSample, {
            {P_ARP_ON, 1}, {P_ARP_MODE, 2}, {P_ARP_RATE, 3}, {P_ARP_OCT, 2}, {P_ARP_GATE, 0.4},
            {P_ARP_SWING, 0.2}, {P_ARP_WALK, 0.1}, {P_ARP_START, 0.2}, {P_ARP_GRAINS, 0.3} } },
        // ------------------------------------------------------------------ IMAGE
        { "Image Pent Melody", PresetImage, {
            {P_GRAIN_LVL, 0.5}, {P_MODE, MODE_CLOUD}, {P_SIZE, 220}, {P_DENSITY, 10},
            {P_REV_SIZE, 0.7}, {P_REV_MIX, 0.45}, {P_DLY_DIV, 4}, {P_DLY_MIX, 0.25} } },
        { "Height Sketch", PresetImage, {
            {P_GRAIN_LVL, 0.4}, {P_MODE, MODE_MOSAIC}, {P_SIZE, 120}, {P_DENSITY, 16},
            {P_DLY_DIV, 2}, {P_DLY_MIX, 0.3}, {P_DLY_PING, 1.0}, {P_REV_MIX, 0.3} } },
        { "Spectral Noise Field", PresetImage, {
            {P_GRAIN_ON, 1}, {P_MODE, MODE_TUNNEL}, {P_GRAIN_LVL, 0.6}, {P_SIZE, 800}, {P_DENSITY, 5},
            {P_REV_SIZE, 0.92}, {P_REV_MIX, 0.6}, {P_DLY_ON, 0} } },
        { "Image Grain Cloud", PresetImage, {
            {P_MODE, MODE_CLOUD}, {P_SIZE, 260}, {P_DENSITY, 16}, {P_GRAIN_LVL, 1.0}, {P_SPRAY, 0.4},
            {P_REV_SIZE, 0.75}, {P_REV_MIX, 0.5} } },
        { "Image Smear Pad", PresetImage, {
            {P_MODE, MODE_GLIDE}, {P_GLIDE, 1200}, {P_SIZE, 600}, {P_DENSITY, 9}, {P_GRAIN_LVL, 0.9},
            {P_REV_ON, 1}, {P_REV_MODE, 1}, {P_REV_SIZE, 0.85}, {P_REV_MIX, 0.8}, {P_REV_WIDTH, 1.5},
            {P_DLY_ON, 0} } },
        { "Image Arp Bells", PresetImage, {
            {P_GRAIN_LVL, 0.3}, {P_ARP_ON, 1}, {P_ARP_MODE, 2}, {P_ARP_RATE, 3}, {P_ARP_OCT, 3},
            {P_ARP_GATE, 0.3}, {P_ARP_RELEASE, 400}, {P_DLY_DIV, 3}, {P_DLY_MIX, 0.35}, {P_DLY_FB, 0.55},
            {P_REV_SIZE, 0.7}, {P_REV_MIX, 0.4} } },
        { "Image Fuzz Steps", PresetImage, {
            {P_MODE, MODE_MOSAIC}, {P_SIZE, 80}, {P_DENSITY, 20}, {P_DRV_ON, 1}, {P_DRV_TYPE, 1},
            {P_DRV_AMT, 0.5}, {P_DRV_MIX, 0.6}, {P_CMP_ON, 1}, {P_DLY_DIV, 1}, {P_DLY_MIX, 0.25} } },
    };
    return presets;
}

bool presetAppliesTo(int p)
{
    const int page = GS_PARAMS[p].page;
    return page == PAGE_GRAIN || page == PAGE_ARP || page == PAGE_FX;
}

juce::File userPresetDirectory()
{
    auto base = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    base = base.getChildFile("Application Support");
   #endif
    return base.getChildFile("Madcap").getChildFile("Presets");
}

juce::File userPresetDirectory(int category)
{
    return userPresetDirectory().getChildFile(presetCategoryName(category));
}

juce::Array<juce::File> userPresetFiles(int category)
{
    juce::Array<juce::File> files;
    const auto pattern = juce::String("*") + presetExtension();
    auto dir = userPresetDirectory(category);
    if (dir.isDirectory()) files = dir.findChildFiles(juce::File::findFiles, false, pattern);
    if (category == PresetSample && userPresetDirectory().isDirectory())   // presets saved before the split
        files.addArray(userPresetDirectory().findChildFiles(juce::File::findFiles, false, pattern));
    files.sort();
    return files;
}

} // namespace madcap
