import numpy as np
import pytest

from seam_carving.energy import sobel_energy
from seam_carving.greedy import (
    carve,
    find_vertical_seam_greedy,
    find_vertical_seam_greedy_multistart,
    remove_horizontal_seam,
    remove_vertical_seam,
    seam_cost,
)

rng = np.random.default_rng(0)


def is_connected(seam):
    return np.all(np.abs(np.diff(seam)) <= 1)


def test_energy_shape_and_flat_image():
    img = np.full((6, 9, 3), 128, dtype=np.uint8)
    e = sobel_energy(img)
    assert e.shape == (6, 9)
    assert np.allclose(e, 0)


def test_follows_zero_energy_column():
    e = np.ones((10, 8))
    e[:, 3] = 0
    assert np.array_equal(find_vertical_seam_greedy(e), np.full(10, 3))


def test_seam_valid_on_random_energy():
    e = rng.random((50, 40))
    for seam in (find_vertical_seam_greedy(e), find_vertical_seam_greedy_multistart(e)):
        assert seam.shape == (50,)
        assert seam.min() >= 0 and seam.max() < 40
        assert is_connected(seam)


def test_greedy_is_local_not_optimal():
    # Cheapest top pixel (col 0) leads into an expensive region; col 4 is a
    # slightly more expensive start but a zero-cost path afterwards.
    e = np.full((6, 5), 10.0)
    e[0, 0] = 0.0
    e[0, 4] = 1.0
    e[1:, 4] = 0.0
    seam = find_vertical_seam_greedy(e)
    assert seam[0] == 0
    assert seam_cost(e, seam) > 1.0
    multi = find_vertical_seam_greedy_multistart(e)
    assert seam_cost(e, multi) == pytest.approx(1.0)


def test_multistart_never_worse_than_single_start():
    for _ in range(20):
        e = rng.random((30, 25))
        single = seam_cost(e, find_vertical_seam_greedy(e))
        multi = seam_cost(e, find_vertical_seam_greedy_multistart(e))
        assert multi <= single + 1e-9


def test_remove_vertical_seam_values():
    a = np.arange(12).reshape(3, 4)
    out = remove_vertical_seam(a, np.array([0, 1, 3]))
    assert out.tolist() == [[1, 2, 3], [4, 6, 7], [8, 9, 10]]


def test_remove_horizontal_seam_values():
    a = np.arange(12).reshape(4, 3)
    out = remove_horizontal_seam(a, np.array([0, 1, 3]))
    # col 0 drops row 0, col 1 drops row 1, col 2 drops row 3
    assert out.tolist() == [[3, 1, 2], [6, 7, 5], [9, 10, 8]]


@pytest.mark.parametrize("mode", ["min", "multistart"])
def test_carve_output_shape_and_seams(mode):
    img = rng.integers(0, 256, size=(30, 40, 3), dtype=np.uint8)
    res = carve(img, 32, 25, mode=mode)
    assert res.image.shape == (25, 32, 3)
    assert len(res.vertical_seams) == 8
    assert len(res.horizontal_seams) == 5
    # Every removed pixel is a distinct pixel of the original image.
    all_px = np.concatenate(res.vertical_seams + res.horizontal_seams)
    assert len({tuple(p) for p in all_px}) == len(all_px)
    assert all_px[:, 0].max() < 30 and all_px[:, 1].max() < 40


def test_carve_removes_low_energy_column():
    img = rng.integers(0, 256, size=(20, 15, 3), dtype=np.uint8)
    img[:, 7:10] = 50  # flat stripe -> zero-energy column in the middle (col 8)
    res = carve(img, 14, None)
    assert np.all(res.vertical_seams[0][:, 1] == 8)


def test_horizontal_equals_transposed_vertical():
    img = rng.integers(0, 256, size=(18, 22, 3), dtype=np.uint8)
    a = carve(img, None, 14).image
    b = carve(np.swapaxes(img, 0, 1), 14, None).image
    assert np.array_equal(a, np.swapaxes(b, 0, 1))


def test_invalid_target_raises():
    img = np.zeros((10, 10, 3), dtype=np.uint8)
    with pytest.raises(ValueError):
        carve(img, 11, 10)
    with pytest.raises(ValueError):
        carve(img, 10, 10, mode="bogus")
