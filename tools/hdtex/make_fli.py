"""Upscale the FLI cutscenes 2x.

    python make_fli.py                       all 39 movies -> greed_final/MOVIES
    python make_fli.py --only ARBITER,TEXT
    python make_fli.py --backend esrgan --resume

Each frame is decoded to palette indices, resolved through the movie's own
palette, upscaled, and requantized back to that same palette -- FLI carries its
palette inline and the engine sets it per chunk, so there is no freedom to pick
a better one and no need to.

Three backends, selected with --backend:

  ngx-vsr          NGX DLVSR, the RTX Video Super Resolution model.  The
                   default.  Ships inside the NVEncC archive, needs no install.
  nvvfx-superres   Maxine Video Effects SuperRes.  Needs the Video Effects
                   runtime; see README.
  esrgan           Real-ESRGAN, which is what the texture pack still uses.
                   Kept so fli_compare.py has something to compare against.

Frames are upscaled independently by all three.  None of these models is
temporal, so a texture the eye tracks across a pan can shimmer slightly frame
to frame; fixing that properly (optical flow, or keyframes plus warping) is a
much larger job than a five-second cutscene justifies.

Output goes beside the originals under a MOVIES directory the game searches, so
the originals are never touched.
"""

import argparse
import time
from pathlib import Path

import numpy as np
from PIL import Image

import fli
import palette as pal

HERE = Path(__file__).resolve().parent
SRC = HERE.parents[1] / "greed_cdrom" / "MOVIES"
DST = HERE.parents[1] / "greed_final" / "MOVIES"

BACKENDS = ("ngx-vsr", "nvvfx-superres", "esrgan")


def expand6(pal6):
    """(256,3) 6-bit DAC levels -> 8-bit RGB, as sys_video.c does."""
    v = np.asarray(pal6, np.uint16) & 0x3F
    return (((v << 2) | (v >> 4)) & 0xFF).astype(np.uint8)


class _Quant:
    """Quantizers for the palettes a movie uses, built on demand.

    Cached because the palette usually holds for a whole movie, and cleared
    rather than grown because when it does change it rarely changes back.  No
    reserved indices: an FLI frame is opaque video, index 0 is just black.
    """

    def __init__(self):
        self.cache = {}

    def __call__(self, pal6):
        rgb8 = expand6(pal6)
        key = rgb8.tobytes()
        if key not in self.cache:
            self.cache.clear()
            self.cache[key] = pal.Quantizer(rgb8)
        return self.cache[key]


def convert_esrgan(r, w, up, scale):
    """Per-frame path: the network is a fixed x4 and takes one image at a time."""
    from upscale import SCALE

    quant = _Quant()
    n = 0
    for frame, pal6 in r.frames():
        big = up.pad(expand6(pal6)[frame])
        if scale != SCALE:
            # Real-ESRGAN only comes in x4, so a smaller output is a box-filtered
            # reduction of its x4 result rather than a separate, cheaper upscale.
            # Doing it after the network keeps the detail it recovered; writing
            # x4 frames into a header claiming a smaller size decodes as garbage.
            # The VSR backends scale natively and need none of this.
            big = np.asarray(Image.fromarray(big).resize(
                (r.width * scale, r.height * scale), Image.BOX))
        w.add(quant(pal6)(big), pal6)
        n += 1
    return n


def convert_vsr(r, w, up, scale):
    """Streamed path: one NVEncC invocation for the whole movie.

    The palettes are collected as a side effect of feeding the frames in.  That
    is safe only because NvUpscaler.run consumes its input completely -- writing
    the intermediate y4m -- before it yields the first output frame, so `pals`
    is fully populated by the time it is indexed.
    """
    quant = _Quant()
    pals = []

    def rgb_frames():
        for frame, pal6 in r.frames():
            pals.append(pal6)
            yield expand6(pal6)[frame]

    n = 0
    for big in up.run(rgb_frames(), r.width, r.height, scale):
        pal6 = pals[n]
        w.add(quant(pal6)(big), pal6)
        n += 1
    return n


def convert(src, dst, up, scale, backend, quiet=False):
    r = fli.Reader(src)
    w = fli.Writer(dst, r.width * scale, r.height * scale, r.speed)
    run = convert_esrgan if backend == "esrgan" else convert_vsr
    n = run(r, w, up, scale)
    size = w.close()
    if not quiet:
        print(f"  {Path(src).name:14s} {r.width}x{r.height} x{scale} "
              f"{n:4d} frames -> {size / 1e6:6.1f} MB")
    return size, n


def _complete(dst, src, scale):
    """Is `dst` a finished conversion of `src` at this scale?

    --resume has to check this rather than mere existence, for two reasons: the
    header is only written at close(), so a run interrupted part way through
    leaves a file with a zeroed signature; and re-running at a different --scale
    must redo the output rather than skip a file that is the wrong size.

    It cannot tell one backend's output from another's -- they are the same
    dimensions -- so --resume is for finishing an interrupted run, not for
    switching backend.  Point --out somewhere else for that.
    """
    if not dst.exists():
        return False
    try:
        r, s = fli.Reader(dst), fli.Reader(src)
        return (r.nframes == s.nframes and r.nframes > 0
                and (r.width, r.height) == (s.width * scale, s.height * scale))
    except Exception:                                     # noqa: BLE001
        return False


def make_upscaler(a):
    if a.backend == "esrgan":
        from upscale import Upscaler
        return Upscaler(a.model, device=a.device)
    import nvvsr
    return nvvsr.NvUpscaler(a.backend, quality=a.quality, mode=a.mode)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--src", default=SRC, type=Path)
    ap.add_argument("--out", default=DST, type=Path)
    ap.add_argument("--only", help="comma-separated basenames, without .FLI")
    ap.add_argument("--backend", default="ngx-vsr", choices=BACKENDS)
    ap.add_argument("--scale", type=int, default=2,
                    help="output multiple (default 2, i.e. 640x400)")
    ap.add_argument("--quality", type=int, default=4,
                    help="ngx-vsr quality, 1-4")
    ap.add_argument("--mode", type=int, default=1,
                    help="nvvfx-superres mode: 0 conservative, 1 aggressive")
    ap.add_argument("--model", default="x4plus_anime", help="esrgan weights")
    ap.add_argument("--device", default="auto", help="esrgan device")
    ap.add_argument("--resume", action="store_true")
    a = ap.parse_args()

    a.out.mkdir(parents=True, exist_ok=True)
    names = set(a.only.upper().split(",")) if a.only else None
    movies = sorted(p for p in a.src.glob("*.FLI")
                    if not names or p.stem.upper() in names)
    if not movies:
        raise SystemExit(f"no movies matched under {a.src}")

    up = make_upscaler(a)
    print(f"  {up}, {len(movies)} movies, x{a.scale} -> {a.out}")

    total = frames = 0
    t0 = time.time()
    for m in movies:
        dst = a.out / m.name
        if a.resume and _complete(dst, m, a.scale):
            total += dst.stat().st_size
            continue
        size, n = convert(m, dst, up, a.scale, a.backend)
        total += size
        frames += n

    print(f"\n  {frames} frames, {total / 1e6:.0f} MB total, "
          f"{time.time() - t0:.0f}s")


if __name__ == "__main__":
    main()
