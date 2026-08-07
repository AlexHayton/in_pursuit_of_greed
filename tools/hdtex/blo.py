"""Reader and writer for the Raven/id "BLO" lump archive (GREED.BLO).

Format, confirmed against source_dos/IDLINK/IDLINK.C and verified byte-for-byte
against greed_final/GREED.BLO (2332 lumps, 19726074 bytes):

    offset 0    fileinfo_t   { int16 numlumps; int32 infotableofs, infotablesize; }
    offset 10   lump data, contiguous, in directory order, no padding, no gaps
    infotableofs
                lumpinfo_t[numlumps] { int32 filepos; uint32 size;
                                       int16 nameofs; int16 compress; }
                then the name blob: NUL-terminated strings, in index order.

`nameofs` is a byte offset from the start of the *infotable* (i.e. it points into
the blob that follows the entries), and 0 means "this lump has no name" -- only
244 of the 2332 lumps are named.  A zero-length lump stores filepos 0 rather than
the running write position; the markers that bracket the wall/flat/sprite ranges
are all zero-length, so this matters for a bit-exact rebuild.

`compress` exists in the struct but is 0 for every lump in the shipped archive,
and D_disk.c has no decompressor.  We preserve the field and refuse to write a
nonzero one.

Everything is little-endian.  numlumps is *signed*, so the format tops out at
32767 lumps.
"""

import struct
from dataclasses import dataclass, field

HEADER = struct.Struct("<hii")      # numlumps, infotableofs, infotablesize
ENTRY = struct.Struct("<iIhh")      # filepos, size, nameofs, compress
HEADER_SIZE = HEADER.size           # 10, and the offset the first lump lands at
MAX_LUMPS = 0x7FFF


class BloError(Exception):
    pass


@dataclass
class Lump:
    index: int
    name: str | None
    data: bytes
    compress: int = 0

    def __len__(self):
        return len(self.data)


@dataclass
class Blo:
    """A whole archive in memory.  19 MB for GREED.BLO, ~220 MB for the HD pack."""

    lumps: list[Lump] = field(default_factory=list)
    _by_name: dict[str, int] = field(default_factory=dict, repr=False)

    # ---------------------------------------------------------------- reading

    @classmethod
    def read(cls, path):
        with open(path, "rb") as f:
            raw = f.read()
        return cls.frombytes(raw, path)

    @classmethod
    def frombytes(cls, raw, path="<bytes>"):
        if len(raw) < HEADER_SIZE:
            raise BloError(f"{path}: too short to be a BLO ({len(raw)} bytes)")

        numlumps, tofs, tsize = HEADER.unpack_from(raw, 0)
        if numlumps < 0:
            raise BloError(f"{path}: negative numlumps {numlumps}")
        if tofs < 0 or tofs + tsize > len(raw):
            raise BloError(
                f"{path}: infotable [{tofs},{tofs + tsize}) outside file of {len(raw)}"
            )
        if tsize < numlumps * ENTRY.size:
            raise BloError(
                f"{path}: infotablesize {tsize} smaller than {numlumps} entries"
            )

        table = raw[tofs : tofs + tsize]
        self = cls()
        for i in range(numlumps):
            filepos, size, nameofs, compress = ENTRY.unpack_from(table, i * ENTRY.size)
            if compress:
                raise BloError(f"{path}: lump {i} is compressed ({compress}); "
                               "no decompressor exists in D_disk.c")
            if size:
                if filepos < 0 or filepos + size > len(raw):
                    raise BloError(
                        f"{path}: lump {i} data [{filepos},{filepos + size}) "
                        f"outside file of {len(raw)}"
                    )
                data = raw[filepos : filepos + size]
            else:
                data = b""
            self.lumps.append(Lump(i, _read_name(table, nameofs, path, i), data))

        # First occurrence wins, matching CA_CheckNamedNum's forward scan
        # (D_disk.c:112-118).  'level4.lay' and 'level1.sux' really are duplicated.
        for lump in self.lumps:
            if lump.name is not None:
                self._by_name.setdefault(lump.name.lower(), lump.index)
        return self

    # ---------------------------------------------------------------- lookup

    def __len__(self):
        return len(self.lumps)

    def __getitem__(self, i):
        return self.lumps[i]

    def find(self, name):
        """Lump number for `name`, or -1.  Case-insensitive, like CA_CheckNamedNum."""
        return self._by_name.get(name.lower(), -1)

    def num(self, name):
        """Lump number for `name`, or raise.  Like CA_GetNamedNum."""
        n = self.find(name)
        if n < 0:
            raise BloError(f"no such lump: {name}")
        return n

    def range(self, start_marker, end_marker):
        """The half-open lump range a pair of markers brackets.

        R_public.c:120-126 does exactly this to find the walls, flats and
        sprites: the marker itself is excluded, so the first real lump is
        start+1.  It computes counts as end-start, which counts the marker --
        that off-by-one is why numwalls is 263 for 262 real walls.  We return
        the honest range.
        """
        return range(self.num(start_marker) + 1, self.num(end_marker))

    # ---------------------------------------------------------------- writing

    def tobytes(self):
        n = len(self.lumps)
        if n > MAX_LUMPS:
            raise BloError(f"{n} lumps exceeds the int16 numlumps limit of {MAX_LUMPS}")

        chunks = []
        entries = bytearray()
        names = bytearray()
        pos = HEADER_SIZE

        for i, lump in enumerate(self.lumps):
            if lump.compress:
                raise BloError(f"lump {i}: refusing to write compress={lump.compress}")
            size = len(lump.data)
            if lump.name is None:
                nameofs = 0
            else:
                # Offsets are from the start of the infotable, so they are only
                # final once we know how many entries precede the blob.
                nameofs = len(names)
                names += lump.name.encode("latin-1") + b"\0"
            # Empty lumps store filepos 0, not the running position.
            entries += ENTRY.pack(pos if size else 0, size, nameofs, 0)
            if size:
                chunks.append(lump.data)
                pos += size

        entry_bytes = n * ENTRY.size
        if names:
            # Rewrite each nonzero nameofs now that the blob's base is known.
            for i, lump in enumerate(self.lumps):
                if lump.name is None:
                    continue
                off = i * ENTRY.size + 8
                (rel,) = struct.unpack_from("<h", entries, off)
                absolute = rel + entry_bytes
                if not 0 < absolute <= 0x7FFF:
                    raise BloError(
                        f"lump {i}: name offset {absolute} overflows int16 nameofs; "
                        "too many named lumps"
                    )
                struct.pack_into("<h", entries, off, absolute)

        table = bytes(entries) + bytes(names)
        return b"".join([HEADER.pack(n, pos, len(table)), *chunks, table])

    def write(self, path):
        data = self.tobytes()
        with open(path, "wb") as f:
            f.write(data)
        return len(data)


def _read_name(table, nameofs, path, index):
    if nameofs == 0:
        return None
    if not 0 < nameofs < len(table):
        raise BloError(f"{path}: lump {index} nameofs {nameofs} outside infotable")
    end = table.find(b"\0", nameofs)
    if end < 0:
        raise BloError(f"{path}: lump {index} name is not NUL-terminated")
    return table[nameofs:end].decode("latin-1")
