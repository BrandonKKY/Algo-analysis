#include "seam_methods.h"
#include <algorithm>

// Dynamic programming seam finder
// Using sobelEnergy(), convert the image into an energy map e
// Using a recurring function, accumulate the costs of each seam from top to bottom pixel by pixel
// based on the energy map. Simultaneously, store the respective costs for each pixel in a cost map M
// After calculating the cumulative costs, work backwards up from the bottom to then remove the 
// cheapest pixel in each row one by one
bool findSeamDp(const Image& img, Seam& seam, std::string& error) {
    // Get the energy map for the image
    const int w = img.w, h = img.h;
    if (w < 2 || h < 1) { error = "image too small to carve"; return false;}
    // Map e(y, x)
    const Grid<double> e = sobelEnergy(img);

    // Fill cumulative-cost table M
    // M(y, x) = cost of the cheapest seam running down from top to bottom pixel
    // Starting from the top row pixel, M(0, x) = e(0, x)
    // Get the cost of the next cheapest neighboring pixel next rown down and add it to the current cost
    // M(y, x) = e(y, x) + min( M(y-1, x-1), M(y-1, x), M(y-1, x+1) ) 
    // Skip any neighbour that falls outside the image: there's no x-1 when x == 0, and no x+1 when x == w-1.
    Grid<double> M(w,h);
    // Copy the first row e(0,x)
    for(int x = 0; x < w; ++x)
    {
        M(0,x) = e(0,x);
    }

    for(int y = 1; y < h; ++y)
    {
        for(int x = 0; x < w; ++x)
        {
            double best = M(y-1, x);
            // Compare with left neighbor M(y-1, x-1) if x > 0
            if (x > 0) {
                best = std::min(best, M(y-1, x-1));
            }
            // Compare with right neighbor M(y-1, x+1) if x < w-1
            if (x < w-1) {
                best = std::min(best, M(y-1, x+1));
            }
            M(y,x) = e(y,x) + best;
        }
    }
    // The cheapest full seam ends at the smallest value in the bottom row
    // So start from the first in the bottom row
    seam.assign(h,0);
    int x = 0;
    for(int i = 1; i < w; ++i)
    {
        // assign x to the i with the smallest cost
        x = M(h-1, i) < M(h-1, x) ? i : x;
    }
    
    // Start the seam at the bottom row with the smallest cost found at x
    seam[h-1] = x;

    // After finding the seam with the smallest cost, backtrack upwards
    for ( int y = h-2; y >= 0; --y)
    {
        int bx = x;
        // Check which neighbour above the current pixel is the smallest M
        if (x > 0 && M(y, x-1) < M(y, bx)) bx = x-1;
        if (x < w-1 && M(y, x+1) < M(y, bx)) bx = x+1;
        // Assign x to the above cheapest pixel
        x = bx;
        seam[y] = x;
    }

    return true;

}
