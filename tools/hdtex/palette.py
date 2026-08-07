"""Palette handling and requantization.

The engine is 8-bit paletted end to end: every pixel is an index, lighting is 64
colormaps that remap indices (R_public.c:261-268), and translucency is a
255x256 blend table.  So upscaled art has to come back to 256 indices -- we gain
detail and sharper edges, not new colours.

Two things worth knowing:

  * The palette lump stores 6-bit VGA DAC levels (0..63), not 8-bit.  The
    expansion the port uses is (v<<2)|(v>>4) (sys_video.c:84-101), which maps
    63 to 255 exactly; a plain v*4 would top out at 252 and tint everything.

  * The full-screen art (briefings, logos, character portraits) does not use
    the game palette at all -- each such pic is followed in the archive by its
    own 768-byte palette lump.  See assets.screen_palettes.

Matching is nearest-neighbour in Oklab.  Oklab because equal distances in it are
roughly equal perceptual differences, which sRGB emphatically does not give you;
the practical effect is that dark ramps stop collapsing onto one index.

No dithering, deliberately.  It would fight the upscaler's smooth gradients, and
because the 64 colormaps remap every index at draw time, a dither pattern turns
into crawling noise as lighting changes rather than averaging out.
"""

import numpy as np

PALETTE_LUMP = "palette"

# Palette indices the status bar uses as *markers* rather than colours.
#
# The meters are not drawn: the art paints index 254 across the bar region, and
# every frame Display.c scans a fixed rectangle and rewrites 254 -- or a value
# it wrote there previously -- with a gradient computed from shield/health
# (Display.c:340-366, 680-760).  So 254 means "meter pixel, not yet filled" and
# 113..168 means "meter pixel I already filled".
#
# Upscaling breaks this in both directions.  Measured on statbar1-3: all 524
# index-254 pixels are destroyed, so the meters would never fill; and 71-80
# spurious 140..168 pixels appear where the network blended neighbouring
# colours into that range, so unrelated pixels would flash with the meter
# colour.  Marker pixels therefore have to survive the upscale as *indices*,
# which means nearest-neighbour, and the network's output must be kept out of
# those ranges entirely.
SEMANTIC_INDICES = frozenset([254]) | frozenset(range(113, 169))

# Which lumps that applies to.  Only the status bar is scanned this way; menus
# and briefing screens use 113..168 as ordinary colours and must keep them.
SEMANTIC_LUMPS = ("statbar1", "statbar2", "statbar3", "statbar4")

RESERVED = {
    # Index 0 is the transparent colour in sprites (DOOMGRB.C:11) and 255 is
    # patched to 0 on load (Spawn.c:41-66), so an opaque sprite pixel must be
    # neither -- emitting either punches a hole in the art.
    "sprite": (0, 255),
    # The HD compositor keys the 2D chrome texture on index 0 (sys_video.c:240),
    # and Display.c:348-366 uses 254 as a colour-substitution marker.
    "pic": (0, 254),
    # The status bar additionally must not have the network land anywhere in
    # the meter ranges, or unrelated pixels get repainted with the meter colour.
    "pic_semantic": tuple(sorted({0} | SEMANTIC_INDICES)),
    # Walls and flats are fully opaque; every index is a real colour.
    "wall": (),
    "flat": (),
}


def expand(raw):
    """768 bytes of 6-bit VGA levels -> (256,3) uint8 RGB, as sys_video.c does."""
    if len(raw) != 768:
        raise ValueError(f"palette lump is {len(raw)} bytes, expected 768")
    v = np.frombuffer(raw, np.uint8).reshape(256, 3).astype(np.uint16)
    return (((v << 2) | (v >> 4)) & 0xFF).astype(np.uint8)


def compress(rgb):
    """(256,3) 8-bit RGB -> 768 bytes of 6-bit levels.  Inverse of expand()."""
    return (np.asarray(rgb, np.uint8) >> 2).tobytes()


def load(archive):
    """The game palette, as (256,3) uint8."""
    return expand(archive[archive.num(PALETTE_LUMP)].data)


# ----------------------------------------------------------------- colour space

def _srgb_to_linear(c):
    c = np.asarray(c, np.float32) / 255.0
    return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)


def to_oklab(rgb):
    """(...,3) uint8 sRGB -> (...,3) float32 Oklab.  Ottosson's matrices.

    The VGA palette is not really sRGB, but source and target both go through
    this same transform, so the assumption cancels out of the comparison.
    """
    lin = _srgb_to_linear(rgb)
    r, g, b = lin[..., 0], lin[..., 1], lin[..., 2]
    l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b
    l_, m_, s_ = np.cbrt(l), np.cbrt(m), np.cbrt(s)
    return np.stack([
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_,
    ], axis=-1).astype(np.float32)


# ------------------------------------------------------------------- quantizer

class Quantizer:
    """Nearest palette index in Oklab, with a set of indices held back.

    Reserved indices are excluded from the *candidate* set rather than remapped
    afterwards, so a pixel that would have matched index 0 lands on the closest
    usable colour instead of on whatever a post-hoc fixup happened to pick.
    """

    def __init__(self, palette, reserved=()):
        self.palette = np.asarray(palette, np.uint8)
        self.allowed = np.array(
            [i for i in range(len(self.palette)) if i not in set(reserved)],
            np.uint8,
        )
        if not len(self.allowed):
            raise ValueError("every palette index is reserved")
        self._lab = to_oklab(self.palette[self.allowed])   # (K,3)

    def __call__(self, rgb, chunk=1 << 18):
        """(H,W,3) uint8 -> (H,W) uint8 indices."""
        rgb = np.asarray(rgb, np.uint8)
        h, w = rgb.shape[:2]

        out = _torch_quantize(rgb, self._lab, self.allowed)
        if out is not None:
            return out.reshape(h, w)

        # CPU path.  Quantize over the image's *distinct* colours only: upscaled
        # art repeats colours heavily, so this is typically a 3-5x cut, and it
        # is exact -- unlike snapping to a coarse RGB lattice.  On the GPU the
        # same trick is a pessimisation, which is why it is only down here: the
        # cost is dominated by getting the data across, and np.unique on a
        # million pixels costs more than the search it saves.
        flat = rgb.reshape(-1, 3)
        colours, inverse = np.unique(flat, axis=0, return_inverse=True)
        lab = to_oklab(colours)

        gpu = _torch_argmin(lab, self._lab)
        if gpu is not None:
            picked = gpu
        else:
            picked = np.empty(lab.shape[0], np.int64)
            # Chunked so the (N,K) distance matrix stays a few MB rather than
            # hundreds for a full-screen pic.
            for i in range(0, lab.shape[0], chunk):
                block = lab[i : i + chunk]
                d = ((block[:, None, :] - self._lab[None, :, :]) ** 2).sum(-1)
                picked[i : i + chunk] = d.argmin(1)

        return self.allowed[picked][inverse].reshape(h, w)

    def error(self, rgb, indices):
        """Mean Oklab distance between the requested colours and what we picked."""
        want = to_oklab(np.asarray(rgb, np.uint8).reshape(-1, 3))
        got = to_oklab(self.palette[np.asarray(indices).reshape(-1)])
        return float(np.sqrt(((want - got) ** 2).sum(-1)).mean())


def to_rgb(indices, palette):
    """(H,W) indices -> (H,W,3) uint8, for writing a PNG or feeding the model."""
    return np.asarray(palette, np.uint8)[np.asarray(indices)]


_DEVICE = ...          # resolved lazily; ... means "not looked up yet"


def _torch_argmin(lab, palette_lab):
    """Nearest-palette-entry search on the GPU, or None if unavailable.

    torch is already a dependency for the upscaler and the whole job is one
    (N,K) distance matrix, so there is no reason to grind it on the CPU -- it
    was taking longer than the network inference it follows.
    """
    global _DEVICE
    if _DEVICE is ...:
        try:
            import torch
            if torch.cuda.is_available():
                _DEVICE = torch.device("cuda")
            elif getattr(torch.backends, "mps", None) and torch.backends.mps.is_available():
                _DEVICE = torch.device("mps")
            else:
                _DEVICE = None          # CPU torch is no faster than numpy here
        except ImportError:
            _DEVICE = None
    if _DEVICE is None:
        return None

    import torch
    with torch.inference_mode():
        a = torch.from_numpy(np.ascontiguousarray(lab)).to(_DEVICE)
        b = torch.from_numpy(np.ascontiguousarray(palette_lab)).to(_DEVICE)
        return torch.cdist(a, b).argmin(1).cpu().numpy()


def _torch_oklab(x):
    """Oklab for a (N,3) float tensor of 0..1 linear-ready sRGB, on device."""
    import torch
    c = torch.where(x <= 0.04045, x / 12.92, ((x + 0.055) / 1.055) ** 2.4)
    r, g, b = c[:, 0], c[:, 1], c[:, 2]
    l = 0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b
    m = 0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b
    s = 0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b
    l_, m_, s_ = l.pow(1 / 3), m.pow(1 / 3), s.pow(1 / 3)
    return torch.stack([
        0.2104542553 * l_ + 0.7936177850 * m_ - 0.0040720468 * s_,
        1.9779984951 * l_ - 2.4285922050 * m_ + 0.4505937099 * s_,
        0.0259040371 * l_ + 0.7827717662 * m_ - 0.8086757660 * s_,
    ], dim=1)


def _torch_quantize(rgb, palette_lab, allowed, chunk=1 << 20):
    """Whole-image nearest-palette search on the GPU, or None if unavailable.

    Converts and searches every pixel rather than deduplicating first.  For a
    1280x800 cutscene frame that is a million pixels against 256 candidates --
    nothing for the GPU, and it removes the np.unique that dominated the CPU
    path at ~1s per frame.
    """
    _torch_argmin(np.zeros((1, 3), np.float32), palette_lab)   # resolve _DEVICE
    if _DEVICE is None:
        return None

    import torch
    with torch.inference_mode():
        pal_t = torch.from_numpy(np.ascontiguousarray(palette_lab)).to(_DEVICE)
        allowed_t = torch.from_numpy(allowed.astype(np.int64)).to(_DEVICE)
        flat = torch.from_numpy(
            np.ascontiguousarray(rgb.reshape(-1, 3))).to(_DEVICE)
        out = torch.empty(flat.shape[0], dtype=torch.uint8, device=_DEVICE)
        for i in range(0, flat.shape[0], chunk):
            block = flat[i : i + chunk].float().div_(255.0)
            lab = _torch_oklab(block)
            out[i : i + chunk] = allowed_t[
                torch.cdist(lab, pal_t).argmin(1)].to(torch.uint8)
        return out.cpu().numpy()
