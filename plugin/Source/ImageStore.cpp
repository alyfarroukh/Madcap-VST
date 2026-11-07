#include "ImageStore.h"
#include "ImageDecode.h"
#include "madcap/Resample.h"

namespace madcap {

namespace {

juce::Image imageFromABGR(const std::vector<uint32_t>& px, int w, int h)
{
    juce::Image img(juce::Image::RGB, w, h, false, juce::SoftwareImageType());
    juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const uint32_t p = px[(size_t)y * (size_t)w + (size_t)x];
            bd.setPixelColour(x, y, juce::Colour((juce::uint8)(p & 255), (juce::uint8)((p >> 8) & 255),
                                                 (juce::uint8)((p >> 16) & 255)));
        }
    return img;
}

} // namespace

void ImageStore::install(std::shared_ptr<PictureData> d, juce::MemoryBlock png)
{
    const juce::ScopedLock sl(lock_);
    data_ = std::move(d);
    png_ = std::move(png);
    ++version_;
}

void ImageStore::setPoster(int index)
{
    index = ((index % posterCount()) + posterCount()) % posterCount();
    auto px = renderPoster(index, PictureData::kW, PictureData::kH);
    auto d = std::make_shared<PictureData>();
    d->image = imageFromABGR(px, PictureData::kW, PictureData::kH);
    d->luma = lumaGridFromABGR(px.data(), PictureData::kW, PictureData::kH, PictureData::kW);
    d->poster = index;
    d->name = posterName(index);
    install(std::move(d), {});
}

void ImageStore::setPicture(const std::vector<uint32_t>& abgr, const juce::String& name)
{
    if (abgr.size() != (size_t)PictureData::kW * PictureData::kH) return;
    auto d = std::make_shared<PictureData>();
    d->image = imageFromABGR(abgr, PictureData::kW, PictureData::kH);
    d->luma = lumaGridFromABGR(abgr.data(), PictureData::kW, PictureData::kH, PictureData::kW);
    d->poster = -1;
    d->name = name;
    juce::MemoryBlock png;
    {
        juce::MemoryOutputStream mo(png, false);
        juce::PNGImageFormat fmt;
        fmt.writeImageToStream(d->image, mo);
    }
    install(std::move(d), std::move(png));
}

bool ImageStore::setFromPng(const juce::MemoryBlock& png, const juce::String& name)
{
    std::vector<uint32_t> px;
    if (!decodeImageCover(static_cast<const uint8_t*>(png.getData()), png.getSize(), PictureData::kW,
                          PictureData::kH, px).empty())
        return false;
    setPicture(px, name);
    return true;
}

std::shared_ptr<const PictureData> ImageStore::get() const
{
    const juce::ScopedLock sl(lock_);
    return data_;
}

juce::MemoryBlock ImageStore::png() const
{
    const juce::ScopedLock sl(lock_);
    return png_;
}

juce::StringArray ImageStore::supportedExtensions()
{
    return {".png", ".jpg", ".jpeg", ".bmp", ".gif"};
}

juce::String ImageStore::decodeFile(const juce::File& f, std::vector<uint32_t>& abgr)
{
    juce::MemoryBlock data;
    if (!f.loadFileAsData(data)) return "Cannot open " + f.getFileName();
    auto err = decodeImageCover(static_cast<const uint8_t*>(data.getData()), data.getSize(), PictureData::kW,
                                PictureData::kH, abgr);
    if (!err.empty()) return juce::String(err);
    return {};
}

} // namespace madcap
