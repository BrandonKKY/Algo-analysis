#pragma once
#include <string>

#include "seam_carving.h"

// Student-implemented methods (stubs until filled in).
bool findSeamDp(const Image& img, Seam& seam, std::string& error);
bool findSeamGreedy(const Image& img, Seam& seam, std::string& error);
bool findSeamGraphcut(const Image& img, Seam& seam, std::string& error);

// Maps the UI method id ("original", "dp", "greedy", "graphcut") to a finder.
// Returns nullptr for an unknown id.
SeamFinder lookupMethod(const std::string& id);
