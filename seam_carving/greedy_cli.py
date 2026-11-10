"""Command-line entry point for greedy seam carving.

Example:
    python -m seam_carving.greedy_cli images/castle.jpg --width 400 --height 300
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

from .greedy import MODES, carve, draw_seams


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description="Shrink an image with greedy seam carving.")
    ap.add_argument("input", type=Path, help="input image (jpg/png/...)")
    ap.add_argument("--width", type=int, help="target width in pixels")
    ap.add_argument("--height", type=int, help="target height in pixels")
    ap.add_argument("--mode", choices=MODES, default="min",
                    help="min = pure greedy from lowest top pixel (default); "
                         "multistart = greedy from every top pixel, keep cheapest")
    ap.add_argument("--out-dir", type=Path, default=Path("output/greedy"))
    args = ap.parse_args(argv)

    if args.width is None and args.height is None:
        ap.error("give at least one of --width / --height")

    img = np.asarray(Image.open(args.input).convert("RGB"))
    h, w = img.shape[:2]
    print(f"Input  : {args.input} ({w}x{h})")

    res = carve(img, args.width, args.height, mode=args.mode)
    oh, ow = res.image.shape[:2]

    args.out_dir.mkdir(parents=True, exist_ok=True)
    stem = f"{args.input.stem}_greedy_{args.mode}"
    out_img = args.out_dir / f"{stem}.png"
    out_seams = args.out_dir / f"{stem}_seams.png"
    out_stats = args.out_dir / f"{stem}_stats.json"

    Image.fromarray(res.image).save(out_img)
    Image.fromarray(draw_seams(img, res.vertical_seams, res.horizontal_seams)).save(out_seams)

    n_seams = len(res.seam_costs)
    stats = {
        "algorithm": "greedy",
        "mode": args.mode,
        "input": str(args.input),
        "original_size": [w, h],
        "output_size": [ow, oh],
        "vertical_seams_removed": len(res.vertical_seams),
        "horizontal_seams_removed": len(res.horizontal_seams),
        "total_time_s": res.total_time,
        "energy_time_s": res.energy_time,
        "seam_find_time_s": res.find_time,
        "seam_remove_time_s": res.remove_time,
        "avg_seam_find_time_ms": 1000 * res.find_time / n_seams if n_seams else 0.0,
        "total_seam_energy": float(sum(res.seam_costs)),
        "mean_seam_energy": float(np.mean(res.seam_costs)) if n_seams else 0.0,
        "seam_costs": res.seam_costs,
    }
    out_stats.write_text(json.dumps(stats, indent=2))

    print(f"Output : {out_img} ({ow}x{oh})")
    print(f"Seams  : {out_seams}  (red = vertical, blue = horizontal)")
    print(f"Stats  : {out_stats}")
    print(f"Time   : total {res.total_time:.3f}s | energy {res.energy_time:.3f}s | "
          f"find {res.find_time:.3f}s | remove {res.remove_time:.3f}s")
    print(f"Energy : total removed {stats['total_seam_energy']:.1f}, "
          f"mean per seam {stats['mean_seam_energy']:.1f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
