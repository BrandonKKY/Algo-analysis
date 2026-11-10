# Algo-analysis

Seam carving (content-aware image resizing).

## Greedy seam carving

Code: [seam_carving/greedy.py](seam_carving/greedy.py), energy map in [seam_carving/energy.py](seam_carving/energy.py) (Sobel gradient magnitude).

For each seam: compute the energy map, start at the lowest-energy pixel in the top row, then go down one row at a time, each time moving to whichever of the 3 adjacent pixels below (left, straight, right) has the lowest energy. Remove that seam and repeat. Horizontal seams reuse the same code on the transposed image.

Modes:
- `min` (default): pure greedy from the single lowest-energy top pixel.
- `multistart`: runs the greedy walk from every top pixel and keeps the cheapest one.

### Setup

Requires Python 3.9+.

```
pip install -r requirements.txt
```

### Run

With `make`:

```
make greedy IMG=images/castle.jpg WIDTH=400 HEIGHT=300
make greedy-multistart IMG=images/castle.jpg WIDTH=400
make test
```

Without `make` (e.g. Windows):

```
python -m seam_carving.greedy_cli images/castle.jpg --width 400 --height 300
python -m seam_carving.greedy_cli images/castle.jpg --width 400 --mode multistart
python -m pytest -q tests
```

Output goes to `output/greedy/`:
- `<name>_greedy_<mode>.png`: the resized image
- `<name>_greedy_<mode>_seams.png`: all removed seams drawn on the original (red = vertical, blue = horizontal)
- `<name>_greedy_<mode>_stats.json`: timings (energy / seam-finding / removal) and the energy of every removed seam, for comparing against DP and graph cut

### Using it from Python

```python
from seam_carving.greedy import carve
res = carve(image_rgb_array, target_width=400, target_height=300, mode="min")
res.image, res.vertical_seams, res.seam_costs, res.find_time, res.total_time
```
