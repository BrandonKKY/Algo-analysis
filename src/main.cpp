// Greedy seam carving tool.
//
// Command line:
//   greedy_carve <input> [--width W] [--height H] [--mode min|multistart] [--out-dir DIR]
//
// Interactive: double-click greedy_carve.exe (or drag an image onto it) and it
// asks for the image, target size and mode, then waits for Enter before closing.
//
// Writes to DIR (default output/greedy):
//   <name>_greedy_<mode>.png        resized image
//   <name>_greedy_<mode>_seams.png  removed seams on the original (red = vertical, blue = horizontal)
//   <name>_greedy_<mode>_stats.json timings + energy of every removed seam
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "greedy.hpp"
#include "image.hpp"

namespace fs = std::filesystem;

static int usage(const char* msg = nullptr) {
    if (msg) std::cerr << "error: " << msg << "\n\n";
    std::cerr << "usage: greedy_carve <input> [--width W] [--height H]\n"
                 "                    [--mode min|multistart] [--out-dir DIR]\n"
                 "  At least one of --width / --height is required.\n"
                 "  min        = pure greedy from the lowest-energy top pixel (default)\n"
                 "  multistart = greedy from every top pixel, keep the cheapest seam\n"
                 "  Run with no arguments for interactive mode.\n";
    return 2;
}

static std::string jsonEscape(const std::string& s) {
    std::string o;
    for (char ch : s) {
        if (ch == '\\' || ch == '"') o += '\\';
        o += ch;
    }
    return o;
}

static std::string trim(std::string s) {
    if (s.rfind("\xEF\xBB\xBF", 0) == 0) s.erase(0, 3);  // UTF-8 byte-order mark from piped input
    while (!s.empty() && (s.back() == ' ' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
    while (!s.empty() && s.front() == ' ') s.erase(s.begin());
    // Dragging a file into a Windows console wraps the path in quotes.
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
    return s;
}

static std::string ask(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) return "";
    return trim(line);
}

// Ask for a size in [1, max]; empty input keeps `max` (no change).
static int askSize(const std::string& name, int max) {
    for (;;) {
        std::string s = ask("Target " + name + " (1-" + std::to_string(max) +
                            ", Enter = keep " + std::to_string(max) + "): ");
        if (s.empty()) return max;
        try {
            int v = std::stoi(s);
            if (v >= 1 && v <= max) return v;
        } catch (const std::exception&) {
        }
        std::cout << "  Please enter a whole number between 1 and " << max << ".\n";
    }
}

static int run(const std::string& input, int width, int height, const std::string& modeStr,
               const std::string& outDir, bool interactive) {
    GreedyMode mode = modeStr == "multistart" ? GreedyMode::Multistart : GreedyMode::Min;

    Image img;
    std::string err;
    if (!loadImage(input, img, err)) {
        std::cerr << "error: could not read " << input << ": " << err << "\n";
        return 1;
    }
    std::cout << "Input  : " << input << " (" << img.w << "x" << img.h << ")\n";

    if (interactive) {
        width = askSize("width", img.w);
        height = askSize("height", img.h);
        std::string m = ask("Mode - min or multistart (Enter = min): ");
        if (m == "multistart") mode = GreedyMode::Multistart;
    }
    const std::string modeName = mode == GreedyMode::Multistart ? "multistart" : "min";
    if (width < 0) width = img.w;
    if (height < 0) height = img.h;

    std::cout << "Carving to " << width << "x" << height << " (" << modeName << ")...\n";
    CarveResult res;
    try {
        res = carveGreedy(img, width, height, mode);
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }

    fs::create_directories(outDir);
    std::string stem = fs::path(input).stem().string() + "_greedy_" + modeName;
    fs::path dir = fs::absolute(outDir);
    fs::path outImg = dir / (stem + ".png");
    fs::path outSeams = dir / (stem + "_seams.png");
    fs::path outStats = dir / (stem + "_stats.json");

    if (!savePng(outImg.string(), res.image) || !savePng(outSeams.string(), drawSeams(img, res))) {
        std::cerr << "error: could not write output images to " << dir.string() << "\n";
        return 1;
    }

    std::size_t n = res.seamCosts.size();
    double totalEnergy = 0;
    for (double c : res.seamCosts) totalEnergy += c;
    double meanEnergy = n ? totalEnergy / n : 0.0;

    std::ofstream js(outStats);
    js.precision(10);
    js << "{\n"
       << "  \"algorithm\": \"greedy\",\n"
       << "  \"mode\": \"" << modeName << "\",\n"
       << "  \"input\": \"" << jsonEscape(input) << "\",\n"
       << "  \"original_size\": [" << img.w << ", " << img.h << "],\n"
       << "  \"output_size\": [" << res.image.w << ", " << res.image.h << "],\n"
       << "  \"vertical_seams_removed\": " << res.verticalSeams.size() << ",\n"
       << "  \"horizontal_seams_removed\": " << res.horizontalSeams.size() << ",\n"
       << "  \"total_time_ms\": " << res.totalMs << ",\n"
       << "  \"energy_time_ms\": " << res.energyMs << ",\n"
       << "  \"seam_find_time_ms\": " << res.findMs << ",\n"
       << "  \"seam_remove_time_ms\": " << res.removeMs << ",\n"
       << "  \"avg_seam_find_time_ms\": " << (n ? res.findMs / n : 0.0) << ",\n"
       << "  \"total_seam_energy\": " << totalEnergy << ",\n"
       << "  \"mean_seam_energy\": " << meanEnergy << ",\n"
       << "  \"seam_costs\": [";
    for (std::size_t i = 0; i < n; ++i) js << (i ? ", " : "") << res.seamCosts[i];
    js << "]\n}\n";

    std::printf("Output : %s (%dx%d)\n", outImg.string().c_str(), res.image.w, res.image.h);
    std::printf("Seams  : %s  (red = vertical, blue = horizontal)\n", outSeams.string().c_str());
    std::printf("Stats  : %s\n", outStats.string().c_str());
    std::printf("Time   : total %.1f ms | energy %.1f ms | find %.1f ms | remove %.1f ms\n",
                res.totalMs, res.energyMs, res.findMs, res.removeMs);
    std::printf("Energy : total removed %.1f, mean per seam %.1f\n", totalEnergy, meanEnergy);
    return 0;
}

int main(int argc, char** argv) {
    std::string input, modeStr = "min", outDir = "output/greedy";
    int width = -1, height = -1;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        try {
            if (a == "--width") { const char* v = next(); if (!v) return usage("--width needs a value"); width = std::stoi(v); }
            else if (a == "--height") { const char* v = next(); if (!v) return usage("--height needs a value"); height = std::stoi(v); }
            else if (a == "--mode") { const char* v = next(); if (!v) return usage("--mode needs a value"); modeStr = v; }
            else if (a == "--out-dir") { const char* v = next(); if (!v) return usage("--out-dir needs a value"); outDir = v; }
            else if (a == "-h" || a == "--help") return usage();
            else if (!a.empty() && a[0] == '-') return usage(("unknown option " + a).c_str());
            else if (input.empty()) input = a;
            else return usage("only one input image allowed");
        } catch (const std::exception&) {
            return usage(("bad number for " + a).c_str());
        }
    }
    if (modeStr != "min" && modeStr != "multistart")
        return usage("--mode must be 'min' or 'multistart'");

    // Interactive when double-clicked (no args) or an image was dropped onto the exe.
    bool interactive = argc == 1 || (argc == 2 && !input.empty());
    if (!interactive) {
        if (input.empty()) return usage("no input image given");
        if (width < 0 && height < 0) return usage("give at least one of --width / --height");
        return run(input, width, height, modeStr, outDir, false);
    }

    std::cout << "=== Greedy seam carving ===\n";
    if (input.empty()) {
        input = ask("Image path (you can drag the image into this window): ");
        if (!input.empty() && !fs::exists(input) && fs::exists(fs::path("..") / input))
            input = (fs::path("..") / input).string();  // allow paths relative to the repo root
    }
    int rc = input.empty() ? (std::cerr << "error: no image given\n", 1)
                           : run(input, width, height, modeStr, outDir, true);
    std::cout << "\nPress Enter to close..." << std::flush;
    std::string dummy;
    std::getline(std::cin, dummy);
    return rc;
}
