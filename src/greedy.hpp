// Seam carving with a Greedy seam-selection strategy.
//
// Vertical seam (greedy):
//   1. Start at the minimum-energy pixel of the top row.
//   2. For each following row, look only at the 3 pixels adjacent to the
//      previous choice (c-1, c, c+1) and move to the one with the lowest energy.
//   3. Stop at the bottom row.
//
// This is a purely local, row-by-row decision: it never looks more than one
// row ahead, so it can walk into high-energy regions that the DP solution
// would avoid. Finding one seam costs O(H) once the energy map is known
// (vs O(W*H) for DP), but the energy map itself is still O(W*H) per seam.
//
// Modes:
//   Min        - pure greedy from the single lowest-energy top-row pixel.
//   Multistart - run the greedy walk from *every* top-row pixel and keep the
//                cheapest. Still greedy per path; O(W*H) per seam, O(H) memory.
//
// Horizontal seams: the image is transposed once, the same vertical-seam code
// is used, and the result is transposed back.
#pragma once

#include <vector>

#include "image.hpp"

enum class GreedyMode { Min, Multistart };

struct Point {
    int r, c;
};

// seam[r] = column of the pixel removed from row r; |seam[r] - seam[r-1]| <= 1.
std::vector<int> findVerticalSeamGreedy(const Grid<double>& e, int startCol = -1);
std::vector<int> findVerticalSeamGreedyMultistart(const Grid<double>& e);
std::vector<int> findVerticalSeam(const Grid<double>& e, GreedyMode mode);

double seamCost(const Grid<double>& e, const std::vector<int>& seam);

// Remove one pixel per row (in place). Width shrinks by 1.
void removeVerticalSeam(Image& img, const std::vector<int>& seam);
void removeVerticalSeam(Grid<int>& g, const std::vector<int>& seam);

struct CarveResult {
    Image image;
    // Each seam as (row, col) coordinates in the ORIGINAL image.
    std::vector<std::vector<Point>> verticalSeams;
    std::vector<std::vector<Point>> horizontalSeams;
    // Energy of each removed seam (vertical seams first, then horizontal).
    std::vector<double> seamCosts;
    double energyMs = 0;  // time computing energy maps
    double findMs = 0;    // time finding seams (the greedy part)
    double removeMs = 0;  // time removing seams + bookkeeping
    double totalMs = 0;
};

// Shrink `input` to targetW x targetH (vertical seams first, then horizontal).
// The energy map is recomputed after every removal.
// Throws std::invalid_argument if the target is larger than the input or < 1.
CarveResult carveGreedy(const Image& input, int targetW, int targetH,
                        GreedyMode mode = GreedyMode::Min, bool trackSeams = true);

// Original image with removed seams drawn on it (red = vertical, blue = horizontal).
Image drawSeams(const Image& original, const CarveResult& res);
