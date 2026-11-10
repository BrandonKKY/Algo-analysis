#include "greedy.hpp"

#include <chrono>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>

#include "energy.hpp"

namespace {

using Clock = std::chrono::steady_clock;

double msSince(Clock::time_point t) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}

// One greedy step: from column c in the row above, pick the cheapest of
// (c, c-1, c+1) in row r. Ties prefer straight down, then left, then right.
inline int greedyStep(const Grid<double>& e, int r, int c) {
    int best = c;
    if (c > 0 && e(r, c - 1) < e(r, best)) best = c - 1;
    if (c < e.w - 1 && e(r, c + 1) < e(r, best)) best = c + 1;
    return best;
}

// Compact a row-major buffer in place, dropping pixel seam[r] from each row.
// `k` = number of elements per pixel (3 for RGB, 1 for grids).
template <typename T>
void removeSeamFromBuffer(std::vector<T>& data, int w, int h, int k,
                          const std::vector<int>& seam) {
    std::size_t dst = 0;
    for (int r = 0; r < h; ++r) {
        std::size_t rowStart = std::size_t(r) * w * k;
        for (int c = 0; c < w; ++c) {
            if (c == seam[r]) continue;
            for (int ch = 0; ch < k; ++ch) data[dst++] = data[rowStart + std::size_t(c) * k + ch];
        }
    }
    data.resize(dst);
}

}  // namespace

// --------------------------------------------------------------------------
// Seam finding
// --------------------------------------------------------------------------
std::vector<int> findVerticalSeamGreedy(const Grid<double>& e, int startCol) {
    std::vector<int> seam(e.h);
    int c = startCol;
    if (c < 0) {  // start at the minimum-energy pixel of the top row
        c = 0;
        for (int j = 1; j < e.w; ++j)
            if (e(0, j) < e(0, c)) c = j;
    }
    seam[0] = c;
    for (int r = 1; r < e.h; ++r) {
        c = greedyStep(e, r, c);
        seam[r] = c;
    }
    return seam;
}

std::vector<int> findVerticalSeamGreedyMultistart(const Grid<double>& e) {
    // Walk greedily from every start column, remember only the best start,
    // then re-walk from it. O(W*H) time, O(H) extra memory.
    int bestStart = 0;
    double bestCost = std::numeric_limits<double>::infinity();
    for (int start = 0; start < e.w; ++start) {
        int c = start;
        double cost = e(0, c);
        for (int r = 1; r < e.h && cost < bestCost; ++r) {  // prune: can't win
            c = greedyStep(e, r, c);
            cost += e(r, c);
        }
        if (cost < bestCost) {
            bestCost = cost;
            bestStart = start;
        }
    }
    return findVerticalSeamGreedy(e, bestStart);
}

std::vector<int> findVerticalSeam(const Grid<double>& e, GreedyMode mode) {
    return mode == GreedyMode::Multistart ? findVerticalSeamGreedyMultistart(e)
                                          : findVerticalSeamGreedy(e);
}

double seamCost(const Grid<double>& e, const std::vector<int>& seam) {
    double s = 0;
    for (int r = 0; r < e.h; ++r) s += e(r, seam[r]);
    return s;
}

// --------------------------------------------------------------------------
// Seam removal
// --------------------------------------------------------------------------
void removeVerticalSeam(Image& img, const std::vector<int>& seam) {
    removeSeamFromBuffer(img.px, img.w, img.h, 3, seam);
    --img.w;
}

void removeVerticalSeam(Grid<int>& g, const std::vector<int>& seam) {
    removeSeamFromBuffer(g.v, g.w, g.h, 1, seam);
    --g.w;
}

// --------------------------------------------------------------------------
// Full resize loop
// --------------------------------------------------------------------------
CarveResult carveGreedy(const Image& input, int targetW, int targetH, GreedyMode mode,
                        bool trackSeams) {
    if (targetW < 1 || targetH < 1 || targetW > input.w || targetH > input.h)
        throw std::invalid_argument(
            "target size " + std::to_string(targetW) + "x" + std::to_string(targetH) +
            " must be between 1x1 and the original " + std::to_string(input.w) + "x" +
            std::to_string(input.h) + " (this implementation only removes seams)");

    auto tStart = Clock::now();
    CarveResult res;
    Image img = input;

    // rowMap/colMap: current pixel -> original (row, col), for drawing seams later.
    Grid<int> rowMap, colMap;
    if (trackSeams) {
        rowMap = Grid<int>(img.w, img.h);
        colMap = Grid<int>(img.w, img.h);
        for (int r = 0; r < img.h; ++r)
            for (int c = 0; c < img.w; ++c) {
                rowMap(r, c) = r;
                colMap(r, c) = c;
            }
    }

    // Removes `count` vertical seams from the current (possibly transposed) image.
    auto removeSeams = [&](int count, std::vector<std::vector<Point>>& store) {
        for (int k = 0; k < count; ++k) {
            auto t0 = Clock::now();
            Grid<double> energy = sobelEnergy(img);
            res.energyMs += msSince(t0);

            auto t1 = Clock::now();
            std::vector<int> seam = findVerticalSeam(energy, mode);
            res.findMs += msSince(t1);

            auto t2 = Clock::now();
            res.seamCosts.push_back(seamCost(energy, seam));
            if (trackSeams) {
                std::vector<Point> coords(seam.size());
                for (int r = 0; r < img.h; ++r)
                    coords[r] = {rowMap(r, seam[r]), colMap(r, seam[r])};
                store.push_back(std::move(coords));
                removeVerticalSeam(rowMap, seam);
                removeVerticalSeam(colMap, seam);
            }
            removeVerticalSeam(img, seam);
            res.removeMs += msSince(t2);
        }
    };

    // 1) Vertical seams -> reduce width.
    removeSeams(input.w - targetW, res.verticalSeams);

    // 2) Horizontal seams -> transpose once, remove "vertical" seams, transpose back.
    //    (Sobel energy of the transposed image == transpose of the energy.)
    int hCount = input.h - targetH;
    if (hCount > 0) {
        img = transpose(img);
        if (trackSeams) {
            rowMap = transpose(rowMap);
            colMap = transpose(colMap);
        }
        removeSeams(hCount, res.horizontalSeams);
        img = transpose(img);
    }

    res.image = std::move(img);
    res.totalMs = msSince(tStart);
    return res;
}

// --------------------------------------------------------------------------
// Visualisation
// --------------------------------------------------------------------------
Image drawSeams(const Image& original, const CarveResult& res) {
    Image out = original;
    auto paint = [&](const std::vector<std::vector<Point>>& seams, std::uint8_t R,
                     std::uint8_t G, std::uint8_t B) {
        for (const auto& seam : seams)
            for (const Point& p : seam) {
                std::uint8_t* px = out.at(p.r, p.c);
                px[0] = R;
                px[1] = G;
                px[2] = B;
            }
    };
    paint(res.verticalSeams, 255, 0, 0);
    paint(res.horizontalSeams, 0, 0, 255);
    return out;
}
