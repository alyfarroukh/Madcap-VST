// The picture behind the image sweeper: the whole picture (never cropped), scaled down to
// fit kMaxW x kMaxH with its own aspect ratio, stored in the project as PNG; or one of the
// built-in posters. The analysis grid follows the picture's resolution, up to the GRID RES
// setting (64 ... 2048 columns), and is rebuilt off the audio thread when GRID RES changes.
#pragma once
#include <juce_graphics/juce_graphics.h>

#include <atomic>
#include <memory>
#include <vector>

namespace madcap {

struct PictureData {
    static constexpr int kMaxW = 2048, kMaxH = 1024;
    static constexpr int kPosterW = 512, kPosterH = 256;
    std::vector<uint32_t> pixels;        // w x h 0xAABBGGRR (source for the grid)
    int w = 0, h = 0;
    juce::Image image;                   // display copy (software image, at most 1024 x 512)
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

    // GRID RES: maximum grid columns (rows = half). setMaxColumns only stores it (use before
    // loading); setResolution also rebuilds the current picture's grid (call off the audio thread).
    void setMaxColumns(int cols) noexcept { maxCols_.store(cols); }
    int maxColumns() const noexcept { return maxCols_.load(); }
    int gridColumnsBuiltFor() const noexcept { return builtFor_.load(); }
    void setResolution(int maxCols);

    std::shared_ptr<const PictureData> get() const;
    int version() const noexcept { return version_.load(); }
    juce::MemoryBlock png() const;       // only for user pictures

    static juce::String decodeFile(const juce::File& f, std::vector<uint32_t>& abgr, int& w, int& h);
    static juce::StringArray supportedExtensions();

private:
    std::shared_ptr<PictureData> makePicture(std::vector<uint32_t> px, int w, int h) const;
    void install(std::shared_ptr<PictureData> d, juce::MemoryBlock png, bool keepPng = false);
    mutable juce::CriticalSection lock_;
    std::shared_ptr<const PictureData> data_;
    juce::MemoryBlock png_;
    std::atomic<int> version_{0};
    std::atomic<int> maxCols_{512};
    std::atomic<int> builtFor_{0};
};

} // namespace madcap
