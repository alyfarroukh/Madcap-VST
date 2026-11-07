// The picture behind the image sweeper: a 512 x 256 cover-fitted copy (shown on the
// IMAGE tab and stored in the project as PNG), or one of the built-in posters.
#pragma once
#include <juce_graphics/juce_graphics.h>

#include <atomic>
#include <memory>
#include <vector>

namespace madcap {

struct PictureData {
    static constexpr int kW = 512, kH = 256;
    juce::Image image;                   // kW x kH, software image
    std::vector<float> luma;             // IMG_ROWS x IMG_COLS grid for the engine / analysis view
    int poster = 0;                      // >= 0: built-in poster, -1: user picture
    juce::String name;
};

class ImageStore {
public:
    void setPoster(int index);
    // pixels: kW x kH 0xAABBGGRR (any thread)
    void setPicture(const std::vector<uint32_t>& abgr, const juce::String& name);
    bool setFromPng(const juce::MemoryBlock& png, const juce::String& name);

    std::shared_ptr<const PictureData> get() const;
    int version() const noexcept { return version_.load(); }
    juce::MemoryBlock png() const;       // only for user pictures

    static juce::String decodeFile(const juce::File& f, std::vector<uint32_t>& abgr);
    static juce::StringArray supportedExtensions();

private:
    void install(std::shared_ptr<PictureData> d, juce::MemoryBlock png);
    mutable juce::CriticalSection lock_;
    std::shared_ptr<const PictureData> data_;
    juce::MemoryBlock png_;
    std::atomic<int> version_{0};
};

} // namespace madcap
