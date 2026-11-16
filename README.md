# Algo-analysis

Seam carving (content-aware image resizing) in C++17.

## Greedy seam carving

Code: [src/greedy.hpp](src/greedy.hpp) / [src/greedy.cpp](src/greedy.cpp). The energy map is in [src/energy.cpp](src/energy.cpp) (Sobel gradient magnitude). Image loading and saving use the single-header [stb](https://github.com/nothings/stb) libraries in `third_party/`, so nothing needs to be installed except a C++ compiler.

For each seam: compute the energy map, start at the lowest-energy pixel in the top row, then go down one row at a time, each time moving to whichever of the 3 adjacent pixels below (left, straight, right) has the lowest energy. Remove that seam and repeat. Horizontal seams are found by transposing the image and reusing the vertical-seam code.

Modes:
- `min` (default): pure greedy from the single lowest-energy top pixel.
- `multistart`: runs the greedy walk from every top pixel and keeps the cheapest one.

### Build and run

**With make (g++ / clang, Linux / macOS / MSYS2):**

```
make                                              # builds build/greedy_carve
make greedy IMG=images/castle.jpg WIDTH=400 HEIGHT=300
make greedy-multistart IMG=images/castle.jpg WIDTH=400
make test
make clean
```

**With CMake (works with Visual Studio on Windows):**

```
cmake -B build
cmake --build build --config Release
build/Release/greedy_carve images/castle.jpg --width 400 --height 300
build/Release/test_greedy
```

(With a single-config generator such as Ninja or Makefiles, the executables are in `build/` rather than `build/Release/`.)

**Command-line options:**

```
greedy_carve <input> [--width W] [--height H] [--mode min|multistart] [--out-dir DIR]
```

At least one of `--width` / `--height` is required. The tool only shrinks images.

Output goes to `output/greedy/` by default:
- `<name>_greedy_<mode>.png`: the resized image
- `<name>_greedy_<mode>_seams.png`: all removed seams drawn on the original (red = vertical, blue = horizontal)
- `<name>_greedy_<mode>_stats.json`: timings (energy / seam finding / removal) and the energy of every removed seam, for comparing against DP and graph cut

### Using it from other code

```cpp
#include "greedy.hpp"
CarveResult res = carveGreedy(image, targetW, targetH, GreedyMode::Min);
// res.image, res.verticalSeams, res.horizontalSeams, res.seamCosts,
// res.energyMs, res.findMs, res.removeMs, res.totalMs
```

`sobelEnergy()`, `Image`, `Grid<T>`, `loadImage()`/`savePng()` and `removeVerticalSeam()` can be reused by the DP and graph-cut implementations so that all three use the same energy function.
