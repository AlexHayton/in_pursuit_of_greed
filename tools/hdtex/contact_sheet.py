"""Side-by-side before/after sheets, so the upscale can actually be judged.

    python contact_sheet.py wall            all walls, original next to 4x
    python contact_sheet.py sprite --n 24   first 24 only
    python contact_sheet.py flat --zoom 2

The original is nearest-neighbour enlarged to the same size as the 4x version,
which is the honest comparison: it is what the engine shows today when a 64px
texture is stretched across a few hundred screen pixels.  Anything smoother in
the right-hand tile is a real gain; anything blurrier is a real loss.
"""

import argparse
from pathlib import Path

import numpy as np
from PIL import Image

HERE = Path(__file__).resolve().parent
GAP = 6
BG = 32


def sheet(work, cls, n=None, cols=6, zoom=1):
    src = Path(work) / "png" / cls
    hd = Path(work) / "preview" / cls
    if not hd.exists():
        hd = Path(work) / "hd" / cls
    files = sorted((p for p in src.glob("*.png") if not p.name.endswith(".mask.png")),
                   key=lambda p: int(p.stem))
    if n:
        files = files[:n]

    pairs = []
    for p in files:
        after_path = hd / p.name
        if not after_path.exists():
            continue
        before = Image.open(p).convert("RGB")
        after = Image.open(after_path).convert("RGB")
        before = before.resize(after.size, Image.NEAREST)
        if zoom != 1:
            size = (after.width * zoom, after.height * zoom)
            before = before.resize(size, Image.NEAREST)
            after = after.resize(size, Image.NEAREST)
        pairs.append((p.stem, before, after))

    if not pairs:
        raise SystemExit(f"nothing to compare in {src} / {hd}")

    cell_w = max(b.width + a.width + GAP for _, b, a in pairs)
    cell_h = max(b.height for _, b, _ in pairs)
    rows = (len(pairs) + cols - 1) // cols
    out = Image.new("RGB", (cols * (cell_w + GAP) + GAP,
                            rows * (cell_h + GAP) + GAP), (BG, BG, BG))
    for k, (_, before, after) in enumerate(pairs):
        r, c = divmod(k, cols)
        x = GAP + c * (cell_w + GAP)
        y = GAP + r * (cell_h + GAP)
        out.paste(before, (x, y))
        out.paste(after, (x + before.width + GAP, y))

    dest = Path(work) / f"contact_{cls}.png"
    out.save(dest)
    print(f"  {len(pairs)} pairs -> {dest}  ({out.width}x{out.height})")
    return dest


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cls", choices=("wall", "flat", "sprite", "pic", "font"))
    ap.add_argument("--work", default=HERE / "work", type=Path)
    ap.add_argument("--n", type=int, help="limit the number of tiles")
    ap.add_argument("--cols", type=int, default=6)
    ap.add_argument("--zoom", type=int, default=1)
    a = ap.parse_args()
    sheet(a.work, a.cls, a.n, a.cols, a.zoom)


if __name__ == "__main__":
    main()
