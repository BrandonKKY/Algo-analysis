#include "seam_methods.h"

// Dynamic programming seam finder -- TO BE IMPLEMENTED BY YOU.
// Find ONE vertical seam of `img` and store the removed column of each row
// in seam[y] (seam.size() == img.height, adjacent rows differ by at most 1).
// You can reuse computeEnergy() from seam_carving.h.
bool findSeamDp(const Image& img, Seam& seam, std::string& error) {
    (void)img;
    (void)seam;
    error = "Dynamic programming method is not implemented yet (src/seam_dp.cpp)";
    return false;
}
