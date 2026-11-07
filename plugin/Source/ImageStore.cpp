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

std::shared_ptr<PictureData> makePicture(const std::vector<uint32_t>& px, int w, int h)
{
    auto d = std::make_shared<PictureData>();
    d->image = imageFromABGR(px, w, h);
    lumaGridSize(w, h, d->cols, d->rows);
    d->luma = lumaGridFromABGR(px.data(), w, h, w, d->cols, d->rows);
    return d;
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
    auto px = renderPoster(index, PictureData::kPosterW, PictureData::kPosterH);
    auto d = makePicture(px, PictureData::kPosterW, PictureData::kPosterH);
    d->poster = index;
    d->name = posterName(index);
    install(std::move(d), {});
}

void ImageStore::setPicture(const std::vector<uint32_t>& abgr, int w, int h, const juce::String& name)
{
    if (w <= 0 || h <= 0 || abgr.size() != (size_t)w * (size_t)h) return;
    auto d = makePicture(abgr, w, h);
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
    int w = 0, h = 0;
    if (!decodeImageFit(static_cast<const uint8_t*>(png.getData()), png.getSize(), PictureData::kMaxW,
                        PictureData::kMaxH, px, w, h).empty())
        return false;
    setPicture(px, w, h, name);
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

juce::String ImageStore::decodeFile(const juce::File& f, std::vector<uint32_t>& abgr, int& w, int& h)
{
    juce::MemoryBlock data;
    if (!f.loadFileAsData(data)) return "Cannot open " + f.getFileName();
    auto err = decodeImageFit(static_cast<const uint8_t*>(data.getData()), data.getSize(), PictureData::kMaxW,
                              PictureData::kMaxH, abgr, w, h);
    if (!err.empty()) return juce::String(err);
    return {};
}

} // namespace madcap
