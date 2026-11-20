// This work was completed with the assistance of Claude Sonnet 5.5.

#include "seam_methods.h"

SeamFinder lookupMethod(const std::string& id) {
    if (id == "original") return findSeamOriginal;
    if (id == "dp") return findSeamDp;
    if (id == "greedy") return findSeamGreedy;
    if (id == "graphcut") return findSeamGraphcut;
    return nullptr;
}
