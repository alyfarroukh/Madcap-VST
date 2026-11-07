// The picture behind the image sweeper: the whole picture (never cropped), scaled down to
// fit kMaxW x kMaxH with its own aspect ratio, stored in the project as PNG; or one of the
// built-in posters. The analysis grid follows the picture's resolution (see lumaGridSize).
#pragma once
#include <juce_graphics/juce_graphics.h>

#include <atomic>
#include <memory>
#include <vector>

namespace madcap {

struct PictureData {
    static constexpr int kMaxW = 1024, kMaxH = 512;
    static constexpr int kPosterW = 512, kPosterH = 256;
    juce::Image image;                   // the picture (software image), any size up to kMaxW x kMaxH
    std::vector<float> luma;             // cols x rows grid for the engine / analysis / terrain views
    int cols = 0, rows = 0;
    int poster = 0;                      // >= 0: built-in poster, -1: user picture
    juce::String name;
};

class ImageStore {
public:
    void setPoster(int index);
    // pixels: w x h 0xAABBGGRR (any thread)
    void setPicture(const std::vector<uint32_t>& abgr, int w, int h, const juce::String& name);
    bool setFromPng(const juce::MemoryBlock& png, const juce::String& name);

    std::shared_ptr<const PictureData> get() const;
    int version() const noexcept { return version_.load(); }
    juce::MemoryBlock png() const;       // only for user pictures

    static juce::String decodeFile(const juce::File& f, std::vector<uint32_t>& abgr, int& w, int& h);
    static juce::StringArray supportedExtensions();

private:
    void install(std::shared_ptr<PictureData> d, juce::MemoryBlock png);
    mutable juce::CriticalSection lock_;
    std::shared_ptr<const PictureData> data_;
    juce::MemoryBlock png_;
    std::atomic<int> version_{0};
};

} // namespace madcap
