#include "SampleStore.h"
#include "madcap/Resample.h"

namespace madcap {

namespace {

bool flacSupportsRate(double rate)
{
    juce::FlacAudioFormat flac;
    for (int r : flac.getPossibleSampleRates())
        if (std::abs((double)r - rate) < 0.5) return true;
    return false;
}

void buildOverview(SampleData& d)
{
    const int bins = SampleData::kOverview;
    d.overviewMin.assign((size_t)bins, 0.0f);
    d.overviewMax.assign((size_t)bins, 0.0f);
    const size_t n = d.samples.size();
    if (n == 0) return;
    for (int b = 0; b < bins; ++b) {
        size_t a = n * (size_t)b / (size_t)bins, e = std::max(a + 1, n * (size_t)(b + 1) / (size_t)bins);
        float mn = 0.0f, mx = 0.0f;
        for (size_t i = a; i < e && i < n; ++i) {
            mn = std::min(mn, d.samples[i]);
            mx = std::max(mx, d.samples[i]);
        }
        d.overviewMin[(size_t)b] = mn;
        d.overviewMax[(size_t)b] = mx;
    }
}

} // namespace

void SampleStore::set(std::vector<float> mono, double rate, const juce::String& name, bool isDemo)
{
    if (rate <= 0) rate = 48000.0;
    // keep the stored copy at a rate FLAC can hold (so the project reopens identically)
    if (!flacSupportsRate(rate) && !mono.empty()) {
        mono = resampleMono(mono.data(), mono.size(), rate, 48000.0);
        rate = 48000.0;
    }
    const size_t maxFrames = (size_t)(kMaxSeconds * rate);
    if (mono.size() > maxFrames) mono.resize(maxFrames);
    for (auto& v : mono)
        if (!std::isfinite(v)) v = 0.0f;

    auto d = std::make_shared<SampleData>();
    d->samples = std::move(mono);
    d->rate = rate;
    d->name = name;
    d->isDemo = isDemo;
    buildOverview(*d);
    juce::MemoryBlock flac = isDemo ? juce::MemoryBlock() : encodeFlac(d->samples, rate);

    const juce::ScopedLock sl(lock_);
    data_ = std::move(d);
    flac_ = std::move(flac);
    ++version_;
}

std::shared_ptr<const SampleData> SampleStore::get() const
{
    const juce::ScopedLock sl(lock_);
    return data_;
}

juce::MemoryBlock SampleStore::flac() const
{
    const juce::ScopedLock sl(lock_);
    return flac_;
}

juce::StringArray SampleStore::supportedExtensions()
{
    return {".wav", ".wave", ".aif", ".aiff", ".aifc", ".flac", ".mp3", ".ogg", ".oga"};
}

juce::String SampleStore::decodeFile(const juce::File& file, std::vector<float>& mono, double& rate)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader(fm.createReaderFor(file));
    if (!reader) return "Unsupported or damaged audio file";
    if (reader->sampleRate <= 0 || reader->numChannels == 0) return "Audio file has no audio";
    rate = reader->sampleRate;
    const auto frames = (int)std::min<juce::int64>(reader->lengthInSamples, (juce::int64)(kMaxSeconds * rate));
    if (frames <= 0) return "Audio file is empty";
    const int ch = (int)std::min<unsigned>(reader->numChannels, 8u);
    juce::AudioBuffer<float> buf(ch, frames);
    if (!reader->read(&buf, 0, frames, 0, true, true)) return "Could not read the audio file";
    mono.assign((size_t)frames, 0.0f);
    const float g = 1.0f / (float)ch;
    for (int c = 0; c < ch; ++c) {
        const float* s = buf.getReadPointer(c);
        for (int i = 0; i < frames; ++i) mono[(size_t)i] += s[i] * g;
    }
    normalizePeak(mono, 0.89f);
    return {};
}

juce::MemoryBlock SampleStore::encodeFlac(const std::vector<float>& mono, double rate)
{
    juce::MemoryBlock out;
    if (mono.empty()) return out;
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::MemoryOutputStream>(out, false);
    auto writer = flac.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                                   .withSampleRate(rate)
                                                   .withNumChannels(1)
                                                   .withBitsPerSample(24)
                                                   .withQualityOptionIndex(5));
    if (!writer) return {};
    const float* chans[1] = { mono.data() };
    const int block = 65536;
    for (size_t pos = 0; pos < mono.size(); pos += (size_t)block) {
        const int n = (int)std::min<size_t>((size_t)block, mono.size() - pos);
        const float* p[1] = { chans[0] + pos };
        writer->writeFromFloatArrays(p, 1, n);
    }
    writer.reset();   // flushes into `out`
    return out;
}

bool SampleStore::decodeFlac(const juce::MemoryBlock& data, std::vector<float>& mono, double& rate)
{
    if (data.isEmpty()) return false;
    juce::FlacAudioFormat flac;
    std::unique_ptr<juce::AudioFormatReader> reader(
        flac.createReaderFor(new juce::MemoryInputStream(data, false), true));
    if (!reader || reader->lengthInSamples <= 0) return false;
    rate = reader->sampleRate;
    const auto frames = (int)std::min<juce::int64>(reader->lengthInSamples, (juce::int64)(kMaxSeconds * rate));
    juce::AudioBuffer<float> buf((int)std::max(1u, reader->numChannels), frames);
    if (!reader->read(&buf, 0, frames, 0, true, true)) return false;
    mono.assign(buf.getReadPointer(0), buf.getReadPointer(0) + frames);
    return true;
}

} // namespace madcap
