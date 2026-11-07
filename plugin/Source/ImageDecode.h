// Picture decoding for the image sweeper (no JUCE): baseline JPEGs of any size via
// picojpeg (decoded block by block, 1/8 "reduce" mode for big photos), everything else
// (progressive JPEG, PNG, BMP, GIF) via stb_image. Output is cover-fitted into a
// dw x dh buffer of 0xAABBGGRR pixels. Thread-safe; call from a background thread.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace madcap {

// Returns an empty string on success, otherwise a short error message.
std::string decodeImageCover(const uint8_t* data, size_t size, int dw, int dh, std::vector<uint32_t>& out);

} // namespace madcap
