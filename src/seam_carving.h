// This work was completed with the assistance of Claude Sonnet 5.5.

#pragma once
#include <string>
#include <vector>

#include "energy.hpp"
#include "image.hpp"

// A vertical seam: seam[y] is the column removed from row y.
using Seam = std::vector<int>;

// Finds ONE vertical seam in the given image. Returns false (and sets `error`)
// if the method cannot do so (e.g. the image is too small, or it was stopped).
using SeamFinder = bool (*)(const Image&, Seam& seam, std::string& error);

// Original seam carving (Avidan & Shamir, SIGGRAPH 2007): cumulative-cost
// table over the Sobel energy, then backtrack from the cheapest bottom cell.
// This is the REFERENCE implementation.
bool findSeamOriginal(const Image& img, Seam& seam, std::string& error);

// Returns a copy of `img` with the seam removed.
Image removeSeam(const Image& img, const Seam& seam);

// Repeatedly finds and removes `count` seams using `finder`.
// `seams[i]` is expressed in the coordinates of the image *after* i removals.
// Returns false with error "stopped" if stopAll() is called while it runs.
bool carve(const Image& img, SeamFinder finder, int count,
           std::vector<Seam>& seams, std::string& error);

// Stops every carve() that is currently running, on any thread.
void stopAll();

// True if stopAll() was called after the current thread's carve() started.
// Always false outside carve(). Slow finders (graph cut) check it while they
// work so a stop takes effect mid-seam instead of after it.
bool stopRequested();
