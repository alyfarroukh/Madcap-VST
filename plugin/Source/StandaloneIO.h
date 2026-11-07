// Access to the Standalone app's audio device (input picker for REC). Everything here is a
// no-op / nullptr inside a DAW, where the record input is the plugin's sidechain bus.
#pragma once
#include <juce_audio_devices/juce_audio_devices.h>

namespace madcap::standalone {

// The Standalone app's device manager, or nullptr when running as a plugin.
juce::AudioDeviceManager* deviceManager();

// JUCE's Standalone app mutes the audio input by default to avoid feedback. Madcap never
// sends its input to the output (it only records it), so REC turns the input back on.
void unmuteInput();

// Opens JUCE's audio settings dialog (device type, devices, channels, buffer size).
void showAudioSettings();

} // namespace madcap::standalone
