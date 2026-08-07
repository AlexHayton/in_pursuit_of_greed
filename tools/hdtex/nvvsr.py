"""NVIDIA super-resolution for the cutscenes, driven through NVEncC.

Two networks, both reached as resize filters:

  ngx-vsr          NGX DLVSR -- the model behind RTX Video Super Resolution.
                   `nvngx_vsr.dll` ships inside the NVEncC archive, so this
                   needs nothing installed.  quality 1..4.
  nvvfx-superres   Maxine Video Effects SuperRes.  Needs the Video Effects
                   runtime installed separately (see README); NVEncC looks for
                   NVVideoEffects.dll under Program Files and says so if it is
                   missing.  mode 0 conservative / 1 aggressive.

Unlike `upscale.Upscaler` this is deliberately a *stream* API rather than a
per-image one.  NVEncC is a process, and the NGX model spends most of a short
job warming up -- 13s for the first eight frames against 3.4s for the next
eight -- so a movie is one invocation, not one per frame.

The transport is y4m in 4:4:4, 10-bit, *limited* range, smpte170m.  Each of
those is forced by something:

  4:4:4       the raw reader has no RGB input colourspace, and 4:2:0 would
              throw away half the chroma of art that is mostly saturated flat
              fills.
  limited     ngx-vsr emits limited-range YUV whatever `--colorrange` says --
              the flag only tags metadata.  Fed full-range data it clamps the
              ends, mapping 0 -> 16 and 255 -> 235 while leaving mid-tones
              alone, which crushes every pure black and pure white in a
              cutscene.  Fed limited-range data the transfer is exact.  This
              was measured with a grey ramp, not assumed; see --selftest.
  10-bit      limited range at 8 bits has 219 levels for 256 values, which
              costs about one level of error and a small negative bias.  At 10
              bits there are 876, so the squeeze is free.
  smpte170m   BT.601 coefficients, which is what 1995 VGA art was authored
              against.  `bt601` is not a token NVEncC accepts.

The filter converts to RGB internally using whatever matrix we declare -- it
logs `matrix:smpte170m->GBR` -- so declaring it consistently is what keeps the
round trip honest.

Frames are upscaled independently.  NGX VSR is a per-frame model despite the
name -- it is not temporal, so it does not fix the pan shimmer that the ESRGAN
backend has, and it is not expected to.
"""

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

import fetch_nvencc

HERE = Path(__file__).resolve().parent

# The intermediate frame rate is meaningless -- both networks are spatial, and
# the output FLI keeps the source's own `speed` field.  It is here only because
# y4m requires the field.
FPS = "10:1"

ALGOS = ("ngx-vsr", "nvvfx-superres")


class VsrError(RuntimeError):
    pass


# ------------------------------------------------------------------- colour

DEPTH = 10                      # bits per sample on the wire; see module docstring


def _levels(depth):
    """(black, white_span, chroma_centre, chroma_span, max) for limited range."""
    s = 1 << (depth - 8)
    return 16 * s, 219 * s, 128 * s, 224 * s, (1 << depth) - 1


def rgb_to_yuv444(rgb, depth=DEPTH):
    """(H,W,3) uint8 -> three (H,W) planes of limited-range BT.601 samples."""
    r = rgb[..., 0].astype(np.float32)
    g = rgb[..., 1].astype(np.float32)
    b = rgb[..., 2].astype(np.float32)
    y = 0.299 * r + 0.587 * g + 0.114 * b
    cb = (b - y) / 1.772
    cr = (r - y) / 1.402

    blk, span, cc, cspan, mx = _levels(depth)
    dt = np.uint16 if depth > 8 else np.uint8
    q = lambda a: np.clip(np.rint(a), 0, mx).astype(dt)          # noqa: E731
    return (q(blk + y * span / 255.0),
            q(cc + cb * cspan / 255.0),
            q(cc + cr * cspan / 255.0))


def yuv444_to_rgb(y, cb, cr, depth=DEPTH):
    """The exact inverse of rgb_to_yuv444, to within rounding."""
    blk, span, cc, cspan, _ = _levels(depth)
    y = (y.astype(np.float32) - blk) * 255.0 / span
    cb = (cb.astype(np.float32) - cc) * 255.0 / cspan
    cr = (cr.astype(np.float32) - cc) * 255.0 / cspan
    rgb = np.stack([y + 1.402 * cr,
                    y - 0.344136 * cb - 0.714136 * cr,
                    y + 1.772 * cb], axis=-1)
    return np.clip(np.rint(rgb), 0, 255).astype(np.uint8)


# ---------------------------------------------------------------------- y4m

def write_y4m(path, frames, w, h, depth=DEPTH):
    """Write RGB frames as 4:4:4 y4m.  Returns the number written."""
    tag = "C444" if depth == 8 else f"C444p{depth}"
    n = 0
    with open(path, "wb") as f:
        f.write(f"YUV4MPEG2 W{w} H{h} F{FPS} Ip A1:1 {tag}\n".encode())
        for rgb in frames:
            f.write(b"FRAME\n")
            for plane in rgb_to_yuv444(rgb, depth):
                # y4m carries >8-bit samples little-endian, which is also numpy's
                # native order here; be explicit so a big-endian host would still
                # write a readable file.
                f.write(plane.astype("<u2" if depth > 8 else "u1").tobytes())
            n += 1
    return n


def read_y4m(path):
    """Stream a 4:4:4 y4m back as RGB frames, at whatever depth it declares.

    Read incrementally rather than slurping: a 640x400 4:4:4 10-bit frame is
    1.5 MB and the longest cutscene runs to hundreds of them.
    """
    with open(path, "rb") as f:
        hdr = b""
        while not hdr.endswith(b"\n"):
            c = f.read(1)
            if not c:
                raise VsrError(f"{path}: truncated y4m header")
            hdr += c
        tags = {t[0:1]: t[1:] for t in hdr.split()[1:]}
        w, h = int(tags[b"W"]), int(tags[b"H"])
        csp = tags.get(b"C", b"420").decode()
        if not csp.startswith("444"):
            raise VsrError(f"{path}: colourspace {csp!r}, expected 444")
        depth = int(csp[4:]) if csp.startswith("444p") else 8

        dt = np.dtype("<u2" if depth > 8 else "u1")
        n = w * h * dt.itemsize
        while True:
            line = f.readline()
            if not line:
                return
            if not line.startswith(b"FRAME"):
                raise VsrError(f"{path}: expected FRAME, got {line[:16]!r}")
            planes = []
            for _ in range(3):
                buf = f.read(n)
                if len(buf) != n:
                    raise VsrError(f"{path}: truncated frame")
                planes.append(np.frombuffer(buf, dt).reshape(h, w))
            yield yuv444_to_rgb(*planes, depth=depth)


def nvencc_io(ow=None, oh=None, depth=DEPTH):
    """The output-side flags every invocation here shares.

    Kept in one place so the controls in fli_compare.py go through byte for byte
    the same colour handling as the AI filters -- otherwise a difference in the
    sheet could be the transport rather than the filter.
    """
    a = ["-c", "raw", "--output-format", "y4m",
         "--output-csp", "yuv444", "--output-depth", str(depth),
         "--colormatrix", "smpte170m", "--colorrange", "limited"]
    if ow and oh:
        a += ["--output-res", f"{ow}x{oh}"]
    return a


# ------------------------------------------------------------------ upscaler

class NvUpscaler:
    """One NVEncC invocation per movie."""

    def __init__(self, algo="ngx-vsr", quality=4, mode=1, strength=None,
                 exe=None, keep_temp=False):
        if algo not in ALGOS:
            raise KeyError(f"unknown algo {algo!r}; have {', '.join(ALGOS)}")
        self.algo = algo
        self.quality = quality
        self.mode = mode
        self.strength = strength
        self.exe = Path(exe) if exe else fetch_nvencc.exe_path(fetch=False)
        self.keep_temp = keep_temp

    def __repr__(self):
        detail = (f"q{self.quality}" if self.algo == "ngx-vsr"
                  else f"mode{self.mode}")
        return f"<NvUpscaler {self.algo} {detail}>"

    def _resize_arg(self):
        p = [f"algo={self.algo}"]
        if self.algo == "ngx-vsr":
            p.append(f"vsr-quality={self.quality}")
        else:
            p.append(f"superres-mode={self.mode}")
            if self.strength is not None:
                p.append(f"superres-strength={self.strength}")
        return ",".join(p)

    def run(self, frames, w, h, scale, workdir=None):
        """Upscale `frames` (an iterable of (h,w,3) uint8) by `scale`.

        Yields (h*scale, w*scale, 3) uint8 in the same order.
        """
        tmp = Path(workdir) if workdir else Path(tempfile.mkdtemp(prefix="hdtex-vsr-"))
        tmp.mkdir(parents=True, exist_ok=True)
        src, dst = tmp / "in.y4m", tmp / "out.y4m"
        try:
            n = write_y4m(src, frames, w, h)
            if n == 0:
                return
            self._invoke(src, dst, w * scale, h * scale)
            got = 0
            for rgb in read_y4m(dst):
                got += 1
                yield rgb
            if got != n:
                raise VsrError(f"sent {n} frames, got {got} back")
        finally:
            if not self.keep_temp:
                shutil.rmtree(tmp, ignore_errors=True)

    def _invoke(self, src, dst, ow, oh):
        cmd = [str(self.exe), "--y4m", "-i", str(src)] + nvencc_io(ow, oh) + [
               "--vpp-resize", self._resize_arg(), "-o", str(dst)]
        p = subprocess.run(cmd, capture_output=True, text=True)
        log = (p.stderr or "") + (p.stdout or "")

        if p.returncode != 0:
            if "Failed load library for nvvfx" in log:
                raise VsrError(
                    "nvvfx-superres needs the NVIDIA Video Effects runtime, which "
                    "is not installed.\nIt comes from NGC and needs an account -- "
                    "see 'Maxine Video Effects' in tools/hdtex/README.md.\n"
                    "ngx-vsr needs no such install and is the default.\n\n"
                    + _tail(log))
            raise VsrError(f"NVEncC failed (rc={p.returncode}):\n{_tail(log)}")

        self._assert_ran(log)

    def _assert_ran(self, log):
        """Fail if NVEncC quietly substituted an ordinary resampler.

        It reports the chain it built, and an AI filter that could not
        initialise leaves a differently-shaped line -- `resize(lanczos4):` for a
        plain resampler against `resize: ngx-vsr` for this one.  Worth checking
        rather than assuming, because NVEncC accepts scale factors the networks
        do not support and works around them silently.
        """
        chain = [L for L in log.splitlines() if "resize" in L.lower()]
        if not any(self.algo in L for L in chain):
            raise VsrError(
                f"NVEncC did not run {self.algo}; its filter chain was:\n  "
                + ("\n  ".join(x.strip() for x in chain) or "(no resize filter)"))


def _tail(log, n=25):
    lines = [L for L in log.splitlines() if L.strip()]
    return "\n".join(lines[-n:])


# ----------------------------------------------------------------- selftest

def _selftest(args):
    """Prove the colour path, then prove the network is actually running.

    Both matter and neither is visible in the output on its own: a limited-range
    squeeze or the wrong matrix would tint every cutscene in a way that looks
    like the upscaler's doing, and a silent fallback to lanczos would look like
    a disappointing model.
    """
    import fli

    src = Path(args.src)
    r = fli.Reader(src)
    frames = []
    for frame, pal6 in r.frames():
        p = np.asarray(pal6, np.uint16) & 0x3F
        rgb8 = (((p << 2) | (p >> 4)) & 0xFF).astype(np.uint8)
        frames.append(rgb8[frame])
        if len(frames) >= args.frames:
            break
    w, h = r.width, r.height
    print(f"  {src.name}  {w}x{h}  {len(frames)} frames")

    exe = fetch_nvencc.exe_path(fetch=False)
    tmp = Path(tempfile.mkdtemp(prefix="hdtex-selftest-"))
    ok = True
    try:
        # 1. identity: same resolution, no scaling, nothing should change
        a, b = tmp / "id_in.y4m", tmp / "id_out.y4m"
        write_y4m(a, frames, w, h)
        p = subprocess.run(
            [str(exe), "--y4m", "-i", str(a)] + nvencc_io() + ["-o", str(b)],
            capture_output=True, text=True)
        if p.returncode != 0:
            raise VsrError(_tail((p.stderr or "") + (p.stdout or "")))
        back = list(read_y4m(b))
        err = np.concatenate([np.abs(x.astype(np.int16) - y.astype(np.int16)).ravel()
                              for x, y in zip(back, frames)])
        bias = float(np.mean([(x.astype(np.int16) - y.astype(np.int16)).mean()
                              for x, y in zip(back, frames)]))
        good = err.max() <= 2 and abs(bias) <= 0.25
        ok &= good
        print(f"  colour round trip   max={err.max()} mean={err.mean():.4f} "
              f"bias={bias:+.4f}   {'ok' if good else 'FAIL'}")

        # 2. the network is not silently an ordinary resampler.  lanczos4 is the
        # yardstick: whatever the model does, it must not land on top of it.
        c = tmp / "ref.y4m"
        p = subprocess.run(
            [str(exe), "--y4m", "-i", str(a)]
            + nvencc_io(w * args.scale, h * args.scale)
            + ["--vpp-resize", "algo=lanczos4", "-o", str(c)],
            capture_output=True, text=True)
        if p.returncode != 0:
            raise VsrError(_tail((p.stderr or "") + (p.stdout or "")))
        ref = list(read_y4m(c))

        # 3. levels survive the filter.  ngx-vsr emits limited-range YUV whatever
        # --colorrange says; fed full-range data it clamped 0 to 16 and 255 to
        # 235, silently crushing every pure black and white in a cutscene.  A
        # grey ramp catches that, and nothing else here does.
        ramp = np.repeat(np.rint(np.linspace(0, 255, w)).astype(np.uint8)[None, :],
                         h, axis=0)
        ramp = np.repeat(ramp[:, :, None], 3, axis=2)

        for algo, kw in (("ngx-vsr", dict(quality=1)),
                         ("ngx-vsr", dict(quality=4)),
                         ("nvvfx-superres", dict(mode=0)),
                         ("nvvfx-superres", dict(mode=1))):
            up = NvUpscaler(algo, exe=exe, **kw)
            try:
                out = list(up.run(iter(frames), w, h, args.scale))
                lv = list(up.run(iter([ramp]), w, h, args.scale))[0]
            except VsrError as e:
                first = str(e).splitlines()[0]
                print(f"  {up!r:34s} skipped: {first}")
                continue
            mad = float(np.mean([np.abs(x.astype(np.int16) - y.astype(np.int16)).mean()
                                 for x, y in zip(out, ref)]))
            row = lv[lv.shape[0] // 2].mean(axis=1)
            black, white = float(row[:4].min()), float(row[-4:].max())
            sized = out[0].shape[:2] == (h * args.scale, w * args.scale)
            levels = black <= 2.0 and white >= 253.0
            # Only distinguishing "ran" from "did not run".  The margin over a
            # good resampler is genuinely small -- about 1.4 here -- so this
            # cannot double as a quality threshold.  _assert_ran, reading
            # NVEncC's own filter chain, is the real check; this is a second
            # opinion that does not depend on the log wording.
            ran = mad > 0.25
            good = sized and ran and levels
            ok &= good
            why = "" if good else (" FAIL: fallback?" if not ran else
                                   f" FAIL: levels {black:.0f}..{white:.0f}")
            print(f"  {up!r:34s} {out[0].shape[1]}x{out[0].shape[0]}  "
                  f"mad vs lanczos4={mad:6.3f}  ramp {black:5.1f}..{white:5.1f}"
                  f"   {'ok' if good else why}")
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    print("\n  " + ("all checks passed" if ok else "SOMETHING FAILED"))
    return 0 if ok else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--src", default=str(HERE.parents[1] / "greed_cdrom" /
                                         "MOVIES" / "ARBITER.FLI"))
    ap.add_argument("--frames", type=int, default=8)
    ap.add_argument("--scale", type=int, default=2)
    a = ap.parse_args()
    if not a.selftest:
        ap.error("nothing to do; this is a library.  Try --selftest.")
    return _selftest(a)


if __name__ == "__main__":
    sys.exit(main())
