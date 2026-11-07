// The master copy of the loaded sample (mono, at its own rate) plus everything derived
// from it off the audio thread: the FLAC bytes stored in the project, and a waveform
// overview for the editor. Thread-safe; never touched by the audio thread.
#pragma once
#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <memory>
#include <vector>

namespace madcap {

struct SampleData {
    std::vector<float> samples;          // mono
    double rate = 48000.0;
    juce::String name;
    bool isDemo = false;
    std::vector<float> overviewMin, overviewMax;   // kOverview bins for drawing
    static constexpr int kOverview = 1024;
    double seconds() const { return rate > 0 ? (double)samples.size() / rate : 0.0; }
};

class SampleStore {
public:
    static constexpr double kMaxSeconds = 60.0;

    // Replace the sample (any thread). Builds the overview and the FLAC cache.
    void set(std::vector<float> mono, double rate, const juce::String& name, bool isDemo);
    std::shared_ptr<const SampleData> get() const;
    int version() const noexcept { return version_.load(); }

    // FLAC bytes of the current sample (empty for the built-in demo).
    juce::MemoryBlock flac() const;

    // Decode any supported audio file to mono (<= 60 s). Returns an error message or "".
    static juce::String decodeFile(const juce::File& file, std::vector<float>& mono, double& rate);
    static bool decodeFlac(const juce::MemoryBlock& data, std::vector<float>& mono, double& rate);
    static juce::MemoryBlock encodeFlac(const std::vector<float>& mono, double rate);
    static juce::StringArray supportedExtensions();

private:
    mutable juce::CriticalSection lock_;
    std::shared_ptr<const SampleData> data_;
    juce::MemoryBlock flac_;
    std::atomic<int> version_{0};
};

} // namespace madcap
