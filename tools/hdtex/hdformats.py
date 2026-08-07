"""Lump layouts for the 4x sidecar archive.

Three of the five classes keep their original layout and simply get bigger:

  wall   {u16 height; u16 collumnofs[64];} + `w` columns of height*4 bytes.
         The header stays 130 bytes even though there are now 256 columns:
         collumnofs has been vestigial since 1995 -- the engine recomputes the
         column table in Utils.c:353-363 -- so widening it would only force the
         renderer's `base = wall + 65*2` and `size = *wall*4` to change for no
         benefit.  Keeping it also means `height` still reads as rows/4 (128 for
         a 512-row wall), which is what sp_loopvalue and span_p->light want.
  flat   raw row-major, 65536 bytes instead of 4096.
  pic    {s16 width, height, orgx, orgy;} -- already self-describing, and
         1280x800 fits the int16 fields fine.

Two classes overflow their original layout at 4x and need a widened one:

  sprite  collumnofs is s16 and only 256 entries; per-column top/bottom are u8.
          Measured against the archive, 4x needs width 704 (vs the 256 cap),
          offsets to 344704 (vs 32767) and top to 780 (vs 255) -- all three
          fields overflow, so the container fails before the arithmetic does.
  font    charofs is s16.  font1 and font2 reach 33506 and 37250 bytes at 4x,
          just past the 32767 cap.  (font3 would fit, but a single layout is
          worth more than saving 2 KB on one lump.)

Both widened layouts keep field *order* identical to the original so the engine
reads them with the same code shape, just wider types.
"""

import struct

import numpy as np

from formats import FormatError, encode_flat, encode_pic, encode_wall  # noqa: F401

# Sprite: s16 leftoffset, s16 width, s32 collumnofs[width]
#         per column: s16 top, s16 bottom, u8 pixels[top-bottom+1]
SPRITE_HDR = 4
SPRITE_OFS = 4          # bytes per collumnofs entry
SPRITE_COLHDR = 4       # bytes of top/bottom per column

# Font: s16 height, u8 width[256], s32 charofs[256]
FONT_HDR = 2 + 256 + 4 * 256


def encode_sprite_hd(arr, mask, meta=None):
    """Widened dsprite.  Same shape as encode_sprite, wider fields."""
    meta = meta or {}
    height, width = arr.shape
    if width > 0x7FFF:
        raise FormatError(f"sprite width {width} overflows the s16 width field")
    if height > 0x7FFF:
        raise FormatError(f"sprite height {height} overflows the s16 top field")

    hdr = SPRITE_HDR + SPRITE_OFS * width
    offsets = [0] * width
    body = bytearray()
    for x in range(width):
        rows = np.flatnonzero(mask[:, x])
        if rows.size == 0:
            continue                       # 0 == empty column, as before
        r0, r1 = int(rows[0]), int(rows[-1])
        top, bottom = height - 1 - r0, height - 1 - r1
        offsets[x] = hdr + len(body)
        body += struct.pack("<hh", top, bottom) + arr[r0 : r1 + 1, x].tobytes()

    return (struct.pack("<hh", meta.get("leftoffset", 0), width)
            + struct.pack(f"<{width}i", *offsets) + bytes(body))


def decode_sprite_hd(data):
    """Inverse of encode_sprite_hd, for --verify."""
    leftoffset, width = struct.unpack_from("<hh", data, 0)
    offsets = struct.unpack_from(f"<{width}i", data, SPRITE_HDR)
    cols, height = [], 0
    for x, off in enumerate(offsets):
        if off == 0:
            cols.append(None)
            continue
        top, bottom = struct.unpack_from("<hh", data, off)
        n = top - bottom + 1
        cols.append((top, bottom, data[off + SPRITE_COLHDR : off + SPRITE_COLHDR + n]))
        height = max(height, top + 1)

    arr = np.zeros((height, width), np.uint8)
    mask = np.zeros((height, width), bool)
    for x, col in enumerate(cols):
        if col is None:
            continue
        top, bottom, px = col
        r0, r1 = height - 1 - top, height - 1 - bottom
        arr[r0 : r1 + 1, x] = np.frombuffer(px, np.uint8)
        mask[r0 : r1 + 1, x] = True
    return arr, mask, {"leftoffset": leftoffset}


def encode_font_hd(glyphs, meta):
    """Widened font_t: s16 height, u8 width[256], s32 charofs[256]."""
    height = meta["height"]
    widths = [0] * 256
    charofs = [0] * 256
    body = bytearray()
    for ch in meta["order"]:
        g = glyphs[ch]
        if g.shape[0] != height:
            raise FormatError(f"font glyph {ch} is {g.shape[0]} rows, expected {height}")
        if g.shape[1] > 0xFF:
            raise FormatError(f"font glyph {ch} is {g.shape[1]} wide, "
                              "past the u8 width field")
        widths[ch] = g.shape[1]
        charofs[ch] = FONT_HDR + len(body)
        body += g.T.tobytes()               # column-major, as D_font.c reads it
    return (struct.pack("<h", height) + struct.pack("<256B", *widths)
            + struct.pack("<256i", *charofs) + bytes(body))


def decode_font_hd(data):
    (height,) = struct.unpack_from("<h", data, 0)
    widths = struct.unpack_from("<256B", data, 2)
    charofs = struct.unpack_from("<256i", data, 2 + 256)
    glyphs = {}
    for ch in range(256):
        w, off = widths[ch], charofs[ch]
        if not w or not off:
            continue
        glyphs[ch] = (np.frombuffer(data, np.uint8, count=w * height, offset=off)
                      .reshape(w, height).T.copy())
    order = sorted((c for c in range(256) if widths[c] and charofs[c]),
                   key=lambda c: charofs[c])
    return glyphs, {"height": height, "order": order}
