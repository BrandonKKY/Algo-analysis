#include "energy.hpp"

#include <algorithm>
#include <cmath>

Grid<double> sobelEnergy(const Image& img) {
    const int w = img.w, h = img.h;

    // RGB -> grayscale (ITU-R BT.601 luma weights)
    Grid<double> gray(w, h);
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c) {
            const std::uint8_t* p = img.at(r, c);
            gray(r, c) = 0.299 * p[0] + 0.587 * p[1] + 0.114 * p[2];
        }

    // Clamped lookup = replicate edge pixels.
    auto g = [&](int r, int c) {
        r = std::clamp(r, 0, h - 1);
        c = std::clamp(c, 0, w - 1);
        return gray(r, c);
    };

    Grid<double> e(w, h);
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c) {
            // Gx = [-1 0 1; -2 0 2; -1 0 1]
            double gx = (g(r - 1, c + 1) + 2 * g(r, c + 1) + g(r + 1, c + 1)) -
                        (g(r - 1, c - 1) + 2 * g(r, c - 1) + g(r + 1, c - 1));
            // Gy = [-1 -2 -1; 0 0 0; 1 2 1]
            double gy = (g(r + 1, c - 1) + 2 * g(r + 1, c) + g(r + 1, c + 1)) -
                        (g(r - 1, c - 1) + 2 * g(r - 1, c) + g(r - 1, c + 1));
            e(r, c) = std::sqrt(gx * gx + gy * gy);
        }
    return e;
}
