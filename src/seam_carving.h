#pragma once
#include <string>
#include <vector>

#include "image.h"

// A vertical seam: seam[y] is the column removed from row y.
using Seam = std::vector<int>;

// Finds ONE vertical seam in the given image. Returns false (and sets `error`)
// if the method cannot do so (e.g. not implemented yet).
using SeamFinder = bool (*)(const Image&, Seam& seam, std::string& error);

// Energy map (Avidan & Shamir e1: |dI/dx| + |dI/dy|), row-major, width*height.
std::vector<double> computeEnergy(const Image& img);

// Original seam carving (Avidan & Shamir, SIGGRAPH 2007): cumulative-cost
// table over the e1 energy, then backtrack from the cheapest bottom cell.
// This is the REFERENCE implementation.
bool findSeamOriginal(const Image& img, Seam& seam, std::string& error);

// Returns a copy of `img` with the seam removed.
Image removeSeam(const Image& img, const Seam& seam);

// Repeatedly finds and removes `count` seams using `finder`.
// `seams[i]` is expressed in the coordinates of the image *after* i removals.
bool carve(const Image& img, SeamFinder finder, int count,
           std::vector<Seam>& seams, std::string& error);
