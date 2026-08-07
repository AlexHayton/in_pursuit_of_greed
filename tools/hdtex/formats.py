"""Decoders and encoders for every graphic lump class in GREED.BLO.

Each decoder returns a dense numpy uint8 array of *palette indices* in normal
image order -- arr[row][col], row 0 at the top -- regardless of how the lump
stores it.  Each encoder is the exact inverse: decode -> encode reproduces the
original lump byte for byte (proven over all 2332 lumps by roundtrip.py).

The layouts come from the original 1995 grabbers, source_dos/SGRAB/RAVGRAB.C
(walls, flats) and source_dos/SGRAB/DOOMGRB.C (sprites), cross-checked against
the renderer in source_shared/src/.

Two things are easy to get wrong and are handled explicitly here:

  * Walls and sprites are stored COLUMN-major; flats, pics and fonts are
    row-major.  Only the wall/sprite decoders transpose.

  * A sprite column stores every pixel between its first and last opaque row,
    including transparent ones in the middle, and ScaleMaskedPost (R_spans.c:249)
    skips them at draw time.  So "is this pixel inside the stored span" is NOT
    the same question as "is this pixel nonzero", and the span is what has to be
    preserved to re-encode exactly.  Sprite decoding therefore returns a mask
    alongside the indices.
"""

import struct

import numpy as np

TRANSCOLOR = 0          # index 0 is transparent in sprites (DOOMGRB.C:11)

WALL_HDR = 130          # u16 height + u16 collumnofs[64]; RAVGRAB.C:13-17
FLAT_SIZE = 64          # GrabFlat hardcodes 64x64; RAVGRAB.C:95-122
PIC_HDR = 8             # s16 width, height, orgx, orgy


class FormatError(Exception):
    pass


# --------------------------------------------------------------------- walls

def decode_wall(data):
    """-> (arr[h][w] uint8, meta).

    Layout: u16 height (in units of 4 rows), u16 collumnofs[64], then `w`
    columns of `height*4` bytes each.  The stored collumnofs are vestigial and
    slightly wrong -- RAVGRAB.C:67 writes `65*2 + x*h - (96-h)` -- which is why
    the engine recomputes them in Utils.c:353-363 and why we ignore them here.
    """
    if len(data) < WALL_HDR:
        raise FormatError(f"wall lump is {len(data)} bytes, shorter than its header")
    (height4,) = struct.unpack_from("<H", data, 0)
    h = height4 * 4
    if h <= 0:
        raise FormatError(f"wall height field {height4} is not positive")
    body = len(data) - WALL_HDR
    if body % h:
        raise FormatError(f"wall body {body} is not a multiple of height {h}")
    w = body // h
    # column-major on disk: data[HDR + x*h + y] is (row y, col x)
    arr = np.frombuffer(data, np.uint8, count=w * h, offset=WALL_HDR)
    return arr.reshape(w, h).T.copy(), {"collumnofs": data[2:WALL_HDR]}


def encode_wall(arr, meta=None):
    h, w = arr.shape
    if h % 4:
        raise FormatError(f"wall height {h} must be a multiple of 4 (stored as h/4)")
    if h // 4 > 0xFFFF:
        raise FormatError(f"wall height {h} overflows the u16 height field")
    # Preserve the original vestigial table when re-encoding an untouched lump
    # so the roundtrip is bit-exact; synthesise RAVGRAB.C's formula otherwise.
    if meta and "collumnofs" in meta and len(meta["collumnofs"]) == WALL_HDR - 2:
        table = meta["collumnofs"]
    else:
        table = struct.pack("<64H", *[(65 * 2 + x * h - (96 - h)) & 0xFFFF
                                      for x in range(64)])
    return struct.pack("<H", h // 4) + table + arr.T.tobytes()


# --------------------------------------------------------------------- flats

def decode_flat(data):
    n = FLAT_SIZE * FLAT_SIZE
    if len(data) != n:
        raise FormatError(f"flat lump is {len(data)} bytes, expected {n}")
    return np.frombuffer(data, np.uint8).reshape(FLAT_SIZE, FLAT_SIZE).copy(), {}


def encode_flat(arr, meta=None):
    return arr.tobytes()


# ---------------------------------------------------------------------- pics

def decode_pic(data):
    """-> (arr[h][w], meta with orgx/orgy).  Self-describing, row-major."""
    if len(data) < PIC_HDR:
        raise FormatError(f"pic lump is {len(data)} bytes, shorter than its header")
    w, h, orgx, orgy = struct.unpack_from("<hhhh", data, 0)
    if w <= 0 or h <= 0:
        raise FormatError(f"pic has non-positive dimensions {w}x{h}")
    if len(data) < PIC_HDR + w * h:
        raise FormatError(f"pic {w}x{h} needs {PIC_HDR + w * h} bytes, has {len(data)}")
    arr = np.frombuffer(data, np.uint8, count=w * h, offset=PIC_HDR).reshape(h, w)
    # A few lumps carry trailing bytes past the image; keep them for the roundtrip.
    tail = data[PIC_HDR + w * h:]
    return arr.copy(), {"orgx": orgx, "orgy": orgy, "tail": tail}


def encode_pic(arr, meta=None):
    meta = meta or {}
    h, w = arr.shape
    return (struct.pack("<hhhh", w, h, meta.get("orgx", 0), meta.get("orgy", 0))
            + arr.tobytes() + meta.get("tail", b""))


# ------------------------------------------------------------------- sprites

def decode_sprite(data):
    """-> (arr[h][w], mask[h][w] bool, meta).

    Layout (DOOMGRB.C:14-21): s16 leftoffset, s16 width, s16 collumnofs[width]
    -- only `width` entries, so the header is 4 + 2*width bytes, not 4 + 512.
    collumnofs[x] is a byte offset from the start of the lump, 0 for an empty
    column.  Each column is { u8 top; u8 bottom; u8 pixels[top-bottom+1] } where
    top and bottom are heights above the sprite's baseline: DOOMGRB.C:110-111
    writes `h-1-first` and `h-1-last`, so top >= bottom and the pixels run
    downward from `top`.

    The lump never stores its own height.  We use h = max(top)+1, the tightest
    array that keeps every top/bottom value intact -- which is what matters,
    since R_spans.c:365-366 positions columns by those values against
    `shapebottom`, so a column that stops short of the baseline (bottom > 0)
    genuinely floats above it and must stay that way.

    `mask` marks pixels inside a stored span.  Interior transparent pixels are
    inside the span but zero-valued; see the module docstring.
    """
    if len(data) < 4:
        raise FormatError(f"sprite lump is {len(data)} bytes, shorter than its header")
    leftoffset, width = struct.unpack_from("<hh", data, 0)
    if width <= 0:
        raise FormatError(f"sprite width {width} is not positive")
    hdr = 4 + 2 * width
    if len(data) < hdr:
        raise FormatError(f"sprite of width {width} needs a {hdr}-byte header, "
                          f"lump is {len(data)}")
    offsets = struct.unpack_from(f"<{width}H", data, 4)

    cols = []
    height = 0
    for x, off in enumerate(offsets):
        if off == 0:
            cols.append(None)
            continue
        if off + 2 > len(data):
            raise FormatError(f"sprite column {x} offset {off} outside lump")
        top, bottom = data[off], data[off + 1]
        if bottom > top:
            raise FormatError(f"sprite column {x} has bottom {bottom} above top {top}")
        n = top - bottom + 1
        if off + 2 + n > len(data):
            raise FormatError(f"sprite column {x} runs {n} px past the end of the lump")
        cols.append((top, bottom, data[off + 2 : off + 2 + n]))
        height = max(height, top + 1)

    arr = np.zeros((height, width), np.uint8)
    mask = np.zeros((height, width), bool)
    for x, col in enumerate(cols):
        if col is None:
            continue
        top, bottom, px = col
        # height `t` above the baseline lives at row (height-1-t)
        r0, r1 = height - 1 - top, height - 1 - bottom
        arr[r0 : r1 + 1, x] = np.frombuffer(px, np.uint8)
        mask[r0 : r1 + 1, x] = True
    return arr, mask, {"leftoffset": leftoffset}


def encode_sprite(arr, mask, meta=None):
    meta = meta or {}
    height, width = arr.shape
    if width > 0xFFFF:
        raise FormatError(f"sprite width {width} overflows the u16 collumnofs count")

    hdr = 4 + 2 * width
    offsets = [0] * width
    body = bytearray()
    for x in range(width):
        rows = np.flatnonzero(mask[:, x])
        if rows.size == 0:
            continue
        r0, r1 = int(rows[0]), int(rows[-1])
        top, bottom = height - 1 - r0, height - 1 - r1
        if top > 0xFF:
            raise FormatError(
                f"sprite column {x} top {top} overflows the u8 field; "
                "the original layout caps sprite height at 256 rows"
            )
        off = hdr + len(body)
        if off > 0xFFFF:
            raise FormatError(
                f"sprite column {x} lands at offset {off}, past the u16 collumnofs "
                "limit of 65535; the original layout cannot hold this sprite"
            )
        offsets[x] = off
        body += bytes((top, bottom)) + arr[r0 : r1 + 1, x].tobytes()

    return (struct.pack("<hh", meta.get("leftoffset", 0), width)
            + struct.pack(f"<{width}H", *offsets) + bytes(body))


# --------------------------------------------------------------------- fonts

def decode_font(data):
    """-> (glyphs {ch: arr[h][w]}, meta).

    font_t (D_font.h:31-36): s16 height; u8 width[256]; s16 charofs[256].
    Glyph pixels are COLUMN-major (D_font.c:59-66 walks y innermost then
    `source++` per column) and charofs is a byte offset from the lump start.

    Glyph bytes are NOT palette indices -- FN_RawPrint adds them to
    `fontbasecolor`, so they are offsets into a colour ramp that the caller
    chooses at draw time.  They must never be palette-quantized.
    """
    if len(data) < 2 + 256 + 512:
        raise FormatError(f"font lump is {len(data)} bytes, shorter than font_t")
    (height,) = struct.unpack_from("<h", data, 0)
    widths = struct.unpack_from("<256B", data, 2)
    charofs = struct.unpack_from("<256h", data, 2 + 256)
    if height <= 0:
        raise FormatError(f"font height {height} is not positive")

    glyphs = {}
    for ch in range(256):
        w, off = widths[ch], charofs[ch]
        if w == 0 or off == 0:
            continue
        n = w * height
        if off < 0 or off + n > len(data):
            raise FormatError(f"font glyph {ch} at {off}+{n} outside lump")
        glyphs[ch] = (np.frombuffer(data, np.uint8, count=n, offset=off)
                      .reshape(w, height).T.copy())
    # Glyphs are not necessarily stored in character order: font3 holds the
    # digits first and appends its space glyph after them.  Record the order the
    # file actually uses so re-encoding puts every glyph back where it was.
    order = sorted((c for c in range(256) if widths[c] and charofs[c]),
                   key=lambda c: charofs[c])
    return glyphs, {"height": height, "order": order}


def encode_font(glyphs, meta):
    height = meta["height"]
    widths = [0] * 256
    charofs = [0] * 256
    body = bytearray()
    base = 2 + 256 + 512
    # Emit in the original order so offsets land where they did before.
    for ch in meta["order"]:
        g = glyphs[ch]
        if g.shape[0] != height:
            raise FormatError(f"font glyph {ch} is {g.shape[0]} rows, expected {height}")
        widths[ch] = g.shape[1]
        charofs[ch] = base + len(body)
        if charofs[ch] > 0x7FFF:
            raise FormatError(f"font glyph {ch} at {charofs[ch]} overflows s16 charofs")
        body += g.T.tobytes()
    return (struct.pack("<h", height) + struct.pack("<256B", *widths)
            + struct.pack("<256h", *charofs) + bytes(body))
