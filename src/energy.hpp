// Energy map: Sobel gradient magnitude.
// Kept separate from the seam-finding code so every algorithm (greedy, DP,
// graph cut) can be compared on exactly the same energy function.
#pragma once

#include "image.hpp"

// e(r, c) = sqrt(Gx^2 + Gy^2) on the grayscale image, with 3x3 Sobel kernels.
// Borders replicate the edge pixel. O(W*H) time and space.
Grid<double> sobelEnergy(const Image& img);
