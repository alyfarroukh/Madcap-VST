#pragma once
#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

namespace madcap {

// Presets come in two categories, one per input. Loading a preset switches to its input and
// changes only the GRAIN, ARP and FX pages: never the sample, the picture, the cut, the
// IMAGE / SAMPLE settings, the harmony (PLAY tab) or the transport.
enum PresetCategory { PresetSample = 0, PresetImage = 1 };

struct FactoryPreset {
    const char* name;
    int category;                                 // PresetSample / PresetImage
    std::vector<std::pair<int, double>> values;   // parameter index -> value (on top of the defaults)
};

const std::vector<FactoryPreset>& factoryPresets();
const char* presetCategoryName(int category);

// Parameters a preset sets: everything on the GRAIN, ARP and FX pages.
bool presetAppliesTo(int p);

juce::File userPresetDirectory();
juce::File userPresetDirectory(int category);    // .../Presets/Sample or .../Presets/Image
juce::Array<juce::File> userPresetFiles(int category);   // (older presets in the root count as Sample)
inline const char* presetExtension() { return ".madcappreset"; }

} // namespace madcap
