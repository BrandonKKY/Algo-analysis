"""Energy map computation (Sobel gradient magnitude).

Kept separate from the seam-finding algorithms so the greedy, DP and graph-cut
implementations can all be compared on exactly the same energy function.
"""
from __future__ import annotations

import numpy as np

# ITU-R BT.601 luma weights for RGB -> grayscale
_LUMA = np.array([0.299, 0.587, 0.114])


def to_gray(image: np.ndarray) -> np.ndarray:
    """Convert an (H, W) or (H, W, 3) image to a float64 grayscale array."""
    img = np.asarray(image, dtype=np.float64)
    if img.ndim == 2:
        return img
    return img[..., :3] @ _LUMA


def sobel_energy(image: np.ndarray) -> np.ndarray:
    """Return e[i, j] = sqrt(Gx^2 + Gy^2) using 3x3 Sobel kernels.

    Borders are handled by replicating edge pixels, so the output has the
    same (H, W) shape as the input. Runs in O(H * W) time and space.
    """
    gray = to_gray(image)
    p = np.pad(gray, 1, mode="edge")

    # Gx kernel: [-1 0 1; -2 0 2; -1 0 1]
    gx = (p[:-2, 2:] + 2 * p[1:-1, 2:] + p[2:, 2:]) - (
        p[:-2, :-2] + 2 * p[1:-1, :-2] + p[2:, :-2]
    )
    # Gy kernel: [-1 -2 -1; 0 0 0; 1 2 1]
    gy = (p[2:, :-2] + 2 * p[2:, 1:-1] + p[2:, 2:]) - (
        p[:-2, :-2] + 2 * p[:-2, 1:-1] + p[:-2, 2:]
    )
    return np.hypot(gx, gy)
