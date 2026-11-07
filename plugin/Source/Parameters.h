#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

extern "C" {
#include "params.h"
}

namespace madcap::params {

// All parameters come from the shared table in dsp/src/params.c (GS_PARAMS),
// so the engine, the editor, presets and the host agree on names, ranges and units.
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

inline juce::String id(int p) { return GS_PARAMS[p].id; }
inline const char* pageName(int page)
{
    static const char* const names[] = {"Sample", "Image", "Grain", "Arp", "FX", "Play", "Global"};
    return page >= 0 && page < PAGE_COUNT ? names[page] : "Other";
}

juce::String format(int p, float value);

} // namespace madcap::params
