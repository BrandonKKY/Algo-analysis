#pragma once
#include <string>

#include "seam_carving.h"

// Student-implemented methods (stubs until filled in).
bool findSeamDp(const Image& img, Seam& seam, std::string& error);
bool findHorizontalSeamDp(const Image& img, Seam& seam, std::string& error);
bool findSeamGreedy(const Image& img, Seam& seam, std::string& error);
bool findSeamGraphcut(const Image& img, Seam& seam, std::string& error);

// Graph cut, horizontal seams: the same method run on the transposed image.
// seam[x] is the row removed from column x (seam.size() == img.w).
Image transposeImage(const Image& img);
bool findHorizontalSeamGraphcut(const Image& img, Seam& seam, std::string& error);

// Maps the UI method id ("original", "dp", "greedy", "graphcut") to a finder.
// Returns nullptr for an unknown id.
SeamFinder lookupMethod(const std::string& id);
