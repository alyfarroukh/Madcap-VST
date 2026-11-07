// Offline helpers for sources: resampling, mono mixdown, normalising, image grids.
// Framework-free; never call these on the audio thread (they allocate).
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace madcap {

// Band-limited windowed-sinc resampler (mono). Returns the input unchanged when rates match.
std::vector<float> resampleMono(const float* in, std::size_t n, double srcRate, double dstRate);

// Peak-normalise in place to the given linear peak (no-op for silence).
void normalizePeak(std::vector<float>& buf, float peak = 0.89f);

// Luminance grid (cols x rows, row 0 = top) from 0xAABBGGRR pixels (box filtered).
std::vector<float> lumaGridFromABGR(const uint32_t* px, int w, int h, int stride, int cols, int rows);
// Grid size for a w x h picture: the picture scaled down (never up) to fit maxCols x maxCols/2
// with its aspect ratio kept, clamped to the engine limits. maxCols: 64 ... IMG_COLS_MAX.
void lumaGridSize(int w, int h, int& cols, int& rows, int maxCols = 512);
// GRID RES parameter value (0..5) -> maximum columns (64, 128, 256, 512, 1024, 2048).
int gridResolutionColumns(int index);

// One of the built-in Bauhaus posters as 0xAABBGGRR pixels (w x h).
std::vector<uint32_t> renderPoster(int index, int w = 512, int h = 256);
int posterCount();
const char* posterName(int index);

// The built-in demo voice: a sung "ahh" on C3 at the given rate.
std::vector<float> demoVoice(double sampleRate, float seconds = 4.0f);

} // namespace madcap
