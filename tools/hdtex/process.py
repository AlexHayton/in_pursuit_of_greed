"""work/png -> work/hd: upscale x4, then requantize to 8-bit indices.

Output is one mode-L PNG per lump holding *palette indices*, which is what
pack.py consumes, plus a mask PNG for sprites and an optional RGB preview for
looking at.

Per-class handling, and why each is different:

  wall, flat   tile on both axes, so they go through Upscaler.wrap (3x3, crop
               the centre) or the seams show up as a line at every tile edge.
  sprite       has a transparent region.  Transparent pixels are filled with
               nearby opaque colour *before* upscaling so the network never
               sees an artificial black edge and inpaints a dark halo; the
               opacity mask is upscaled separately and re-applied after.
  pic          does not tile; replicate-padded.  Full-screen art is quantized
               against its own palette lump, not the game palette.  Hard edges
               -- text baked into the art, object silhouettes -- come from EPX
               instead of the model, which cannot render a 6-pixel letterform;
               see pixelart.py.
  font         does not go through the model at all.  Glyph bytes are indices or
               ramp offsets depending on the lump, and neither survives being
               interpolated, so fonts are EPX end to end.
"""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

import palette as pal
import pixelart
from upscale import SCALE, Upscaler

HERE = Path(__file__).resolve().parent
DEFAULT_WORK = HERE / "work"

# Oklab distance at which a source edge is taken from EPX rather than the model.
# The distribution is strongly bimodal -- on info1, coverage is 26.8% at 0.25 and
# 26.6% at 0.35 -- so anything on that plateau gives the same mask.  0 disables
# the blend and restores the model-only behaviour, for comparison.
SHARP_THRESHOLD = 0.30

# Which model suits which class.  Overridable with --model.  Fonts are absent:
# they are EPX only, and asking for a model for them would just load weights
# nothing calls.
DEFAULT_MODELS = {
    "wall": "x4plus",
    "flat": "x4plus",
    "pic": "x4plus",
    "sprite": "x4plus_anime",
}


def fill_transparent(rgb, mask, passes=8):
    """Bleed opaque colour outward into the transparent region.

    Feeding index-0 black into the network makes it inpaint a dark fringe all
    the way around every sprite -- the single most visible failure mode of naive
    sprite upscaling.  Filling first means the edge the network sees is a
    continuation of the artwork; the real edge is restored afterwards from the
    separately-upscaled mask.

    A few passes of "average of opaque 4-neighbours" is plenty: the art has no
    large transparent interiors, and anything still unfilled is far enough from
    the silhouette not to influence it.
    """
    out = rgb.astype(np.float32).copy()
    known = mask.copy()
    out[~known] = 0
    for _ in range(passes):
        if known.all():
            break
        acc = np.zeros_like(out)
        cnt = np.zeros(known.shape, np.float32)
        for axis, shift in ((0, 1), (0, -1), (1, 1), (1, -1)):
            acc += np.roll(np.where(known[..., None], out, 0), shift, axis)
            cnt += np.roll(known.astype(np.float32), shift, axis)
        fillable = (~known) & (cnt > 0)
        if not fillable.any():
            break
        out[fillable] = acc[fillable] / cnt[fillable, None]
        known |= fillable
    return np.clip(out, 0, 255).astype(np.uint8)


class Processor:
    def __init__(self, work=DEFAULT_WORK, models=None, device=None, preview=True,
                 sharp_threshold=SHARP_THRESHOLD):
        self.work = Path(work)
        self.meta = json.loads((self.work / "meta.json").read_text("utf-8"))
        self.game_pal = np.array(self.meta["game_palette"], np.uint8)
        self.models = dict(DEFAULT_MODELS, **(models or {}))
        self.device = device
        self.preview = preview
        self.sharp_threshold = sharp_threshold
        self._cache = {}
        self._quant = {}
        self._lab_cache = {}

    def upscaler(self, name):
        if name not in self._cache:
            self._cache[name] = Upscaler(name, device=self.device)
            print(f"  loaded {self._cache[name]}")
        return self._cache[name]

    def quantizer(self, cls, palette):
        key = (cls, palette.tobytes())
        if key not in self._quant:
            self._quant[key] = pal.Quantizer(palette, pal.RESERVED.get(cls, ()))
        return self._quant[key]

    def lab(self, palette):
        """256x3 Oklab table for a palette, cached -- pics reuse a handful."""
        key = palette.tobytes()
        if key not in self._lab_cache:
            self._lab_cache[key] = pal.to_oklab(
                palette.reshape(1, -1, 3)).reshape(-1, 3)
        return self._lab_cache[key]

    # ---------------------------------------------------------------- per class

    def run(self, classes=None, resume=False, limit=None, report=False):
        out_root = self.work / "hd"
        out_root.mkdir(exist_ok=True)
        by_class = {}
        for key, entry in self.meta["lumps"].items():
            by_class.setdefault(entry["class"], []).append((int(key), entry))

        errors = []
        for cls, items in sorted(by_class.items()):
            if classes and cls not in classes:
                continue
            items.sort()
            if limit:
                items = items[:limit]
            d = out_root / cls
            d.mkdir(exist_ok=True)
            if self.preview:
                (self.work / "preview" / cls).mkdir(parents=True, exist_ok=True)
            # Fonts have no model; don't load weights to pass an unused argument.
            up = self.upscaler(self.models[cls]) if cls in self.models else None

            done = 0
            for i, entry in _progress(items, cls):
                dest = d / f"{i}.png"
                if resume and dest.exists():
                    continue
                err = getattr(self, f"_do_{cls}")(i, entry, up, d)
                if err is not None and report:
                    errors.append((cls, i, err))
                done += 1
            print(f"  {cls:8s} {done} lumps -> {d}")

        if report and errors:
            errors.sort(key=lambda e: -e[2])
            print("\n  worst quantization error (mean Oklab distance):")
            for cls, i, err in errors[:15]:
                name = self.meta["lumps"][str(i)].get("name") or "unnamed"
                print(f"    {cls:8s} {i:5d} {name:14s} {err:.4f}")

    def _src(self, cls, i, suffix=""):
        return Image.open(self.work / "png" / cls / f"{i}{suffix}.png")

    def _save(self, path, indices, palette):
        Image.fromarray(np.ascontiguousarray(indices), "L").save(path, optimize=True)
        if self.preview:
            prev = path.parent.parent.parent / "preview" / path.parent.name / path.name
            Image.fromarray(pal.to_rgb(indices, palette), "RGB").save(prev,
                                                                     optimize=True)

    def _do_tiled(self, cls, i, entry, up, d):
        rgb = np.asarray(self._src(cls, i).convert("RGB"))
        big = up.wrap(rgb, wrap_x=True, wrap_y=True)
        q = self.quantizer(cls, self.game_pal)
        idx = q(big)
        self._save(d / f"{i}.png", idx, self.game_pal)
        return q.error(big, idx)

    def _do_wall(self, i, entry, up, d):
        return self._do_tiled("wall", i, entry, up, d)

    def _do_flat(self, i, entry, up, d):
        return self._do_tiled("flat", i, entry, up, d)

    def _do_sprite(self, i, entry, up, d):
        rgb = np.asarray(self._src("sprite", i).convert("RGB"))
        mask = np.asarray(self._src("sprite", i, ".mask").convert("L")) >= 128

        # The mask marks the stored span; what actually renders is the span
        # minus its interior transparent pixels (ScaleMaskedPost skips index 0),
        # and that is the silhouette the model should be shown.
        visible = mask & (np.asarray(self._src("sprite", i).convert("L")) != 0)
        visible |= mask & (rgb.sum(axis=2) > 0)

        filled = fill_transparent(rgb, visible)
        big = up.pad(filled)
        big_mask = up.mask(visible)

        q = self.quantizer("sprite", self.game_pal)
        idx = q(big)
        idx[~big_mask] = 0                      # index 0 == transparent
        self._save(d / f"{i}.png", idx, self.game_pal)
        Image.fromarray((big_mask * 255).astype(np.uint8), "L").save(
            d / f"{i}.mask.png", optimize=True)
        return q.error(big[big_mask], idx[big_mask]) if big_mask.any() else 0.0

    def _do_pic(self, i, entry, up, d):
        rgb = np.asarray(self._src("pic", i).convert("RGB"))
        src = np.asarray(self._src("pic", i, ".idx").convert("L"))

        # Index 0 is transparent in every VI_DrawMaskedPic path -- the weapon,
        # the cursor, the heart, the menu sliders.  It has to be *preserved*,
        # which is a different job from keeping the network out of it: the
        # quantizer excludes 0 so no new holes appear, and the original holes
        # come back from the upscaled mask.  Without this the weapon renders
        # inside a black rectangle.
        opaque = src != 0
        if entry.get("masked"):
            rgb = fill_transparent(rgb, opaque)

        big = up.pad(rgb)
        # Full-screen art (briefings, logos, portraits) is drawn under its own
        # palette lump, not the game's; quantizing it against lump 91 wrecks it.
        palette = (np.array(entry["palette_rgb"], np.uint8)
                   if "palette_rgb" in entry else self.game_pal)
        semantic = (entry.get("name") or "").lower() in pal.SEMANTIC_LUMPS
        q = self.quantizer("pic_semantic" if semantic else "pic", palette)
        idx = q(big)

        # Text baked into the art -- the whole of info1, the briefing screens --
        # is destroyed by the model, so hard edges come from EPX instead.  This
        # has to land before the mask re-cut and the marker restore below, both
        # of which must still have the last word on index semantics.
        if self.sharp_threshold > 0:
            m = pixelart.edge_mask(src, self.lab(palette), self.sharp_threshold)
            m4 = np.repeat(np.repeat(m, SCALE, axis=0), SCALE, axis=1)
            idx[m4] = pixelart.epx4(src)[m4]

        if entry.get("masked"):
            idx[~up.mask(opaque)] = 0
        if semantic:
            idx = self._keep_markers(i, idx)
        self._save(d / f"{i}.png", idx, palette)
        return q.error(big, idx)

    def _keep_markers(self, i, idx):
        """Restore the status bar's marker indices by nearest-neighbour.

        Marker pixels carry meaning, not colour -- see palette.SEMANTIC_INDICES.
        They are replicated 4x from the original indices so their geometry
        survives exactly; they are solid bars, so the blockiness is invisible,
        and it is the only way the meters keep working.
        """
        src = np.asarray(self._src("pic", i, ".idx").convert("L"))
        near = np.repeat(np.repeat(src, 4, axis=0), 4, axis=1)
        near = near[: idx.shape[0], : idx.shape[1]]
        keep = np.isin(near, list(pal.SEMANTIC_INDICES))
        out = idx.copy()
        out[keep] = near[keep]
        return out

    def _do_font(self, i, entry, up, d):
        """Fonts are EPX only -- see pixelart.py for why the model cannot help.

        The two lump kinds differ and neither tolerates interpolation: font1's
        bytes are ramp offsets that FN_RawPrint adds fontbasecolor to, while
        font2 and font3 are drawn with fontbasecolor=0 at every call site
        (Display.c) and so their bytes are palette indices outright.  EPX copies
        values rather than blending them, which is correct for both.
        """
        atlas = np.asarray(self._src("font", i).convert("L"))
        out = np.zeros((atlas.shape[0] * SCALE, atlas.shape[1] * SCALE), np.uint8)
        # Per glyph, not per atlas: extract._font_atlas packs them edge to edge
        # with no gutter, and EPX's 3x3 window would otherwise pull a
        # neighbouring letter into a glyph's first and last column.  Slicing at
        # x*SCALE keeps the geometry pack._font_from_atlas expects.
        for g in entry["glyphs"]:
            x, w = g["x"], g["w"]
            out[:, x * SCALE:(x + w) * SCALE] = pixelart.epx4(atlas[:, x:x + w])
        Image.fromarray(out, "L").save(d / f"{i}.png", optimize=True)
        return 0.0


def _progress(items, label):
    try:
        import sys

        from tqdm import tqdm
        # Only when a terminal is watching; redirected to a file the bar's
        # carriage returns turn into thousands of lines of noise.
        return tqdm(items, desc=f"  {label:8s}", unit="lump", leave=False,
                    disable=not sys.stderr.isatty())
    except ImportError:
        return items


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--work", default=DEFAULT_WORK, type=Path)
    ap.add_argument("--only", help="comma-separated classes")
    ap.add_argument("--model", help="override the model for every class")
    ap.add_argument("--device", default="auto", help="cuda / mps / cpu / auto")
    ap.add_argument("--resume", action="store_true", help="skip existing outputs")
    ap.add_argument("--limit", type=int, help="first N lumps per class, for a smoke test")
    ap.add_argument("--report", action="store_true", help="print worst quantization error")
    ap.add_argument("--sharp-threshold", type=float, default=SHARP_THRESHOLD,
                    help="Oklab edge threshold for taking a pic's hard edges "
                         "from EPX rather than the model; 0 disables")
    ap.add_argument("--no-preview", action="store_true")
    a = ap.parse_args()

    models = {c: a.model for c in DEFAULT_MODELS} if a.model else None
    p = Processor(a.work, models, a.device, preview=not a.no_preview,
                  sharp_threshold=a.sharp_threshold)
    p.run(set(a.only.split(",")) if a.only else None,
          resume=a.resume, limit=a.limit, report=a.report)


if __name__ == "__main__":
    main()
