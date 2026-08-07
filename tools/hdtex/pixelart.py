"""EPX/Scale2x upscaling for art whose pixels are indices rather than colour.

Real-ESRGAN is the wrong tool for glyphs.  It has no notion of a letterform, and
the game's text is 6 pixels tall at 320x200 -- below anything the model can
preserve.  It blurs strokes together and hallucinates detail into the gaps: the
help screen came out reading VERPON SELECTION, LEPTSHIFT, DUMP, SPADE BAR.

Worse, running it over a *palette-indexed* image interpolates between indices.
font2 stores five distinct values and font3 nine; the upscaled versions used
every value in 0..132 and 0..133, of which 35% and 48% of pixels were indices
that appear nowhere in the source.  Those land in unrelated parts of the VGA
palette, so a green glyph came out fringed with red, white and grey speckle.
This is the same trap as the status bar's meter markers (palette.SEMANTIC_INDICES)
-- indices that carry meaning rather than colour -- one class further on.

EPX sidesteps both.  Every output pixel is *copied* from one of five input
neighbours, so the output value set is a subset of the input's by construction:
no quantizer, no interpolation, no stray indices, and index 0 (transparent, or
"skip" in the font blitters) survives exactly.
"""

import numpy as np

SCALE = 4


def _epx2(a):
    """One EPX doubling of a 2-D index array.

    Each source pixel C becomes four, and a corner takes a neighbour's value
    only where two adjacent neighbours agree and the opposing pair does not --
    which is what rounds a staircase without inventing a colour.
    """
    h, w = a.shape
    P = np.pad(a, 1, mode="edge")
    C = P[1:h + 1, 1:w + 1]
    A = P[0:h, 1:w + 1]
    B = P[1:h + 1, 2:w + 2]
    D = P[1:h + 1, 0:w]
    E = P[2:h + 2, 1:w + 1]
    o = np.repeat(np.repeat(C, 2, 0), 2, 1)
    o[0::2, 0::2] = np.where((D == A) & (D != E) & (A != B), A, C)
    o[0::2, 1::2] = np.where((A == B) & (A != D) & (B != E), B, C)
    o[1::2, 0::2] = np.where((E == D) & (E != B) & (D != A), D, C)
    o[1::2, 1::2] = np.where((B == E) & (B != A) & (E != D), E, C)
    return o


def epx4(a):
    """x4 to match Upscaler.SCALE, as two doublings."""
    return _epx2(_epx2(a))


def edge_mask(indices, palette_lab, threshold, dilate=True):
    """Where the source has a hard edge, in perceptual terms.

    Text is uniformly high-contrast against whatever it sits on, so this finds
    it without needing to know which lumps contain any.  Object silhouettes
    match too, which is wanted: EPX handles a hard edge better than the model,
    whose ringing there is the other thing that looks wrong at 4x.

    Dilating by one source pixel covers the body of a 1-pixel stroke and its
    immediate surround, so a glyph is taken from EPX whole rather than edge-only.
    """
    lab = palette_lab[indices]
    d = np.zeros(indices.shape, np.float32)
    for axis, shift in ((0, 1), (0, -1), (1, 1), (1, -1)):
        d = np.maximum(d, np.linalg.norm(lab - np.roll(lab, shift, axis), axis=2))
    m = d > threshold
    if dilate:
        out = m.copy()
        for axis, shift in ((0, 1), (0, -1), (1, 1), (1, -1)):
            out |= np.roll(m, shift, axis)
        return out
    return m
