"""Compare cutscene upscalers on the same frames, as the game would show them.

    python fli_compare.py                          ARBITER, CITYBURN, TEXT
    python fli_compare.py --only ARBITER --frames 10,30,50
    python fli_compare.py --sizes                  also re-encode whole movies

Every cell is requantized back to the frame's own 256-colour palette and then
resolved through it again, because that is what reaches the screen.  Comparing
the networks' float output would flatter all of them equally and answer the
wrong question: the palette is fixed by the format, and how gracefully an
upscaler survives being squeezed back into it is most of what matters here.

The `point` column is the honest baseline -- it is what the engine did before
any of this, a nearest-neighbour enlargement.  `lanczos4` is the other useful
control: an ordinary resampler costs nothing and is what the AI models have to
beat to justify themselves.

Two sheets per movie: the whole frame, and a zoomed crop, because the failure
modes show up at different scales.  Ringing and smearing are visible in the
full view; what happens to dithered gradients and to letterforms is only
visible in the crop.
"""

import argparse
import sys
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

import fli
import palette as pal
from make_fli import expand6

HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1] / "greed_cdrom" / "MOVIES"
WORK = HERE / "work"

DEFAULT_MOVIES = ("ARBITER", "CITYBURN", "TEXT")
GAP = 8
BG = 24
LABEL_H = 18


# ------------------------------------------------------------- treatments

def t_point(frames, w, h, scale):
    return [np.repeat(np.repeat(f, scale, 0), scale, 1) for f in frames]


def _nvencc_plain(frames, w, h, scale, algo):
    """An ordinary resampler, through the same y4m transport as the AI ones.

    Routing the control through the identical path means a difference in the
    sheet is the filter and not the colour handling.
    """
    import nvvsr
    tmp = Path(tempfile.mkdtemp(prefix="hdtex-cmp-"))
    try:
        src, dst = tmp / "in.y4m", tmp / "out.y4m"
        nvvsr.write_y4m(src, frames, w, h)
        import subprocess
        import fetch_nvencc
        p = subprocess.run(
            [str(fetch_nvencc.exe_path(fetch=False)), "--y4m", "-i", str(src)]
            + nvvsr.nvencc_io(w * scale, h * scale)
            + ["--vpp-resize", f"algo={algo}", "-o", str(dst)],
            capture_output=True, text=True)
        if p.returncode != 0:
            raise nvvsr.VsrError(nvvsr._tail((p.stderr or "") + (p.stdout or "")))
        return list(nvvsr.read_y4m(dst))
    finally:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)


def t_lanczos(frames, w, h, scale):
    return _nvencc_plain(frames, w, h, scale, "lanczos4")


def _vsr(algo, **kw):
    def go(frames, w, h, scale):
        import nvvsr
        up = nvvsr.NvUpscaler(algo, **kw)
        return list(up.run(iter(frames), w, h, scale))
    return go


def t_esrgan(frames, w, h, scale):
    from upscale import SCALE, Upscaler
    up = t_esrgan.up = getattr(t_esrgan, "up", None) or Upscaler("x4plus_anime")
    out = []
    for f in frames:
        big = up.pad(f)
        if scale != SCALE:
            big = np.asarray(Image.fromarray(big).resize(
                (w * scale, h * scale), Image.BOX))
        out.append(big)
    return out


TREATMENTS = [
    ("point",      t_point),
    ("lanczos4",   t_lanczos),
    ("esrgan",     t_esrgan),
    ("ngx-vsr q1", _vsr("ngx-vsr", quality=1)),
    ("ngx-vsr q4", _vsr("ngx-vsr", quality=4)),
    ("nvvfx m0",   _vsr("nvvfx-superres", mode=0)),
    ("nvvfx m1",   _vsr("nvvfx-superres", mode=1)),
]


# ------------------------------------------------------------------ sheets

def _label(img, text):
    out = Image.new("RGB", (img.width, img.height + LABEL_H), (BG, BG, BG))
    out.paste(img, (0, LABEL_H))
    ImageDraw.Draw(out).text((2, 3), text, fill=(230, 230, 230))
    return out


def _grid(cells, cols, names):
    """cells: list of rows, each a list of PIL images in `names` order."""
    cw = max(c.width for row in cells for c in row)
    ch = max(c.height for row in cells for c in row)
    out = Image.new("RGB", (cols * (cw + GAP) + GAP,
                            len(cells) * (ch + LABEL_H + GAP) + GAP), (BG, BG, BG))
    for r, row in enumerate(cells):
        for c, img in enumerate(row):
            out.paste(_label(img, names[c]),
                      (GAP + c * (cw + GAP), GAP + r * (ch + LABEL_H + GAP)))
    return out


def compare(name, indices, scale, crop, work):
    src = SRC / f"{name}.FLI"
    r = fli.Reader(src)
    want = set(indices)
    frames, pals = [], []
    for i, (frame, pal6) in enumerate(r.frames()):
        if i in want:
            frames.append(expand6(pal6)[frame])
            pals.append(pal6)
    if not frames:
        raise SystemExit(f"{name}: no frames matched {sorted(want)}")
    w, h = r.width, r.height
    print(f"\n  {name}.FLI  {w}x{h}  frames {sorted(want)}")

    quant = [pal.Quantizer(expand6(p)) for p in pals]

    done, names = [], []
    for label, fn in TREATMENTS:
        try:
            out = fn(list(frames), w, h, scale)
        except Exception as e:                                # noqa: BLE001
            print(f"    {label:11s} skipped: {str(e).splitlines()[0]}")
            continue
        # requantize and resolve back through the palette: what the game shows
        shown = [expand6(pals[i])[quant[i](big)] for i, big in enumerate(out)]
        done.append(shown)
        names.append(label)
        print(f"    {label:11s} ok")

    if not done:
        raise SystemExit("every treatment failed")

    full = _grid([[Image.fromarray(t[i]) for t in done] for i in range(len(frames))],
                 len(done), names)
    fp = Path(work) / f"_vsr_{name}_full.png"
    full.save(fp)
    print(f"    -> {fp}  ({full.width}x{full.height})")

    cw, chh = crop
    x0, y0 = (w * scale - cw) // 2, (h * scale - chh) // 2
    zoom = _grid(
        [[Image.fromarray(t[i][y0:y0 + chh, x0:x0 + cw]).resize(
            (cw * 2, chh * 2), Image.NEAREST) for t in done]
         for i in range(len(frames))], len(done), names)
    zp = Path(work) / f"_vsr_{name}_zoom.png"
    zoom.save(zp)
    print(f"    -> {zp}  ({zoom.width}x{zoom.height})")
    return fp, zp


def sizes(names, scale, work):
    """Re-encode whole movies per backend and report what each costs on disk."""
    import make_fli
    print("\n  encoded size, whole movies")
    rows = {}
    tmp = Path(tempfile.mkdtemp(prefix="hdtex-size-"))
    try:
        for backend, kw in (("esrgan", {}), ("ngx-vsr", dict(quality=4)),
                            ("nvvfx-superres", dict(mode=1))):
            for n in names:
                s = SRC / f"{n}.FLI"
                try:
                    up = (make_fli.make_upscaler(
                        argparse.Namespace(backend=backend, model="x4plus_anime",
                                           device="auto", quality=kw.get("quality", 4),
                                           mode=kw.get("mode", 1))))
                    sz, _ = make_fli.convert(s, tmp / f"{backend}_{n}.FLI", up,
                                             scale, backend, quiet=True)
                except Exception as e:                        # noqa: BLE001
                    print(f"    {backend:15s} {n:10s} skipped: "
                          f"{str(e).splitlines()[0]}")
                    continue
                rows.setdefault(n, {})[backend] = sz
    finally:
        import shutil
        shutil.rmtree(tmp, ignore_errors=True)

    backends = sorted({b for v in rows.values() for b in v})
    print(f"    {'movie':10s} {'original':>10s} "
          + " ".join(f"{b:>15s}" for b in backends))
    for n in names:
        orig = (SRC / f"{n}.FLI").stat().st_size
        line = f"    {n:10s} {orig / 1e6:9.2f}M "
        for b in backends:
            v = rows.get(n, {}).get(b)
            line += f"{v / 1e6:14.2f}M " if v else f"{'-':>15s} "
        print(line)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--only", help="comma-separated basenames, without .FLI")
    ap.add_argument("--frames", help="comma-separated frame indices")
    ap.add_argument("--n", type=int, default=3, help="frames per movie if --frames absent")
    ap.add_argument("--scale", type=int, default=2)
    ap.add_argument("--crop", default="260x170", help="zoom crop, in output pixels")
    ap.add_argument("--sizes", action="store_true", help="also re-encode whole movies")
    ap.add_argument("--work", default=WORK, type=Path)
    a = ap.parse_args()

    a.work.mkdir(parents=True, exist_ok=True)
    names = [s.strip().upper() for s in a.only.split(",")] if a.only \
        else list(DEFAULT_MOVIES)
    cw, ch = (int(v) for v in a.crop.lower().split("x"))

    for n in names:
        src = SRC / f"{n}.FLI"
        if not src.exists():
            print(f"  {n}: no such movie under {SRC}", file=sys.stderr)
            continue
        if a.frames:
            idx = [int(v) for v in a.frames.split(",")]
        else:
            total = fli.Reader(src).nframes
            # spread the samples over the movie, skipping the fades at either end
            idx = [int(total * (k + 1) / (a.n + 1)) for k in range(a.n)]
        compare(n, idx, a.scale, (cw, ch), a.work)

    if a.sizes:
        sizes(names, a.scale, a.work)


if __name__ == "__main__":
    main()
