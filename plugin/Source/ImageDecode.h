// Picture decoding for the image sweeper (no JUCE): baseline JPEGs of any size via
// picojpeg (decoded block by block, 1/8 "reduce" mode for big photos), everything else
// (progressive JPEG, PNG, BMP, GIF) via stb_image. The whole picture is kept (no crop):
// it is scaled down to fit inside maxW x maxH with its own aspect ratio (never scaled up)
// into outW x outH 0xAABBGGRR pixels. Thread-safe; call from a background thread.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace madcap {

// Returns an empty string on success, otherwise a short error message.
std::string decodeImageFit(const uint8_t* data, size_t size, int maxW, int maxH, std::vector<uint32_t>& out,
                           int& outW, int& outH);

} // namespace madcap
