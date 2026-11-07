#include "ImageDecode.h"

#include <algorithm>
#include <cstring>
#include <mutex>

#if defined(__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wall"
 #pragma GCC diagnostic ignored "-Wextra"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wshadow"
#elif defined(_MSC_VER)
 #pragma warning(push, 0)
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_STATIC
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 30000
#include "third_party/stb_image.h"
#include "third_party/picojpeg.h"

#if defined(__clang__)
 #pragma clang diagnostic pop
#elif defined(__GNUC__)
 #pragma GCC diagnostic pop
#elif defined(_MSC_VER)
 #pragma warning(pop)
#endif

namespace madcap {

namespace {

constexpr uint64_t kMaxStbPixels = 80ull * 1024 * 1024;   // ~320 MB of RGBA at most

std::mutex picoMutex;   // picojpeg keeps its decoder state in globals

struct MemReader {
    const uint8_t* data;
    size_t size, pos;
};

unsigned char picoNeed(unsigned char* buf, unsigned char want, unsigned char* got, void* ud)
{
    auto* r = static_cast<MemReader*>(ud);
    size_t n = std::min<size_t>(want, r->size - r->pos);
    if (n) std::memcpy(buf, r->data + r->pos, n);
    r->pos += n;
    *got = (unsigned char)n;
    return 0;
}

void accumulateToOut(const std::vector<float>& acc, int dw, int dh, std::vector<uint32_t>& out)
{
    out.assign((size_t)dw * (size_t)dh, 0xFF808080u);
    for (int y = 0; y < dh; ++y)
        for (int x = 0; x < dw; ++x) {
            const float* a = &acc[((size_t)y * (size_t)dw + (size_t)x) * 4];
            uint32_t& o = out[(size_t)y * (size_t)dw + (size_t)x];
            if (a[3] <= 0.0f) {                          // tiny pictures: copy the neighbour
                if (x > 0) o = out[(size_t)y * (size_t)dw + (size_t)x - 1];
                else if (y > 0) o = out[(size_t)(y - 1) * (size_t)dw + (size_t)x];
                continue;
            }
            auto c = [&](int i) { return (uint32_t)std::clamp((int)(a[i] / a[3] + 0.5f), 0, 255); };
            o = 0xFF000000u | (c(2) << 16) | (c(1) << 8) | c(0);
        }
}

std::string loadPico(const uint8_t* data, size_t size, int dw, int dh, std::vector<uint32_t>& out)
{
    std::lock_guard<std::mutex> lock(picoMutex);
    MemReader rd{data, size, 0};
    pjpeg_image_info_t info{};
    unsigned char st = pjpeg_decode_init(&info, picoNeed, &rd, 0);
    if (st) return st == PJPG_UNSUPPORTED_MODE ? "progressive JPEG" : "JPEG error";
    const int W = info.m_width, H = info.m_height;
    if (W <= 0 || H <= 0) return "empty JPEG";
    const float sc = std::max((float)dw / (float)W, (float)dh / (float)H);
    const bool reduce = sc * 8.0f <= 1.3f;               // 1/8 resolution is (nearly) all we need
    if (reduce) {
        rd.pos = 0;
        st = pjpeg_decode_init(&info, picoNeed, &rd, 1);
        if (st) return "JPEG error";
    }
    std::vector<float> acc((size_t)dw * (size_t)dh * 4, 0.0f);
    const int unit = reduce ? 8 : 1;
    const float ox = ((float)W * sc - (float)dw) / 2.0f, oy = ((float)H * sc - (float)dh) / 2.0f;
    const int bw = info.m_MCUWidth / 8, bh = info.m_MCUHeight / 8;
    for (int my = 0; my < info.m_MCUSPerCol; ++my)
        for (int mx = 0; mx < info.m_MCUSPerRow; ++mx) {
            st = pjpeg_decode_mcu();
            if (st) {
                if (st == PJPG_NO_MORE_BLOCKS) goto done;
                return "JPEG error";
            }
            for (int by = 0; by < bh; ++by)
                for (int bx = 0; bx < bw; ++bx) {
                    const int blk = (by * bw + bx) * 64;
                    const int n = reduce ? 1 : 8;
                    for (int py = 0; py < n; ++py)
                        for (int px = 0; px < n; ++px) {
                            const int sx = mx * info.m_MCUWidth + bx * 8 + px * unit;
                            const int sy = my * info.m_MCUHeight + by * 8 + py * unit;
                            if (sx >= W || sy >= H) continue;
                            const int dx = (int)(((float)sx + (float)unit * 0.5f) * sc - ox);
                            const int dy = (int)(((float)sy + (float)unit * 0.5f) * sc - oy);
                            if (dx < 0 || dy < 0 || dx >= dw || dy >= dh) continue;
                            const int o = blk + (reduce ? 0 : py * 8 + px);
                            float* a = &acc[((size_t)dy * (size_t)dw + (size_t)dx) * 4];
                            if (info.m_comps == 1) {
                                float v = info.m_pMCUBufR[o];
                                a[0] += v; a[1] += v; a[2] += v;
                            } else {
                                a[0] += info.m_pMCUBufR[o]; a[1] += info.m_pMCUBufG[o]; a[2] += info.m_pMCUBufB[o];
                            }
                            a[3] += 1.0f;
                        }
                }
        }
done:
    accumulateToOut(acc, dw, dh, out);
    return {};
}

std::string loadStb(const uint8_t* data, size_t size, int dw, int dh, std::vector<uint32_t>& out)
{
    int w = 0, h = 0, comp = 0;
    if (size > (size_t)0x7fffffff) return "file too big";
    if (!stbi_info_from_memory(data, (int)size, &w, &h, &comp)) return stbi_failure_reason() ? stbi_failure_reason() : "unknown format";
    if (w <= 0 || h <= 0) return "empty picture";
    if ((uint64_t)w * (uint64_t)h > kMaxStbPixels) return "picture too big";
    unsigned char* px = stbi_load_from_memory(data, (int)size, &w, &h, &comp, 4);
    if (!px) return stbi_failure_reason() ? stbi_failure_reason() : "decode failed";
    const float sc = std::max((float)dw / (float)w, (float)dh / (float)h);   // cover: fill, crop the overflow
    const float ox = ((float)w * sc - (float)dw) / 2.0f, oy = ((float)h * sc - (float)dh) / 2.0f;
    std::vector<float> acc((size_t)dw * (size_t)dh * 4, 0.0f);
    for (int y = 0; y < dh; ++y) {
        int y0 = (int)(((float)y + oy) / sc), y1 = (int)(((float)y + 1.0f + oy) / sc);
        y0 = std::clamp(y0, 0, h - 1);
        y1 = std::clamp(y1, y0 + 1, h);
        for (int x = 0; x < dw; ++x) {
            int x0 = (int)(((float)x + ox) / sc), x1 = (int)(((float)x + 1.0f + ox) / sc);
            x0 = std::clamp(x0, 0, w - 1);
            x1 = std::clamp(x1, x0 + 1, w);
            float* a = &acc[((size_t)y * (size_t)dw + (size_t)x) * 4];
            for (int sy = y0; sy < y1; ++sy)
                for (int sx = x0; sx < x1; ++sx) {
                    const unsigned char* p = px + ((size_t)sy * (size_t)w + (size_t)sx) * 4;
                    const float al = p[3] / 255.0f;   // composite alpha over white paper
                    a[0] += p[0] * al + 255.0f * (1.0f - al);
                    a[1] += p[1] * al + 255.0f * (1.0f - al);
                    a[2] += p[2] * al + 255.0f * (1.0f - al);
                    a[3] += 1.0f;
                }
        }
    }
    stbi_image_free(px);
    accumulateToOut(acc, dw, dh, out);
    return {};
}

} // namespace

std::string decodeImageCover(const uint8_t* data, size_t size, int dw, int dh, std::vector<uint32_t>& out)
{
    if (!data || size < 8 || dw <= 0 || dh <= 0) return "empty file";
    std::string picoErr;
    if (data[0] == 0xFF && data[1] == 0xD8 && data[2] == 0xFF) {
        picoErr = loadPico(data, size, dw, dh, out);
        if (picoErr.empty()) return {};
    }
    std::string err = loadStb(data, size, dw, dh, out);
    if (err.empty()) return {};
    return picoErr.empty() ? err : picoErr + " / " + err;
}

} // namespace madcap
