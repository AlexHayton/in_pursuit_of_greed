"""work/hd -> GREED_HD.001.BLO ..., the 4x sidecar archive.

The sidecar is a lump-number-parallel overlay of GREED.BLO: it has the same
entry count, and an entry with size 0 means "this lump is not overridden, read
it from the original".  It has to be addressed by number rather than by name
because the wall, flat and sprite lumps are *unnamed* in GREED.BLO -- only the
244 markers and menu items carry names -- so there is nothing to match on.

Header (packed, 16 bytes), a superset of fileinfo_t so the first three fields
parse identically:

    int16 numlumps
    int32 infotableofs
    int32 infotablesize
    int16 version        1
    int16 texshift       log2 of the wall/flat dimension, 8 for a 4x pack
    int16 spriteshift    extra sprite density exponent, 2 for a 4x pack

then the standard 12-byte lumpinfo_t directory and the lump data, exactly as
D_disk.c already expects.  The engine reads texshift/spriteshift out of this
header rather than being compiled for them, so one binary runs both art sets.

The output is split across several files because the whole pack is 209 MB and
GitHub refuses a blob over 100 MB, warning over 50.  The split needed no new
format: a part is just a partial pack, the same shape `--only wall` already
produced, and CA_OverlayArt merges any number of them.  Every part carries the
*whole* build's header values rather than its own, because hudscale is only set
when both pics and fonts are present and those can land in different parts --
the engine merges by max, so agreeing values are what make the merge right.
"""

import argparse
import json
import struct
from pathlib import Path

import numpy as np
from PIL import Image

import blo
import formats
import hdformats

HERE = Path(__file__).resolve().parent
DEFAULT_WORK = HERE / "work"
DEFAULT_OUT = HERE.parents[1] / "greed_final" / "GREED_HD.BLO"

# numlumps, infotableofs, infotablesize, version, texshift, flatshift,
# spriteshift, hudscale.  The four trailing fields are per class so a partial
# pack -- which is how this gets built and tested, one class at a time --
# describes only what it actually contains.
HD_HEADER = struct.Struct("<hiihhhhh")
HD_VERSION = 1

# GitHub blocks a blob over 100 MB and warns over 50.  45 keeps every part
# under the warning with room for the 28 KB directory each one carries.
DEFAULT_PART_BYTES = 45_000_000


def _load_L(path):
    return np.asarray(Image.open(path).convert("L"))


def build(work=DEFAULT_WORK, out=DEFAULT_OUT, classes=None, scale=4,
          max_part_bytes=DEFAULT_PART_BYTES):
    work = Path(work)
    meta = json.loads((work / "meta.json").read_text("utf-8"))
    numlumps = meta["numlumps"]

    # size 0 / filepos 0 == not overridden
    entries = [None] * numlumps
    blobs = [b""] * numlumps
    counts = {}

    for key, entry in sorted(meta["lumps"].items(), key=lambda kv: int(kv[0])):
        i = int(key)
        cls = entry["class"]
        if classes and cls not in classes:
            continue
        src = work / "hd" / cls / f"{i}.png"
        if not src.exists():
            continue

        if cls == "wall":
            data = hdformats.encode_wall(_load_L(src))
        elif cls == "flat":
            data = hdformats.encode_flat(_load_L(src))
        elif cls == "pic":
            # orgx/orgy stay in 320x200 logical units, NOT scaled: the blit
            # primitives subtract them from the caller's logical coordinate and
            # scale the result, so a pre-scaled origin would be applied twice.
            data = hdformats.encode_pic(
                _load_L(src),
                {"orgx": entry.get("orgx", 0), "orgy": entry.get("orgy", 0)},
            )
        elif cls == "sprite":
            arr = _load_L(src)
            mask = _load_L(src.with_suffix(".mask.png")) >= 128
            data = hdformats.encode_sprite_hd(
                arr, mask, {"leftoffset": entry["leftoffset"] * scale})
        elif cls == "font":
            glyphs, fmeta = _font_from_atlas(_load_L(src), entry, scale)
            data = hdformats.encode_font_hd(glyphs, fmeta)
        else:
            continue

        blobs[i] = data
        counts[cls] = counts.get(cls, 0) + 1

    # Only claim a class is upscaled if lumps of that class are actually here.
    # Computed over the whole build, not per part, and written identically into
    # every part -- see the note at the top about pics and fonts.
    up = scale.bit_length() - 1                      # 4 -> 2
    texshift = 6 + up if counts.get("wall") else 6
    flatshift = 6 + up if counts.get("flat") else 6
    spriteshift = up if counts.get("sprite") else 0
    # Both, not either: hudscale drives the chrome buffer size *and* selects the
    # widened font layout, so a pack with pics but no fonts would make the
    # engine read original fonts through the wide charofs table.
    hudscale = scale if (counts.get("pic") and counts.get("font")) else 1
    shifts = (texshift, flatshift, spriteshift, hudscale)

    out = Path(out)
    for stale in _existing_parts(out):
        stale.unlink()

    groups = _split(blobs, max_part_bytes)
    written = []
    for n, members in enumerate(groups, 1):
        path = out if len(groups) == 1 else _part_path(out, n)
        written.append(_emit(path, numlumps, blobs, members, shifts))

    total = sum(p.stat().st_size for p in written)
    print(f"  {sum(counts.values())} lumps overridden of {numlumps} "
          f"({', '.join(f'{k} {v}' for k, v in sorted(counts.items()))})")
    print(f"  wall {1 << texshift}px, flat {1 << flatshift}px, "
          f"sprite x{1 << spriteshift}, hud x{hudscale}")
    for p in written:
        print(f"  wrote {p.name:22s} {p.stat().st_size / 1e6:7.1f} MB")
    print(f"  {len(written)} part(s), {total / 1e6:.1f} MB total")
    return written


def _part_path(out, n):
    return out.with_name(f"{out.stem}.{n:03d}{out.suffix}")


def _existing_parts(out):
    """Every output this build could be replacing, so a rerun leaves no orphans.

    A pack that splits into five parts and is then rebuilt into four would
    otherwise leave .005 behind, and the engine loads whatever it finds -- the
    stale part would keep overriding lumps with art from the previous run.
    """
    found = [out] if out.exists() else []
    found += sorted(out.parent.glob(f"{out.stem}.[0-9][0-9][0-9]{out.suffix}"))
    return found


def _split(blobs, max_part_bytes):
    """Group lump indices into parts of at most `max_part_bytes` of lump data.

    Greedy in lump order, which keeps each part's contents contiguous and so
    keeps a class together where it fits.  Lumps are never divided: the largest
    is 1.0 MB against a 45 MB budget, so the packing is not tight enough to be
    worth anything cleverer.
    """
    live = [i for i, b in enumerate(blobs) if b]
    if not max_part_bytes:
        return [set(live)]

    groups, cur, acc = [], [], 0
    for i in live:
        n = len(blobs[i])
        if cur and acc + n > max_part_bytes:
            groups.append(set(cur))
            cur, acc = [], 0
        cur.append(i)
        acc += n
    if cur:
        groups.append(set(cur))
    return groups or [set()]


def _emit(path, numlumps, blobs, members, shifts):
    """Write one part: header, lump data, directory.

    Lumps outside `members` get size 0 and filepos 0 -- the format's "not
    overridden" -- so each part is a valid standalone pack over the full lump
    number space, which is what lets the engine merge any subset of them.
    """
    texshift, flatshift, spriteshift, hudscale = shifts
    pos = HD_HEADER.size
    table = bytearray()
    chunks = []
    for i in range(numlumps):
        size = len(blobs[i]) if i in members else 0
        table += blo.ENTRY.pack(pos if size else 0, size, 0, 0)
        if size:
            chunks.append(blobs[i])
            pos += size
    if pos > 0x7FFFFFFF:
        raise SystemExit(f"{path.name} exceeds the 2 GB int32 filepos limit")

    with open(path, "wb") as f:
        f.write(HD_HEADER.pack(numlumps, pos, len(table), HD_VERSION,
                               texshift, flatshift, spriteshift, hudscale))
        for c in chunks:
            f.write(c)
        f.write(bytes(table))
    return path


def _font_from_atlas(atlas, entry, scale):
    """Cut the upscaled atlas back into per-character glyphs."""
    height = entry["height"] * scale
    glyphs, order = {}, []
    for g in entry["glyphs"]:
        x, w = g["x"] * scale, g["w"] * scale
        glyphs[g["char"]] = atlas[:, x : x + w]
        order.append(g["char"])
    if atlas.shape[0] != height:
        raise SystemExit(f"font atlas is {atlas.shape[0]} rows, expected {height}")
    return glyphs, {"height": height, "order": order}


def verify(path, work=DEFAULT_WORK, original=None):
    """Re-read the sidecar and check every lump decodes to the expected shape.

    Merges the parts the same way CA_OverlayArt does -- later parts win, shifts
    merge by max -- so this checks what the engine will actually see rather than
    one file in isolation.
    """
    meta = json.loads((Path(work) / "meta.json").read_text("utf-8"))
    parts = _existing_parts(Path(path))
    if not parts:
        raise SystemExit(f"no pack found at {path} or {path}.NNN")

    numlumps = None
    shifts = [6, 6, 0, 1]
    where = {}                       # lump -> (raw bytes of its part, filepos, size)
    for p in parts:
        raw = p.read_bytes()
        (n, tofs, tsize, ver, tex, flat, spr, hud) = HD_HEADER.unpack_from(raw, 0)
        if ver != HD_VERSION:
            raise SystemExit(f"{p.name}: unexpected version {ver}")
        if numlumps is None:
            numlumps = n
        elif n != numlumps:
            raise SystemExit(f"{p.name}: {n} lumps, expected {numlumps}")
        shifts = [max(a, b) for a, b in zip(shifts, (tex, flat, spr, hud or 1))]
        table = raw[tofs : tofs + tsize]
        held = 0
        for i in range(n):
            filepos, size, _, _ = blo.ENTRY.unpack_from(table, i * blo.ENTRY.size)
            if size:
                where[i] = (raw, filepos, size)
                held += 1
        print(f"  {p.name:22s} {p.stat().st_size / 1e6:7.1f} MB  {held} lumps")

    texshift, flatshift, spriteshift, hudscale = shifts
    print(f"  merged: {numlumps} lumps, {len(where)} overridden, wall "
          f"{1 << texshift}px, flat {1 << flatshift}px, "
          f"sprite x{1 << spriteshift}, hud x{hudscale}")

    ok = bad = 0
    for i in range(numlumps):
        if i not in where:
            continue
        raw, filepos, size = where[i]
        data = raw[filepos : filepos + size]
        entry = meta["lumps"].get(str(i))
        if entry is None:
            print(f"  lump {i}: overridden but absent from meta.json")
            bad += 1
            continue
        cls = entry["class"]
        try:
            if cls == "wall":
                arr, _ = formats.decode_wall(data)
                want = (entry["h"] * 4, entry["w"] * 4)
            elif cls == "flat":
                arr = np.frombuffer(data, np.uint8)
                side = int(round(len(arr) ** 0.5))
                arr = arr.reshape(side, side)
                want = (entry["h"] * 4, entry["w"] * 4)
            elif cls == "pic":
                arr, _ = formats.decode_pic(data)
                want = (entry["h"] * 4, entry["w"] * 4)
            elif cls == "sprite":
                arr, _, smeta = hdformats.decode_sprite_hd(data)
                # Height is NOT stored -- decode derives it as max(top)+1 -- and
                # the 4x encoding legitimately drops the leading transparent
                # pixels the original kept inside each column's span, so the
                # array comes back shorter than 4x.  That is invisible to the
                # renderer, which positions columns by top/bottom against the
                # baseline and never sees the array.  Check what actually
                # matters: the width, the left offset, and that nothing sticks
                # out above the 4x box.
                if arr.shape[1] != entry["w"] * 4:
                    raise ValueError(f"width {arr.shape[1]} != {entry['w'] * 4}")
                if arr.shape[0] > entry["h"] * 4:
                    raise ValueError(f"height {arr.shape[0]} > {entry['h'] * 4}")
                if smeta["leftoffset"] != entry["leftoffset"] * 4:
                    raise ValueError(f"leftoffset {smeta['leftoffset']} "
                                     f"!= {entry['leftoffset'] * 4}")
                ok += 1
                continue
            elif cls == "font":
                glyphs, fmeta = hdformats.decode_font_hd(data)
                if fmeta["height"] != entry["height"] * 4:
                    raise ValueError(f"height {fmeta['height']} "
                                     f"!= {entry['height'] * 4}")
                ok += 1
                continue
            else:
                continue
        except Exception as exc:                       # noqa: BLE001
            print(f"  lump {i} ({cls}): {type(exc).__name__}: {exc}")
            bad += 1
            continue
        if arr.shape != want:
            print(f"  lump {i} ({cls}): shape {arr.shape}, expected {want}")
            bad += 1
        else:
            ok += 1

    print(f"  {ok} lumps verified, {bad} bad")
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--work", default=DEFAULT_WORK, type=Path)
    ap.add_argument("--out", default=DEFAULT_OUT, type=Path)
    ap.add_argument("--only", help="comma-separated classes")
    ap.add_argument("--verify", action="store_true", help="check an existing pack")
    ap.add_argument("--part-mb", type=float, default=DEFAULT_PART_BYTES / 1e6,
                    help="max MB of lump data per part; 0 writes one file")
    a = ap.parse_args()
    if a.verify:
        raise SystemExit(verify(a.out, a.work))
    build(a.work, a.out, set(a.only.split(",")) if a.only else None,
          max_part_bytes=int(a.part_mb * 1e6))


if __name__ == "__main__":
    main()
