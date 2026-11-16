#include "seam_methods.h"

// Greedy seam finder for the web UI.
//
// Same walk as findVerticalSeamGreedy() in src/greedy.cpp (the "min" mode of
// the greedy command-line tool), written against the interface's Image and
// sobelEnergy() so it can be compared with DP and graph cut on the same
// energy map:
//   1. Start at the lowest-energy pixel of the top row.
//   2. In each following row, look only at the 3 pixels below the previous
//      choice (left, straight, right) and move to the lowest-energy one.
//      On a tie, straight wins, then left.
// The choice is purely local, so the seam is not guaranteed to be the cheapest.
// Time per seam: O(W*H) for the energy map, then only O(W + H) for the walk.
//
// Find ONE vertical seam of `img` and store the removed column of each row
// in seam[y] (seam.size() == img.h, adjacent rows differ by at most 1).
bool findSeamGreedy(const Image& img, Seam& seam, std::string& error) {
    const int w = img.w, h = img.h;
    if (w < 2 || h < 1) {
        error = "image too small to carve";
        return false;
    }
    const Grid<double> e = sobelEnergy(img);
    auto energy = [&](int y, int x) { return e(y, x); };

    // Step 1: lowest-energy pixel of the top row.
    int x = 0;
    for (int i = 1; i < w; ++i)
        if (energy(0, i) < energy(0, x)) x = i;

    seam.assign(h, 0);
    seam[0] = x;

    // Step 2: walk down one row at a time.
    for (int y = 1; y < h; ++y) {
        int best = x;                                                  // straight down
        if (x > 0 && energy(y, x - 1) < energy(y, best)) best = x - 1;      // down-left
        if (x < w - 1 && energy(y, x + 1) < energy(y, best)) best = x + 1;  // down-right
        x = best;
        seam[y] = x;
    }
    return true;
}
