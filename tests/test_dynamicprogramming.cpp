// Checks the dynamic programming seam finder (findSeamDp).
//
// Three references:
//   - brute force: on tiny images, try EVERY connected seam and keep the
//     cheapest. Independent of any DP, so it catches a wrong recurrence.
//   - findSeamOriginal: the reference DP. Same tie-break rules, so the seams
//     must be identical, not just the same cost.
//   - greedy: DP is exact, so it can never be more expensive than greedy.
//
// Build + run:  make test

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
    const Grid<double> e = sobelEnergy(img);
    double total = 0;
    for (int y = 0; y < img.h; ++y) total += e(y, seam[y]);
    return total;
}

bool isValidSeam(const Image& img, const Seam& seam) {
    if (static_cast<int>(seam.size()) != img.h) return false;
    for (int y = 0; y < img.h; ++y) {
        if (seam[y] < 0 || seam[y] >= img.w) return false;
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

// Cheapest seam by trying every one: w * 3^(h-1) seams, so tiny images only.
double bruteForceCost(const Grid<double>& e, int y, int x) {
    if (x < 0 || x >= e.w) return INFINITY;
    if (y == e.h - 1) return e(y, x);
    double best = INFINITY;
    for (int dx = -1; dx <= 1; ++dx) best = std::min(best, bruteForceCost(e, y + 1, x + dx));
    return e(y, x) + best;
}

double bruteForceBest(const Image& img) {
    const Grid<double> e = sobelEnergy(img);
    double best = INFINITY;
    for (int x = 0; x < img.w; ++x) best = std::min(best, bruteForceCost(e, 0, x));
    return best;
}

// Runs DP on one image and checks it against the reference DP and greedy.
void compareOnce(const Image& img, const std::string& name) {
    Seam dp, ref, greedy;
    std::string err;
    check(findSeamDp(img, dp, err), name + ": dp failed: " + err);
    check(findSeamOriginal(img, ref, err), name + ": reference failed: " + err);
    check(findSeamGreedy(img, greedy, err), name + ": greedy failed: " + err);
    if (dp.empty() || ref.empty() || greedy.empty()) return;
    check(isValidSeam(img, dp), name + ": dp seam is not a connected seam");
    const double got = seamCost(img, dp), want = seamCost(img, ref);
    check(std::fabs(got - want) < 1e-6,
          name + ": cost differs, original=" + std::to_string(want) + " dp=" + std::to_string(got));
    check(dp == ref, name + ": seam differs from findSeamOriginal (tie-break)");
    check(got <= seamCost(img, greedy) + 1e-6, name + ": dp is more expensive than greedy");
}

double millis(std::chrono::steady_clock::time_point from) {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - from).count();
}

}  // namespace

int main() {
    std::mt19937 rng(2024);

    std::printf("[1] cost equals brute force on tiny images\n");
    int cases = 0;
    for (int levels : {2, 4, 256})
        for (int trial = 0; trial < 100; ++trial) {
            const int w = 2 + static_cast<int>(rng() % 5), h = 1 + static_cast<int>(rng() % 7);
            const Image img = randomImage(w, h, levels, rng);
            Seam dp;
            std::string err;
            check(findSeamDp(img, dp, err), "brute force case: dp failed: " + err);
            if (dp.empty()) continue;
            const double want = bruteForceBest(img), got = seamCost(img, dp);
            check(std::fabs(want - got) < 1e-6, std::to_string(w) + "x" + std::to_string(h) +
                                                     ": brute force=" + std::to_string(want) +
                                                     " dp=" + std::to_string(got));
            ++cases;
        }
    std::printf("  %d images checked\n", cases);

    std::printf("[2] matches findSeamOriginal and beats greedy on random images\n");
    cases = 0;
    for (int levels : {2, 3, 8, 256})
        for (int trial = 0; trial < 150; ++trial) {
            const int w = 2 + static_cast<int>(rng() % 60), h = 1 + static_cast<int>(rng() % 60);
            compareOnce(randomImage(w, h, levels, rng),
                        std::to_string(w) + "x" + std::to_string(h) + " levels=" + std::to_string(levels));
            ++cases;
        }
    std::printf("  %d images checked\n", cases);

    std::printf("[3] edge-case shapes\n");
    for (auto wh : std::vector<std::pair<int, int>>{{2, 1}, {2, 2}, {2, 50}, {50, 1}, {3, 3}, {200, 2}, {2, 200}}) {
        compareOnce(randomImage(wh.first, wh.second, 4, rng),
                    std::to_string(wh.first) + "x" + std::to_string(wh.second));
        compareOnce(Image(wh.first, wh.second, std::vector<uint8_t>(static_cast<size_t>(wh.first) * wh.second * 3, 128)),
                    "flat " + std::to_string(wh.first) + "x" + std::to_string(wh.second));
    }
    {
        Seam s;
        std::string err;
        check(!findSeamDp(Image(1, 5, std::vector<uint8_t>(15, 0)), s, err), "width-1 image must be rejected");
        // Every seam costs 0 on a flat image, so the tie-break decides: straight down.
        check(findSeamDp(Image(10, 8, std::vector<uint8_t>(240, 128)), s, err), "flat 10x8: dp failed");
        bool straight = !s.empty();
        for (int x : s) straight = straight && x == s[0];
        check(straight, "flat image: seam should be straight");
    }

    std::printf("[4] carving many seams in a row (checked at every step)\n");
    {
        Image cur = randomImage(60, 40, 8, rng);
        for (int i = 0; i < 40; ++i) {
            compareOnce(cur, "step " + std::to_string(i));
            Seam s;
            std::string err;
            if (!findSeamDp(cur, s, err)) break;
            cur = removeSeam(cur, s);
        }
        check(cur.w == 20, "expected width 20 after removing 40 seams");

        std::vector<Seam> seams;
        std::string err;
        check(carve(randomImage(80, 50, 256, rng), findSeamDp, 60, seams, err), "carve() with dp failed: " + err);
        check(seams.size() == 60, "carve() should return 60 seams");
    }

    std::printf("[5] running time, one vertical seam (ms)\n");
    std::printf("  %-10s %10s %10s %10s\n", "image", "dp", "original", "greedy");
    for (auto wh : std::vector<std::pair<int, int>>{{100, 75}, {400, 300}, {800, 600}}) {
        const Image img = randomImage(wh.first, wh.second, 256, rng);
        Seam s;
        std::string err;
        double ms[3];
        const SeamFinder finders[3] = {findSeamDp, findSeamOriginal, findSeamGreedy};
        for (int k = 0; k < 3; ++k) {
            const int reps = 10;
            const auto t0 = std::chrono::steady_clock::now();
            for (int r = 0; r < reps; ++r) finders[k](img, s, err);
            ms[k] = millis(t0) / reps;
        }
        std::printf("  %-10s %10.2f %10.2f %10.2f\n",
                    (std::to_string(wh.first) + "x" + std::to_string(wh.second)).c_str(), ms[0], ms[1], ms[2]);
    }

    if (failures) {
        std::printf("\n%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("\nAll checks passed\n");
    return 0;
}
