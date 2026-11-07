#include "StandaloneIO.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>

// Madcap defines createPluginFilter() in PluginProcessor.cpp
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace madcap::standalone {

juce::AudioDeviceManager* deviceManager()
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        return &holder->deviceManager;
    return nullptr;
}

void unmuteInput()
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->getMuteInputValue().setValue(false);
}

void showAudioSettings()
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        holder->showAudioSettingsDialog();
}

} // namespace madcap::standalone
