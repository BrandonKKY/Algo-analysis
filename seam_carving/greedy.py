"""Seam carving with a Greedy seam-selection strategy.

Vertical seam (greedy):
    1. Start at the minimum-energy pixel of the top row.
    2. For each following row, look only at the 3 pixels adjacent to the
       previous choice (j-1, j, j+1) and pick the one with the lowest energy.
    3. Stop at the bottom row.

This is a purely local, row-by-row decision: it never looks further than one
row ahead, so it can walk into high-energy regions that the DP solution would
avoid. Finding one seam costs O(H) once the energy map is known (vs O(H*W)
for DP), but the energy map itself is still O(H*W) per iteration.

Two modes are provided:
    "min"        - pure greedy from the single lowest-energy top-row pixel.
    "multistart" - run the greedy walk from *every* top-row pixel (vectorised)
                   and keep the cheapest. Still greedy per path, O(H*W) per seam.

Horizontal seams are found by transposing the image and reusing the
vertical-seam code.
"""
from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Callable

import numpy as np

from .energy import sobel_energy

EnergyFn = Callable[[np.ndarray], np.ndarray]
MODES = ("min", "multistart")


# --------------------------------------------------------------------------- #
# Seam finding
# --------------------------------------------------------------------------- #
def find_vertical_seam_greedy(energy: np.ndarray, start: int | None = None) -> np.ndarray:
    """Greedy vertical seam.

    Returns an int array `seam` of length H where seam[i] is the column of the
    pixel removed from row i. |seam[i] - seam[i-1]| <= 1 for every i.

    Ties prefer going straight down, then left, then right.
    Time: O(H) (+ O(W) for choosing the start). Extra space: O(H).
    """
    h, w = energy.shape
    seam = np.empty(h, dtype=np.intp)
    j = int(np.argmin(energy[0])) if start is None else int(start)
    seam[0] = j
    for i in range(1, h):
        row = energy[i]
        best = j
        if j > 0 and row[j - 1] < row[best]:
            best = j - 1
        if j < w - 1 and row[j + 1] < row[best]:
            best = j + 1
        j = best
        seam[i] = j
    return seam


def find_vertical_seam_greedy_multistart(energy: np.ndarray) -> np.ndarray:
    """Run the greedy walk from every top-row pixel at once; return the cheapest.

    Each individual path is still chosen greedily (one row of look-ahead).
    Time: O(H * W). Space: O(H * W) for the candidate paths.
    """
    h, w = energy.shape
    j = np.arange(w)
    paths = np.empty((h, w), dtype=np.intp)
    paths[0] = j
    totals = energy[0].astype(np.float64).copy()
    for i in range(1, h):
        row = energy[i]
        left = np.maximum(j - 1, 0)
        right = np.minimum(j + 1, w - 1)
        best = j.copy()
        best_e = row[j].copy()
        e_left = row[left]
        m = e_left < best_e
        best[m], best_e[m] = left[m], e_left[m]
        e_right = row[right]
        m = e_right < best_e
        best[m], best_e[m] = right[m], e_right[m]
        j = best
        paths[i] = j
        totals += best_e
    return paths[:, int(np.argmin(totals))].copy()


def find_vertical_seam(energy: np.ndarray, mode: str = "min") -> np.ndarray:
    if mode == "min":
        return find_vertical_seam_greedy(energy)
    if mode == "multistart":
        return find_vertical_seam_greedy_multistart(energy)
    raise ValueError(f"unknown mode {mode!r}; expected one of {MODES}")


def find_horizontal_seam(energy: np.ndarray, mode: str = "min") -> np.ndarray:
    """Horizontal seam: seam[j] is the row removed from column j."""
    return find_vertical_seam(energy.T, mode)


def seam_cost(energy: np.ndarray, seam: np.ndarray) -> float:
    """Total energy of a vertical seam (sum of e[i, seam[i]])."""
    return float(energy[np.arange(len(seam)), seam].sum())


# --------------------------------------------------------------------------- #
# Seam removal
# --------------------------------------------------------------------------- #
def remove_vertical_seam(arr: np.ndarray, seam: np.ndarray) -> np.ndarray:
    """Remove one pixel per row. Works for (H, W) and (H, W, C) arrays."""
    h, w = arr.shape[:2]
    mask = np.ones((h, w), dtype=bool)
    mask[np.arange(h), seam] = False
    return arr[mask].reshape((h, w - 1) + arr.shape[2:])


def remove_horizontal_seam(arr: np.ndarray, seam: np.ndarray) -> np.ndarray:
    """Remove one pixel per column (seam[j] = row index)."""
    return np.swapaxes(remove_vertical_seam(np.swapaxes(arr, 0, 1), seam), 0, 1)


# --------------------------------------------------------------------------- #
# Full resize loop
# --------------------------------------------------------------------------- #
@dataclass
class CarveResult:
    image: np.ndarray
    # Each seam as an (N, 2) array of (row, col) coordinates in the ORIGINAL image.
    vertical_seams: list[np.ndarray] = field(default_factory=list)
    horizontal_seams: list[np.ndarray] = field(default_factory=list)
    # Energy of each removed seam (in removal order: vertical first, then horizontal).
    seam_costs: list[float] = field(default_factory=list)
    energy_time: float = 0.0   # seconds spent computing energy maps
    find_time: float = 0.0     # seconds spent finding seams (the greedy part)
    remove_time: float = 0.0   # seconds spent removing seams / bookkeeping
    total_time: float = 0.0


def carve(
    image: np.ndarray,
    target_width: int | None = None,
    target_height: int | None = None,
    mode: str = "min",
    energy_fn: EnergyFn = sobel_energy,
    track_seams: bool = True,
) -> CarveResult:
    """Shrink `image` to (target_height, target_width) by greedy seam removal.

    Vertical seams are removed first (width), then horizontal seams (height).
    The energy map is recomputed after every removal, as required.
    """
    if mode not in MODES:
        raise ValueError(f"unknown mode {mode!r}; expected one of {MODES}")
    img = np.asarray(image)
    h, w = img.shape[:2]
    tw = w if target_width is None else int(target_width)
    th = h if target_height is None else int(target_height)
    if not (1 <= tw <= w and 1 <= th <= h):
        raise ValueError(
            f"target size {tw}x{th} must be between 1x1 and the original {w}x{h} "
            "(this implementation only removes seams)"
        )

    res = CarveResult(image=img)
    # Maps from current pixel -> original (row, col), used to draw seams on the original.
    rmap, cmap = np.indices((h, w)) if track_seams else (None, None)
    t_start = time.perf_counter()

    def step(img, rmap, cmap, horizontal: bool):
        if horizontal:  # work in transposed space, reuse vertical code
            img = np.swapaxes(img, 0, 1)
            if track_seams:
                rmap, cmap = rmap.T, cmap.T

        t0 = time.perf_counter()
        energy = energy_fn(img)
        t1 = time.perf_counter()
        seam = find_vertical_seam(energy, mode)
        t2 = time.perf_counter()

        res.seam_costs.append(seam_cost(energy, seam))
        if track_seams:
            rows = np.arange(len(seam))
            coords = np.stack([rmap[rows, seam], cmap[rows, seam]], axis=1)
            (res.horizontal_seams if horizontal else res.vertical_seams).append(coords)
            rmap = remove_vertical_seam(rmap, seam)
            cmap = remove_vertical_seam(cmap, seam)
        img = remove_vertical_seam(img, seam)
        t3 = time.perf_counter()

        res.energy_time += t1 - t0
        res.find_time += t2 - t1
        res.remove_time += t3 - t2

        if horizontal:
            img = np.swapaxes(img, 0, 1)
            if track_seams:
                rmap, cmap = rmap.T, cmap.T
        return img, rmap, cmap

    for _ in range(w - tw):
        img, rmap, cmap = step(img, rmap, cmap, horizontal=False)
    for _ in range(h - th):
        img, rmap, cmap = step(img, rmap, cmap, horizontal=True)

    res.total_time = time.perf_counter() - t_start
    res.image = np.ascontiguousarray(img)
    return res


# --------------------------------------------------------------------------- #
# Visualisation
# --------------------------------------------------------------------------- #
def draw_seams(
    original: np.ndarray,
    vertical_seams: list[np.ndarray],
    horizontal_seams: list[np.ndarray] = (),
    v_color=(255, 0, 0),
    h_color=(0, 0, 255),
) -> np.ndarray:
    """Overlay all removed seams on a copy of the original RGB image."""
    out = np.array(original, copy=True)
    if out.ndim == 2:
        out = np.stack([out] * 3, axis=-1)
    for coords in vertical_seams:
        out[coords[:, 0], coords[:, 1], :3] = v_color
    for coords in horizontal_seams:
        out[coords[:, 0], coords[:, 1], :3] = h_color
    return out
