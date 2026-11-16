// Checks the graph cut seam finder against dynamic programming.
//
// The graph from the brief ties the last column to the sink, so graph cut
// finds the cheapest seam that does not use the last column. The test runs a
// small DP with the same restriction: both are exact, so the two costs must
// be EQUAL on every image (the seams themselves may differ on ties).
// It also counts how often that differs from the unrestricted DP reference
// (findSeamOriginal), which is allowed to use the last column.
//
// Build + run:  make test

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include "../src/seam_methods.h"

namespace {

int failures = 0;

void check(bool ok, const std::string& what) {
    if (!ok) {
        std::printf("  FAIL: %s\n", what.c_str());
        ++failures;
    }
}

double seamCost(const Image& img, const Seam& seam) {
    const std::vector<double> e = computeEnergy(img);
    double total = 0;
    for (int y = 0; y < img.height; ++y) total += e[static_cast<size_t>(y) * img.width + seam[y]];
    return total;
}

bool isValidSeam(const Image& img, const Seam& seam) {
    if (static_cast<int>(seam.size()) != img.height) return false;
    for (int y = 0; y < img.height; ++y) {
        if (seam[y] < 0 || seam[y] >= img.width) return false;
        if (y > 0 && std::abs(seam[y] - seam[y - 1]) > 1) return false;
    }
    return true;
}

// `levels` controls how many distinct values a channel can take: small values
// produce flat regions and lots of ties, 256 produces pure noise.
Image randomImage(int w, int h, int levels, std::mt19937& rng) {
    std::vector<uint8_t> data(static_cast<size_t>(w) * h * 3);
    std::uniform_int_distribution<int> d(0, levels - 1);
    for (auto& v : data) v = static_cast<uint8_t>(d(rng) * 255 / std::max(levels - 1, 1));
    return Image(w, h, std::move(data));
}

// A smooth gradient with a bright vertical bar: looks more like a real photo
// (large low-energy areas) than random noise does.
Image smoothImage(int w, int h) {
    std::vector<uint8_t> data(static_cast<size_t>(w) * h * 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            uint8_t* p = &data[(static_cast<size_t>(y) * w + x) * 3];
            const bool bar = x > w / 2 && x < w / 2 + w / 8;
            p[0] = static_cast<uint8_t>(bar ? 250 : 40 + 100 * x / w);
            p[1] = static_cast<uint8_t>(bar ? 240 : 60 + 120 * y / h);
            p[2] = static_cast<uint8_t>(bar ? 200 : 90 + 30 * std::sin(x * 0.2) + 30 * std::cos(y * 0.15));
        }
    return Image(w, h, std::move(data));
}

// Cost of the cheapest vertical seam that stays in columns 0..w-2 (plain DP).
double bestCostWithoutLastColumn(const Image& img) {
    const int w = img.width, h = img.height, cols = w - 1;
    const std::vector<double> e = computeEnergy(img);
    std::vector<double> prev(cols), cur(cols);
    for (int x = 0; x < cols; ++x) prev[x] = e[x];
    for (int y = 1; y < h; ++y) {
        for (int x = 0; x < cols; ++x) {
            double best = prev[x];
            if (x > 0) best = std::min(best, prev[x - 1]);
            if (x + 1 < cols) best = std::min(best, prev[x + 1]);
            cur[x] = e[static_cast<size_t>(y) * w + x] + best;
        }
        prev.swap(cur);
    }
    return *std::min_element(prev.begin(), prev.end());
}

int comparisons = 0, worseThanFullDp = 0;

void compareOnce(const Image& img, const std::string& name) {
    Seam dp, gc;
    std::string err;
    check(findSeamOriginal(img, dp, err), name + ": DP reference failed: " + err);
    check(findSeamGraphcut(img, gc, err), name + ": graph cut failed: " + err);
    if (dp.empty() || gc.empty()) return;
    check(isValidSeam(img, gc), name + ": graph cut seam is not a connected seam");
    const double want = bestCostWithoutLastColumn(img), got = seamCost(img, gc);
    check(std::fabs(want - got) < 1e-6,
          name + ": cost differs, DP=" + std::to_string(want) + " graphcut=" + std::to_string(got));
    ++comparisons;
    if (got > seamCost(img, dp) + 1e-6) ++worseThanFullDp;
}

// Horizontal seam: seam[x] = row removed from column x.
void compareHorizontal(const Image& img, const std::string& name) {
    Seam gc;
    std::string err;
    check(findHorizontalSeamGraphcut(img, gc, err), name + ": horizontal graph cut failed: " + err);
    if (gc.empty()) return;
    bool valid = static_cast<int>(gc.size()) == img.width;
    for (int x = 0; valid && x < img.width; ++x) {
        if (gc[x] < 0 || gc[x] >= img.height) valid = false;
        if (x > 0 && std::abs(gc[x] - gc[x - 1]) > 1) valid = false;
    }
    check(valid, name + ": horizontal seam is not a connected seam");
    if (!valid) return;
    // Cost measured directly on the un-transposed image.
    const std::vector<double> e = computeEnergy(img);
    double got = 0;
    for (int x = 0; x < img.width; ++x) got += e[static_cast<size_t>(gc[x]) * img.width + x];
    const double want = bestCostWithoutLastColumn(transposeImage(img));
    check(std::fabs(want - got) < 1e-6,
          name + ": horizontal cost differs, DP=" + std::to_string(want) + " graphcut=" + std::to_string(got));
}

double millis(std::chrono::steady_clock::time_point from) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
}

}  // namespace

int main() {
    std::mt19937 rng(12345);

    std::printf("[1] single-seam cost equals DP on random images\n");
    int cases = 0;
    for (int levels : {2, 3, 8, 256})
        for (int trial = 0; trial < 150; ++trial) {
            const int w = 2 + static_cast<int>(rng() % 30), h = 1 + static_cast<int>(rng() % 30);
            compareOnce(randomImage(w, h, levels, rng),
                        std::to_string(w) + "x" + std::to_string(h) + " levels=" + std::to_string(levels));
            ++cases;
        }
    std::printf("  %d images checked\n", cases);

    std::printf("[2] edge-case shapes\n");
    for (auto wh : std::vector<std::pair<int, int>>{{2, 1}, {2, 2}, {2, 50}, {50, 1}, {3, 3}, {200, 2}, {2, 200}}) {
        compareOnce(randomImage(wh.first, wh.second, 4, rng),
                    std::to_string(wh.first) + "x" + std::to_string(wh.second));
        compareOnce(Image(wh.first, wh.second, std::vector<uint8_t>(static_cast<size_t>(wh.first) * wh.second * 3, 128)),
                    "flat " + std::to_string(wh.first) + "x" + std::to_string(wh.second));
    }
    Seam s;
    std::string err;
    check(!findSeamGraphcut(Image(1, 5, std::vector<uint8_t>(15, 0)), s, err), "width-1 image must be rejected");

    std::printf("[3] carving many seams in a row (cost checked at every step)\n");
    {
        Image cur = smoothImage(60, 40);
        for (int i = 0; i < 40; ++i) {
            compareOnce(cur, "smooth step " + std::to_string(i));
            Seam gc;
            if (!findSeamGraphcut(cur, gc, err)) break;
            cur = removeSeam(cur, gc);
        }
        check(cur.width == 20, "expected width 20 after removing 40 seams");
    }

    std::printf("  graph cut cost was higher than unrestricted DP on %d of %d images\n", worseThanFullDp, comparisons);

    std::printf("[4] horizontal seams (transposed image)\n");
    {
        int n = 0;
        for (int levels : {2, 8, 256})
            for (int trial = 0; trial < 100; ++trial, ++n) {
                const int w = 1 + static_cast<int>(rng() % 30), h = 2 + static_cast<int>(rng() % 30);
                compareHorizontal(randomImage(w, h, levels, rng),
                                  std::to_string(w) + "x" + std::to_string(h) + " levels=" + std::to_string(levels));
            }
        std::printf("  %d images checked\n", n);

        // Remove 15 horizontal seams: transpose, remove as a vertical seam, transpose back.
        Image cur = smoothImage(50, 40);
        for (int i = 0; i < 15; ++i) {
            Seam gc;
            if (!findHorizontalSeamGraphcut(cur, gc, err)) break;
            cur = transposeImage(removeSeam(transposeImage(cur), gc));
        }
        check(cur.width == 50 && cur.height == 25, "expected 50x25 after removing 15 horizontal seams");
        check(!findHorizontalSeamGraphcut(Image(5, 1, std::vector<uint8_t>(15, 0)), s, err), "height-1 image must be rejected");
    }

    std::printf("[5] running time, one vertical seam (ms)\n");
    std::printf("  %-12s %-8s %10s %10s %8s\n", "image", "kind", "DP", "graphcut", "ratio");
    for (auto wh : std::vector<std::pair<int, int>>{{64, 40}, {100, 75}, {200, 150}}) {
        for (int kind = 0; kind < 2; ++kind) {
            const Image img = kind == 0 ? smoothImage(wh.first, wh.second) : randomImage(wh.first, wh.second, 256, rng);
            Seam dp, gc;
            auto t0 = std::chrono::steady_clock::now();
            findSeamOriginal(img, dp, err);
            const double tDp = millis(t0);
            t0 = std::chrono::steady_clock::now();
            findSeamGraphcut(img, gc, err);
            const double tGc = millis(t0);
            check(std::fabs(bestCostWithoutLastColumn(img) - seamCost(img, gc)) < 1e-6, "timing image: cost differs");
            std::printf("  %-12s %-8s %10.2f %10.2f %7.0fx\n",
                        (std::to_string(wh.first) + "x" + std::to_string(wh.second)).c_str(),
                        kind == 0 ? "smooth" : "noise", tDp, tGc, tGc / std::max(tDp, 1e-6));
        }
    }

    if (failures) {
        std::printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll checks passed\n");
    return 0;
}
