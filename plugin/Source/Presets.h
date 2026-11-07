#pragma once
#include <juce_core/juce_core.h>

#include <utility>
#include <vector>

namespace madcap {

struct FactoryPreset {
    const char* name;
    std::vector<std::pair<int, double>> values;   // parameter index -> value (on top of the defaults)
    int poster = -1;                             // >= 0: load this built-in poster (image presets)
};

const std::vector<FactoryPreset>& factoryPresets();

// Parameters a preset never touches (transport, tempo, MIDI setup, the cut, the sample root).
bool presetKeepsParameter(int p);

juce::File userPresetDirectory();
juce::Array<juce::File> userPresetFiles();
inline const char* presetExtension() { return ".madcappreset"; }

} // namespace madcap
