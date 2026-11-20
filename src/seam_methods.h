// This work was completed with the assistance of Claude Sonnet 5.5.

#pragma once
#include <string>

#include "seam_carving.h"

// The team's seam finders. Each finds ONE vertical seam: seam[y] is the column
// removed from row y (seam.size() == img.h, adjacent rows differ by at most 1).
bool findSeamDp(const Image& img, Seam& seam, std::string& error);        // dynamic programming (seam_dp.cpp)
bool findSeamGreedy(const Image& img, Seam& seam, std::string& error);    // greedy walk (seam_greedy.cpp)
bool findSeamGraphcut(const Image& img, Seam& seam, std::string& error);  // min-cut / max-flow (seam_graphcut.cpp)

// Dynamic programming, horizontal seams: run left to right instead of top to
// bottom. seam[x] is the row removed from column x (seam.size() == img.w).
bool findHorizontalSeamDp(const Image& img, Seam& seam, std::string& error);

// Graph cut, horizontal seams: the same method run on the transposed image.
// seam[x] is the row removed from column x (seam.size() == img.w).
Image transposeImage(const Image& img);
bool findHorizontalSeamGraphcut(const Image& img, Seam& seam, std::string& error);

// Maps the UI method id ("original", "dp", "greedy", "graphcut") to a finder.
// Returns nullptr for an unknown id.
SeamFinder lookupMethod(const std::string& id);
