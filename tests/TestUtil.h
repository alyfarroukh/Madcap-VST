#pragma once
#include "madcap/MadcapEngine.h"
#include "madcap/Resample.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <random>
#include <vector>

namespace test {

inline std::unique_ptr<madcap::SourceBuffer> makeSource(std::vector<float> s, double sr)
{
    auto b = std::make_unique<madcap::SourceBuffer>();
    b->samples = std::move(s);
    b->analyse((int)sr);
    return b;
}

inline std::unique_ptr<madcap::ImageGrid> posterGrid(int i)
{
    auto px = madcap::renderPoster(i);
    auto g = std::make_unique<madcap::ImageGrid>();
    g->luma = madcap::lumaGridFromABGR(px.data(), 512, 256, 512);
    return g;
}

// Engine with the demo voice and poster 0 loaded, all parameters at their defaults.
inline std::unique_ptr<madcap::Engine> makeEngine(double sr)
{
    auto e = std::make_unique<madcap::Engine>();
    e->prepare(sr);
    e->setSourceNow(makeSource(madcap::demoVoice(sr, 3.0f), sr));
    e->setImageNow(posterGrid(0));
    e->setTarget(P_BPM, 120.0f);
    return e;
}

struct RenderStats {
    bool finite = true;
    float peak = 0.0f;
    double rms = 0.0;
};

// Renders `seconds` of audio in blocks of `block` (block <= 0: random sizes 1..1024).
template <typename PerBlock>
RenderStats render(madcap::Engine& e, double seconds, int block, PerBlock&& perBlock, unsigned seed = 1)
{
    const int total = (int)(seconds * e.sampleRate());
    std::vector<float> l(4096), r(4096);
    std::mt19937 rng(seed);
    RenderStats st;
    double sum = 0.0;
    int done = 0;
    while (done < total) {
        int n = block > 0 ? block : (int)(rng() % 1024) + 1;
        if (n > total - done) n = total - done;
        if ((int)l.size() < n) { l.resize((size_t)n); r.resize((size_t)n); }
        perBlock(done);
        e.process(l.data(), r.data(), n);
        for (int i = 0; i < n; ++i) {
            if (!std::isfinite(l[(size_t)i]) || !std::isfinite(r[(size_t)i])) st.finite = false;
            st.peak = std::max(st.peak, std::max(std::abs(l[(size_t)i]), std::abs(r[(size_t)i])));
            sum += (double)l[(size_t)i] * l[(size_t)i] + (double)r[(size_t)i] * r[(size_t)i];
        }
        done += n;
    }
    st.rms = total > 0 ? std::sqrt(sum / (2.0 * total)) : 0.0;
    return st;
}

inline RenderStats render(madcap::Engine& e, double seconds, int block)
{
    return render(e, seconds, block, [](int) {});
}

} // namespace test
