#include "madcap/Resample.h"
#include "demo.h"
#include "imgsynth.h"
#include "posters.h"
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdlib>

namespace madcap {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr int kZeroCrossings = 16;   // taps per side
constexpr int kTableRes = 512;       // table points per zero crossing

struct SincTable {
    std::vector<float> t;
    SincTable()
    {
        const int n = kZeroCrossings * kTableRes + 2;
        t.resize((size_t)n);
        for (int i = 0; i < n; ++i) {
            double x = (double)i / kTableRes;                 // in zero crossings
            double s = x == 0.0 ? 1.0 : std::sin(kPi * x) / (kPi * x);
            double r = x / kZeroCrossings;                     // Blackman-Harris window
            double w = r >= 1.0 ? 0.0
                     : 0.35875 + 0.48829 * std::cos(kPi * r) + 0.14128 * std::cos(2 * kPi * r)
                       + 0.01168 * std::cos(3 * kPi * r);
            t[(size_t)i] = (float)(s * w);
        }
    }
    float at(double x) const   // x >= 0, in zero crossings
    {
        double p = x * kTableRes;
        size_t i = (size_t)p;
        if (i + 1 >= t.size()) return 0.0f;
        float f = (float)(p - (double)i);
        return t[i] + (t[i + 1] - t[i]) * f;
    }
};
} // namespace

std::vector<float> resampleMono(const float* in, std::size_t n, double srcRate, double dstRate)
{
    if (!in || n == 0) return {};
    if (srcRate <= 0 || dstRate <= 0 || std::abs(srcRate - dstRate) < 1e-6) return std::vector<float>(in, in + n);
    static const SincTable table;
    const double ratio = dstRate / srcRate;
    const double cutoff = std::min(1.0, ratio) * 0.97;         // anti-alias when downsampling
    const size_t outN = (size_t)std::floor((double)n * ratio);
    std::vector<float> out(outN);
    const double halfWidth = kZeroCrossings / cutoff;          // input samples per side
    for (size_t o = 0; o < outN; ++o) {
        double centre = (double)o / ratio;
        long first = (long)std::ceil(centre - halfWidth);
        long last = (long)std::floor(centre + halfWidth);
        double acc = 0.0, wsum = 0.0;
        for (long k = first; k <= last; ++k) {
            double x = std::abs((double)k - centre) * cutoff;
            if (x >= kZeroCrossings) continue;
            double w = table.at(x);
            wsum += w;
            if (k >= 0 && k < (long)n) acc += w * in[k];
        }
        out[o] = (float)(wsum > 1e-9 ? acc / wsum : 0.0);
    }
    return out;
}

void normalizePeak(std::vector<float>& buf, float peak)
{
    float m = 0.0f;
    for (float v : buf) m = std::max(m, std::abs(v));
    if (m < 1e-6f) return;
    const float g = peak / m;
    for (float& v : buf) v *= g;
}

std::vector<float> lumaGridFromABGR(const uint32_t* px, int w, int h, int stride, int cols, int rows)
{
    std::vector<float> grid((size_t)std::max(cols, 1) * (size_t)std::max(rows, 1), 0.0f);
    if (!px || w <= 0 || h <= 0 || cols <= 0 || rows <= 0) return grid;
    gs_rgba_to_grid(reinterpret_cast<const unsigned*>(px), w, h, stride, grid.data(), cols, rows);
    return grid;
}

void lumaGridSize(int w, int h, int& cols, int& rows)
{
    // follow the picture: one column per pixel up to the maximum, never squashing the aspect
    double sc = std::min({1.0, (double)IMG_COLS_MAX / std::max(w, 1), (double)IMG_ROWS_MAX / std::max(h, 1)});
    cols = std::clamp((int)std::lround(w * sc), IMG_COLS_MIN, IMG_COLS_MAX);
    rows = std::clamp((int)std::lround(h * sc), IMG_ROWS_MIN, IMG_ROWS_MAX);
}

std::vector<uint32_t> renderPoster(int index, int w, int h)
{
    std::vector<uint32_t> px((size_t)w * (size_t)h);
    gs_poster_render(index, reinterpret_cast<unsigned*>(px.data()), w, h, w);
    return px;
}

int posterCount() { return gs_poster_count(); }
const char* posterName(int index) { return gs_poster_name(index); }

std::vector<float> demoVoice(double sampleRate, float seconds)
{
    float* raw = nullptr;
    int n = gs_demo_voice(&raw, (int)std::lround(sampleRate), seconds);
    std::vector<float> v;
    if (raw && n > 0) v.assign(raw, raw + n);
    std::free(raw);
    return v;
}

} // namespace madcap
