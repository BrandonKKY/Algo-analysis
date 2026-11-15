#pragma once
#include <cstdint>
#include <vector>

// Simple interleaved 8-bit RGB image (row-major).
struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgb;

    Image() = default;
    Image(int w, int h, std::vector<uint8_t> data)
        : width(w), height(h), rgb(std::move(data)) {}

    const uint8_t* px(int x, int y) const { return &rgb[(static_cast<size_t>(y) * width + x) * 3]; }
};
