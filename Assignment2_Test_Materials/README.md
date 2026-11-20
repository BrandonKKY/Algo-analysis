# Assignment 2 test inputs and outputs (Team T20)

## inputs/

- `images.jpg`: strawberry photo, 642 x 350.
- `sample.png`: small gradient test image, 64 x 40.

## outputs/

Each folder is one run: the input scaled to the width shown, then vertical seams
removed with each method.

| Folder | Input | Size carved | Seams removed | Methods |
| --- | --- | --- | --- | --- |
| `strawberry_100px_30_seams` | `images.jpg` | 100 x 55 | 30 | dynamic programming, greedy, graph cut |
| `strawberry_400px_120_seams` | `images.jpg` | 400 x 218 | 120 | dynamic programming, greedy |
| `sample_64px_20_seams` | `sample.png` | 64 x 40 | 20 | dynamic programming, greedy, graph cut |

In each folder:

- `input_scaled.png`: the input at the size it was carved.
- `<method>_seams.png`: the input with every removed seam drawn in pink.
- `<method>_result.png`: the resized output image.

Also in `outputs/`:

- `results.csv`: per run and method, the output size, total energy removed and time per seam (ms).
- `test_log.txt`: output of the three test programs (`make test`).

Graph cut was not run at 400 px wide: it takes about a minute per seam at that size.
