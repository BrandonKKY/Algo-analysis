#include "seam_methods.h"
#include <algorithm>

// Dynamic programming seam finder
// Using sobelEnergy(), convert the image into an energy map e
// Using a recurrence (filled in with loops, not recursion), accumulate the costs of each seam from
// top to bottom pixel by pixel based on the energy map, storing the cost for each pixel in a cost map M
// After calculating the cumulative costs, start at the cheapest pixel in the bottom row and walk back
// up, each time moving to the cheapest of the three neighbours above. That path is the seam to remove
bool findSeamDp(const Image& img, Seam& seam, std::string& error) {
    // Get the energy map for the image
    const int w = img.w, h = img.h;
    if (w < 2 || h < 1) { error = "image too small to carve"; return false;}
    // Map e(y, x)
    const Grid<double> e = sobelEnergy(img);

    // Fill cumulative-cost table M
    // M(y, x) = cost of the cheapest seam running down from top to bottom pixel
    // Starting from the top row pixel, M(0, x) = e(0, x)
    // For each pixel, take the cheapest of its three neighbours in the row above and add its own energy
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
    // So scan the bottom row for it, starting from the first pixel
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

// Horizontal seam finder (same DP, run left to right instead of top to bottom)
// seam[x] = the row removed from column x (seam.size() == img.w)
// M(y, x) = cost of the cheapest seam running from the left column to pixel (y, x)
// Starting from the left column pixel, M(y, 0) = e(y, 0)
// M(y, x) = e(y, x) + min( M(y-1, x-1), M(y, x-1), M(y+1, x-1) )
// Skip any neighbour that falls outside the image: there's no y-1 when y == 0, and no y+1 when y == h-1.
bool findHorizontalSeamDp(const Image& img, Seam& seam, std::string& error) {
    const int w = img.w, h = img.h;
    if (h < 2 || w < 1) { error = "image too small to carve"; return false;}
    // Map e(y, x)
    const Grid<double> e = sobelEnergy(img);

    Grid<double> M(w,h);
    // Copy the first column e(y,0)
    for(int y = 0; y < h; ++y)
    {
        M(y,0) = e(y,0);
    }

    // Fill column by column, left to right
    for(int x = 1; x < w; ++x)
    {
        for(int y = 0; y < h; ++y)
        {
            double best = M(y, x-1);
            // Compare with upper neighbor M(y-1, x-1) if y > 0
            if (y > 0) {
                best = std::min(best, M(y-1, x-1));
            }
            // Compare with lower neighbor M(y+1, x-1) if y < h-1
            if (y < h-1) {
                best = std::min(best, M(y+1, x-1));
            }
            M(y,x) = e(y,x) + best;
        }
    }

    // The cheapest full seam ends at the smallest value in the rightmost column
    seam.assign(w,0);
    int y = 0;
    for(int i = 1; i < h; ++i)
    {
        y = M(i, w-1) < M(y, w-1) ? i : y;
    }

    // Start the seam at the rightmost column with the smallest cost found at y
    seam[w-1] = y;

    // After finding the seam with the smallest cost, backtrack leftwards
    for (int x = w-2; x >= 0; --x)
    {
        int by = y;
        // Check which neighbour to the left of the current pixel has the smallest M
        if (y > 0 && M(y-1, x) < M(by, x)) by = y-1;
        if (y < h-1 && M(y+1, x) < M(by, x)) by = y+1;
        y = by;
        seam[x] = y;
    }

    return true;
}
