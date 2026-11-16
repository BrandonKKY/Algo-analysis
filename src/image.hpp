// Basic image / 2-D grid types and image file I/O.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Row-major 2-D array, e.g. an energy map (Grid<double>) or an index map (Grid<int>).
template <typename T>
struct Grid {
    int w = 0, h = 0;
    std::vector<T> v;

    Grid() = default;
    Grid(int w_, int h_, T init = T()) : w(w_), h(h_), v(std::size_t(w_) * h_, init) {}

    T& operator()(int r, int c) { return v[std::size_t(r) * w + c]; }
    const T& operator()(int r, int c) const { return v[std::size_t(r) * w + c]; }
};

// 8-bit RGB image, row-major, 3 bytes per pixel.
struct Image {
    int w = 0, h = 0;
    std::vector<std::uint8_t> px;

    Image() = default;
    Image(int w_, int h_) : w(w_), h(h_), px(std::size_t(w_) * h_ * 3, 0) {}

    std::uint8_t* at(int r, int c) { return &px[(std::size_t(r) * w + c) * 3]; }
    const std::uint8_t* at(int r, int c) const { return &px[(std::size_t(r) * w + c) * 3]; }
};

// Load any format stb_image supports (png, jpg, bmp, ...) as RGB.
bool loadImage(const std::string& path, Image& out, std::string& err);
bool savePng(const std::string& path, const Image& img);

Image transpose(const Image& img);

template <typename T>
Grid<T> transpose(const Grid<T>& g) {
    Grid<T> t(g.h, g.w);
    for (int r = 0; r < g.h; ++r)
        for (int c = 0; c < g.w; ++c) t(c, r) = g(r, c);
    return t;
}
