"""Autodesk FLI reader and writer, matching what source_shared/src/Playfli.c decodes.

The 39 cutscenes in greed_cdrom/MOVIES are 320x200 FLI (signature 0xAF11), 5722
frames in total.  Upscaling them means decoding every frame, running it through
the network, and re-encoding -- there is no way to do it at run time.

Chunk types, exactly the five the engine handles:

  11  COLOR_64   packets of palette entries, 6-bit DAC levels
  12  LC         a band of lines, each a run of skip/copy packets
  13  BLACK      clear the frame
  15  BRUN       whole frame, one RLE run per row
  16  COPY       whole frame, uncompressed

Two sign conventions that are easy to invert, and are opposite to each other:

  BRUN  count < 0  ->  -count literal bytes ;  count > 0  ->  repeat next byte
  LC    count < 0  ->  repeat next byte     ;  count > 0  ->  count literal bytes

Both match Playfli.c (`fli_brun`, `fli_linecompression`) and the published
format.  Getting either backwards produces a frame that looks almost right,
which is why they are spelled out here.

The engine reads a per-row packet count and loops exactly that many times, so
the writer must keep it inside a byte.  At 1280 wide a BRUN row needs at most 11
packets (127 literals each); LC rows are capped explicitly and fall back to BRUN
for the whole frame if they would not fit.
"""

import struct

import numpy as np

HEADER = struct.Struct("<IHHHHHHHII")     # then 102 bytes of padding
HEADER_SIZE = 128
FRAME = struct.Struct("<IHH8x")           # size, signature, nchunks, padding
CHUNK = struct.Struct("<IH")

FLI_MAGIC = 0xAF11
FRAME_MAGIC = 0xF1FA

COLOR_64, LC, BLACK, BRUN, COPY = 11, 12, 13, 15, 16

MAX_PACKETS = 255                         # the engine's per-row counter is a byte
MAX_RUN = 127                             # signed count


class FliError(Exception):
    pass


# ----------------------------------------------------------------------- read

class Reader:
    """Decode an FLI into successive (frame, palette) pairs.

    `frame` is a (h,w) uint8 index array, `palette` a (256,3) uint8 array of
    6-bit DAC levels -- the same values the lump palettes use, so
    palette.expand() applies.
    """

    def __init__(self, path):
        self.raw = open(path, "rb").read()
        (self.size, self.signature, self.nframes, self.width, self.height,
         self.depth, self.flags, self.speed, self.next_, self.frit) = \
            HEADER.unpack_from(self.raw, 0)
        if self.signature != FLI_MAGIC:
            raise FliError(f"{path}: signature 0x{self.signature:04X}, not FLI")
        self.frame = np.zeros((self.height, self.width), np.uint8)
        self.palette = np.zeros((256, 3), np.uint8)

    def frames(self):
        pos = HEADER_SIZE
        for _ in range(self.nframes):
            if pos + FRAME.size > len(self.raw):
                break
            size, sig, nchunks = FRAME.unpack_from(self.raw, pos)
            if sig != FRAME_MAGIC:
                raise FliError(f"frame signature 0x{sig:04X} at {pos}")
            p = pos + FRAME.size
            for _ in range(nchunks):
                csize, ctype = CHUNK.unpack_from(self.raw, p)
                self._chunk(ctype, self.raw[p + CHUNK.size : p + csize])
                p += csize
            pos += size if size else FRAME.size
            yield self.frame.copy(), self.palette.copy()

    def _chunk(self, ctype, d):
        if ctype == COLOR_64:
            self._colors(d)
        elif ctype == LC:
            self._lc(d)
        elif ctype == BRUN:
            self._brun(d)
        elif ctype == BLACK:
            self.frame[:] = 0
        elif ctype == COPY:
            n = self.width * self.height
            self.frame[:] = np.frombuffer(d, np.uint8, count=n).reshape(
                self.height, self.width)
        # anything else: the engine ignores it, so do the same

    def _colors(self, d):
        i = 0
        packets = d[i] | (d[i + 1] << 8); i += 2
        idx = 0
        for _ in range(packets):
            skip, change = d[i], d[i + 1]; i += 2
            idx += skip
            n = change if change else 256
            self.palette[idx : idx + n] = np.frombuffer(
                d, np.uint8, count=n * 3, offset=i).reshape(n, 3)
            i += n * 3
            idx += n

    def _brun(self, d):
        i = 0
        flat = self.frame.reshape(-1)
        for y in range(self.height):
            base = y * self.width
            x = 0
            packets = d[i]; i += 1
            for _ in range(packets):
                count = d[i] - 256 if d[i] > 127 else d[i]; i += 1
                if count < 0:                     # literal
                    n = -count
                    flat[base + x : base + x + n] = np.frombuffer(
                        d, np.uint8, count=n, offset=i)
                    i += n; x += n
                else:                             # run
                    flat[base + x : base + x + count] = d[i]; i += 1
                    x += count

    def _lc(self, d):
        i = 0
        y = d[i] | (d[i + 1] << 8); i += 2
        n = d[i] | (d[i + 1] << 8); i += 2
        flat = self.frame.reshape(-1)
        for _ in range(n):
            base = y * self.width
            x = 0
            packets = d[i]; i += 1
            for _ in range(packets):
                x += d[i]; i += 1
                count = d[i] - 256 if d[i] > 127 else d[i]; i += 1
                if count < 0:                     # run
                    flat[base + x : base + x - count] = d[i]; i += 1
                    x += -count
                else:                             # literal
                    flat[base + x : base + x + count] = np.frombuffer(
                        d, np.uint8, count=count, offset=i)
                    i += count; x += count
            y += 1


# ---------------------------------------------------------------------- write

def _brun_row(row):
    """One BRUN row: packets byte then RLE. Returns None if it needs >255."""
    out = bytearray()
    packets = 0
    x, w = 0, len(row)
    while x < w:
        # count the run at x
        run = 1
        while run < MAX_RUN and x + run < w and row[x + run] == row[x]:
            run += 1
        if run >= 3:
            out += bytes((run, int(row[x])))
            x += run
        else:
            # gather literals until a run of 3 would start
            start = x
            while x < w and (x - start) < MAX_RUN:
                if (x + 2 < w and row[x] == row[x + 1] == row[x + 2]):
                    break
                x += 1
            n = x - start
            out += bytes((256 - n,)) + bytes(row[start:x])
        packets += 1
        if packets > MAX_PACKETS:
            return None
    return bytes((packets,)) + bytes(out)


def encode_brun(frame):
    out = bytearray()
    for row in frame:
        r = _brun_row(row)
        if r is None:
            return None
        out += r
    return bytes(out)


def _lc_row(prev, cur):
    """One LC row. Returns (packets_bytes, npackets) or None if too many."""
    w = len(cur)
    diff = np.flatnonzero(prev != cur)
    if diff.size == 0:
        return b"\x00", 0
    out = bytearray()
    packets = 0
    x = 0
    i = 0
    while i < diff.size:
        start = int(diff[i])
        # skip is a byte, so long gaps need filler packets
        while start - x > 255:
            out += bytes((255, 0))
            x += 255
            packets += 1
            if packets > MAX_PACKETS:
                return None
        # extend to the end of this changed span, tolerating short unchanged gaps
        end = start + 1
        j = i + 1
        while j < diff.size and diff[j] - end <= 2:
            end = int(diff[j]) + 1
            j += 1
        out += bytes((start - x,))
        packets += 1
        seg = cur[start:end]
        s = 0
        while s < len(seg):
            run = 1
            while run < MAX_RUN and s + run < len(seg) and seg[s + run] == seg[s]:
                run += 1
            if run >= 3:
                out += bytes((256 - run, int(seg[s])))
                s += run
            else:
                q = s
                while q < len(seg) and (q - s) < MAX_RUN:
                    if q + 2 < len(seg) and seg[q] == seg[q + 1] == seg[q + 2]:
                        break
                    q += 1
                out += bytes((q - s,)) + bytes(seg[s:q])
                s = q
            if s < len(seg):
                out += bytes((0,))      # another packet, no skip
                packets += 1
                if packets > MAX_PACKETS:
                    return None
        x = end
        i = j
    if packets > MAX_PACKETS:
        return None
    return bytes((packets,)) + bytes(out), packets


def encode_lc(prev, cur):
    """LC chunk body for the changed band, or None if it will not fit."""
    changed = np.flatnonzero((prev != cur).any(axis=1))
    if changed.size == 0:
        return b"\x00\x00\x00\x00"
    y0, y1 = int(changed[0]), int(changed[-1]) + 1
    body = bytearray(struct.pack("<HH", y0, y1 - y0))
    for y in range(y0, y1):
        r = _lc_row(prev[y], cur[y])
        if r is None:
            return None
        body += r[0]
    return bytes(body)


def encode_colors(palette, prev=None):
    """COLOR_64 chunk body, or None when nothing changed."""
    pal = np.asarray(palette, np.uint8)
    if prev is not None and np.array_equal(pal, prev):
        return None
    # One packet covering all 256; `change == 0` is the format's "256" escape.
    return struct.pack("<HBB", 1, 0, 0) + pal.tobytes()


class Writer:
    """Assemble an FLI. Frames are added in order; the header is patched at the end."""

    def __init__(self, path, width, height, speed):
        self.f = open(path, "wb")
        self.width, self.height, self.speed = width, height, speed
        self.nframes = 0
        self.prev = None
        self.prevpal = None
        self.f.write(b"\0" * HEADER_SIZE)

    def add(self, frame, palette):
        chunks = []
        c = encode_colors(palette, self.prevpal)
        if c is not None:
            chunks.append((COLOR_64, c))
            self.prevpal = np.asarray(palette, np.uint8).copy()

        # Take whichever of the two is smaller rather than always preferring the
        # delta.  On high-motion frames LC degenerates into literals plus a skip
        # byte per span and loses to a plain RLE of the whole frame -- one
        # cutscene came out at 519 KB/frame that way, five times its neighbours.
        lc = encode_lc(self.prev, frame) if self.prev is not None else None
        brun = encode_brun(frame)
        if lc is not None and (brun is None or len(lc) <= len(brun)):
            chunks.append((LC, lc))
        elif brun is not None:
            chunks.append((BRUN, brun))
        else:                                              # last resort
            chunks.append((COPY, frame.tobytes()))

        payload = bytearray()
        for ctype, data in chunks:
            payload += CHUNK.pack(CHUNK.size + len(data), ctype) + data
        self.f.write(FRAME.pack(FRAME.size + len(payload), FRAME_MAGIC,
                                len(chunks)) + bytes(payload))
        self.prev = frame.copy()
        self.nframes += 1

    def close(self):
        size = self.f.tell()
        self.f.seek(0)
        self.f.write(HEADER.pack(size, FLI_MAGIC, self.nframes,
                                 self.width, self.height, 8, 0, self.speed,
                                 0, 0) + b"\0" * 102)
        self.f.close()
        return size
