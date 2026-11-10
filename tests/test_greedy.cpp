// Minimal self-contained unit tests (no framework needed).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

#include "energy.hpp"
#include "greedy.hpp"
#include "image.hpp"

static int failures = 0, checks = 0;
#define CHECK(cond)                                                          \
    do {                                                                     \
        ++checks;                                                            \
        if (!(cond)) {                                                       \
            ++failures;                                                      \
            std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                    \
    } while (0)

static std::mt19937 rng(0);

static Image randomImage(int w, int h) {
    Image img(w, h);
    std::uniform_int_distribution<int> d(0, 255);
    for (auto& b : img.px) b = static_cast<std::uint8_t>(d(rng));
    return img;
}

static Grid<double> randomEnergy(int w, int h) {
    Grid<double> e(w, h);
    std::uniform_real_distribution<double> d(0.0, 1.0);
    for (auto& x : e.v) x = d(rng);
    return e;
}

static bool connected(const std::vector<int>& s, int w) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] < 0 || s[i] >= w) return false;
        if (i && std::abs(s[i] - s[i - 1]) > 1) return false;
    }
    return true;
}

static void testFlatImageHasZeroEnergy() {
    Image img(9, 6);
    for (auto& b : img.px) b = 128;
    Grid<double> e = sobelEnergy(img);
    CHECK(e.w == 9 && e.h == 6);
    for (double x : e.v) CHECK(x == 0.0);
}

static void testFollowsZeroEnergyColumn() {
    Grid<double> e(8, 10, 1.0);
    for (int r = 0; r < 10; ++r) e(r, 3) = 0.0;
    auto s = findVerticalSeamGreedy(e);
    for (int c : s) CHECK(c == 3);
}

static void testSeamsValidOnRandomEnergy() {
    for (int t = 0; t < 20; ++t) {
        auto e = randomEnergy(40, 50);
        auto a = findVerticalSeamGreedy(e);
        auto b = findVerticalSeamGreedyMultistart(e);
        CHECK(a.size() == 50 && connected(a, 40));
        CHECK(b.size() == 50 && connected(b, 40));
    }
}

// The cheapest top pixel (col 0) leads into an expensive region, while col 4
// costs slightly more at the top but is free afterwards. Greedy takes the bait.
static void testGreedyIsLocalNotOptimal() {
    Grid<double> e(5, 6, 10.0);
    e(0, 0) = 0.0;
    e(0, 4) = 1.0;
    for (int r = 1; r < 6; ++r) e(r, 4) = 0.0;
    auto g = findVerticalSeamGreedy(e);
    CHECK(g[0] == 0);
    CHECK(seamCost(e, g) == 50.0);  // optimal is 1.0
    auto m = findVerticalSeamGreedyMultistart(e);
    CHECK(seamCost(e, m) == 1.0);
}

static void testMultistartNeverWorse() {
    for (int t = 0; t < 50; ++t) {
        auto e = randomEnergy(25, 30);
        double single = seamCost(e, findVerticalSeamGreedy(e));
        double multi = seamCost(e, findVerticalSeamGreedyMultistart(e));
        CHECK(multi <= single + 1e-9);
    }
}

static void testRemoveVerticalSeamValues() {
    Grid<int> g(4, 3);
    for (int i = 0; i < 12; ++i) g.v[i] = i;
    removeVerticalSeam(g, {0, 1, 3});
    CHECK(g.w == 3 && g.h == 3);
    CHECK((g.v == std::vector<int>{1, 2, 3, 4, 6, 7, 8, 9, 10}));
}

static void testCarveShapeAndSeams(GreedyMode mode) {
    Image img = randomImage(40, 30);
    auto res = carveGreedy(img, 32, 25, mode);
    CHECK(res.image.w == 32 && res.image.h == 25);
    CHECK(res.image.px.size() == std::size_t(32) * 25 * 3);
    CHECK(res.verticalSeams.size() == 8);
    CHECK(res.horizontalSeams.size() == 5);
    CHECK(res.seamCosts.size() == 13);
    // Every removed pixel is a distinct pixel of the original image.
    std::set<std::pair<int, int>> seen;
    std::size_t total = 0;
    for (const auto* list : {&res.verticalSeams, &res.horizontalSeams})
        for (const auto& seam : *list)
            for (const Point& p : seam) {
                CHECK(p.r >= 0 && p.r < 30 && p.c >= 0 && p.c < 40);
                seen.insert({p.r, p.c});
                ++total;
            }
    CHECK(seen.size() == total);
    CHECK(total == 8 * 30 + 5 * 32);
}

static void testCarveRemovesFlatStripe() {
    Image img = randomImage(15, 20);
    for (int r = 0; r < 20; ++r)
        for (int c = 7; c <= 9; ++c)
            for (int k = 0; k < 3; ++k) img.at(r, c)[k] = 50;
    auto res = carveGreedy(img, 14, 20);
    for (const Point& p : res.verticalSeams[0]) CHECK(p.c == 8);
}

static void testHorizontalEqualsTransposedVertical() {
    Image img = randomImage(22, 18);
    auto a = carveGreedy(img, 22, 14).image;
    auto b = transpose(carveGreedy(transpose(img), 14, 22).image);
    CHECK(a.w == b.w && a.h == b.h && a.px == b.px);
}

static void testInvalidTargetThrows() {
    Image img(10, 10);
    bool threw = false;
    try { carveGreedy(img, 11, 10); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { carveGreedy(img, 0, 10); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw);
}

int main() {
    testFlatImageHasZeroEnergy();
    testFollowsZeroEnergyColumn();
    testSeamsValidOnRandomEnergy();
    testGreedyIsLocalNotOptimal();
    testMultistartNeverWorse();
    testRemoveVerticalSeamValues();
    testCarveShapeAndSeams(GreedyMode::Min);
    testCarveShapeAndSeams(GreedyMode::Multistart);
    testCarveRemovesFlatStripe();
    testHorizontalEqualsTransposedVertical();
    testInvalidTargetThrows();

    std::printf("%d/%d checks passed\n", checks - failures, checks);
    return failures ? 1 : 0;
}
