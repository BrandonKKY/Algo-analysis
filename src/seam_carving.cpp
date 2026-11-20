#include "seam_carving.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>

namespace {

// stopAll() bumps the generation; a carve() is stopped once the generation it
// started in is out of date. -1 = this thread is not inside carve().
std::atomic<long> stopGeneration{0};
thread_local long carveGeneration = -1;

}  // namespace

void stopAll() { ++stopGeneration; }

bool stopRequested() { return carveGeneration >= 0 && carveGeneration != stopGeneration.load(); }

bool findSeamOriginal(const Image& img, Seam& seam, std::string& error) {
    const int w = img.w, h = img.h;
    if (w < 2 || h < 1) {
        error = "image too small to carve";
        return false;
    }
    const Grid<double> e = sobelEnergy(img);

    // m[y*w + x] = energy of the cheapest seam from the top row down to pixel (y, x).
    std::vector<double> m(e.v.size());

    // Top row: a seam that starts here has only this pixel's energy so far.
    for (int x = 0; x < w; ++x) m[x] = e(0, x);

    // Every other row: own energy + the cheapest of the 3 neighbours above
    // (up-left, up, up-right), skipping neighbours outside the image.
    for (int y = 1; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double best = m[static_cast<size_t>(y - 1) * w + x];
            if (x > 0) best = std::min(best, m[static_cast<size_t>(y - 1) * w + x - 1]);
            if (x < w - 1) best = std::min(best, m[static_cast<size_t>(y - 1) * w + x + 1]);
            m[static_cast<size_t>(y) * w + x] = e(y, x) + best;
        }
    }

    // The cheapest complete seam ends at the smallest value in the bottom row.
    seam.assign(h, 0);
    int x = 0;
    for (int i = 1; i < w; ++i)
        if (m[static_cast<size_t>(h - 1) * w + i] < m[static_cast<size_t>(h - 1) * w + x]) x = i;
    seam[h - 1] = x;

    // Backtrack: walk up one row at a time, moving to whichever of the 3
    // neighbours above has the smallest m. That is the path the seam came from.
    for (int y = h - 2; y >= 0; --y) {
        int bx = x;
        double best = m[static_cast<size_t>(y) * w + x];
        if (x > 0 && m[static_cast<size_t>(y) * w + x - 1] < best) { best = m[static_cast<size_t>(y) * w + x - 1]; bx = x - 1; }
        if (x < w - 1 && m[static_cast<size_t>(y) * w + x + 1] < best) { bx = x + 1; }
        x = bx;
        seam[y] = x;
    }
    return true;
}

// Builds the narrower image row by row: copy the pixels left of the seam pixel,
// skip the seam pixel, then copy the pixels to its right.
Image removeSeam(const Image& img, const Seam& seam) {
    const int w = img.w, h = img.h;
    std::vector<uint8_t> out;
    out.reserve(static_cast<size_t>(w - 1) * h * 3);  // 3 bytes (R, G, B) per pixel
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = img.at(y, 0);
        out.insert(out.end(), row, row + static_cast<size_t>(seam[y]) * 3);
        out.insert(out.end(), row + static_cast<size_t>(seam[y] + 1) * 3, row + static_cast<size_t>(w) * 3);
    }
    return Image(w - 1, h, std::move(out));
}

bool carve(const Image& img, SeamFinder finder, int count,
           std::vector<Seam>& seams, std::string& error) {
    if (count < 0 || count >= img.w) {
        error = "seam count must be between 0 and width-1";
        return false;
    }
    // Marks this thread as carving until the function returns.
    struct Guard {
        Guard() { carveGeneration = stopGeneration.load(); }
        ~Guard() { carveGeneration = -1; }
    } guard;

    // Find a seam, remove it, repeat. The energy map is recomputed inside the
    // finder each time, because removing a seam changes the neighbours of the
    // pixels next to it.
    Image cur = img;
    seams.clear();
    for (int i = 0; i < count; ++i) {
        Seam s;
        const bool found = finder(cur, s, error);
        if (stopRequested()) {
            error = "stopped";
            return false;
        }
        if (!found) return false;
        cur = removeSeam(cur, s);
        seams.push_back(std::move(s));
    }
    return true;
}
