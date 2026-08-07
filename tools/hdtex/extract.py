"""GREED.BLO -> work/png/<class>/<lump>.png, plus the metadata the packer needs.

Everything is written as RGB, resolved through whichever palette the lump is
actually drawn under (the game palette, or a screen's own trailing palette
lump).  Sprites additionally get a <lump>.mask.png: the span mask, which is not
recoverable from the pixels because a sprite column stores interior transparent
pixels inside its span.  See formats.decode_sprite.

Fonts are extracted as one atlas per lump plus their glyph table.  Their bytes
are ramp offsets rather than palette indices (FN_RawPrint adds fontbasecolor to
them), so the atlas is written as greyscale and must never be palette-matched.

    python extract.py [--blo PATH] [--out DIR] [--only CLASS[,CLASS...]]
"""

import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

import assets
import blo
import formats
import palette as pal

HERE = Path(__file__).resolve().parent
DEFAULT_BLO = HERE.parents[1] / "greed_final" / "GREED.BLO"
DEFAULT_OUT = HERE / "work"


def save_rgb(path, arr):
    Image.fromarray(np.ascontiguousarray(arr), "RGB").save(path, optimize=True)


def save_gray(path, arr):
    Image.fromarray(np.ascontiguousarray(arr), "L").save(path, optimize=True)


def extract(blo_path=DEFAULT_BLO, out=DEFAULT_OUT, only=None):
    archive = blo.Blo.read(blo_path)
    groups = assets.classify(archive)
    own_pal = assets.screen_palettes(archive)
    game_pal = pal.load(archive)

    out = Path(out)
    (out / "png").mkdir(parents=True, exist_ok=True)
    meta = {
        "source": str(blo_path),
        "numlumps": len(archive),
        "game_palette": game_pal.tolist(),
        "lumps": {},
    }
    # Merge into an existing manifest rather than replacing it: `--only pic`
    # must not drop the wall/flat/sprite entries pack.py still needs.
    existing = out / "meta.json"
    if only and existing.exists():
        try:
            meta["lumps"] = json.loads(existing.read_text("utf-8")).get("lumps", {})
        except (OSError, ValueError):
            pass

    for cls, indices in groups.items():
        if only and cls not in only:
            continue
        d = out / "png" / cls
        d.mkdir(exist_ok=True)
        for i in indices:
            lump = archive[i]
            entry = {"class": cls, "name": lump.name, "size": len(lump.data)}

            if cls == "font":
                glyphs, fmeta = formats.decode_font(lump.data)
                atlas, table = _font_atlas(glyphs, fmeta)
                save_gray(d / f"{i}.png", atlas)
                entry.update(height=fmeta["height"], order=fmeta["order"],
                             glyphs=table, ramp=int(atlas.max()))

            elif cls == "sprite":
                arr, mask, smeta = formats.decode_sprite(lump.data)
                save_rgb(d / f"{i}.png", pal.to_rgb(arr, game_pal))
                save_gray(d / f"{i}.mask.png", (mask * 255).astype(np.uint8))
                entry.update(w=arr.shape[1], h=arr.shape[0],
                             leftoffset=smeta["leftoffset"])

            else:
                if cls == "pic":
                    arr, pmeta = formats.decode_pic(lump.data)
                    p = (pal.expand(archive[own_pal[i]].data)
                         if i in own_pal else game_pal)
                    entry.update(orgx=pmeta["orgx"], orgy=pmeta["orgy"],
                                 tail=len(pmeta["tail"]))
                    if i in own_pal:
                        # Carry the palette itself, not just its lump number:
                        # process.py requantizes against it and would otherwise
                        # have to reopen GREED.BLO to find it.
                        entry["palette_lump"] = own_pal[i]
                        entry["palette_rgb"] = p.tolist()
                elif cls == "wall":
                    arr, wmeta = formats.decode_wall(lump.data)
                    p = game_pal
                    entry["collumnofs"] = wmeta["collumnofs"].hex()
                else:                                    # flat
                    arr, _ = formats.decode_flat(lump.data)
                    p = game_pal
                save_rgb(d / f"{i}.png", pal.to_rgb(arr, p))
                entry.update(w=arr.shape[1], h=arr.shape[0])
                if cls == "pic":
                    # The raw indices too.  Two things need them and the RGB
                    # rendering above has thrown them away: index 0 means
                    # *transparent* in every VI_DrawMaskedPic path, and the
                    # status bar's meter markers have to survive as indices.
                    save_gray(d / f"{i}.idx.png", arr)
                    if (lump.name or "").lower() in pal.SEMANTIC_LUMPS:
                        entry["semantic"] = True
                    entry["masked"] = bool((arr == 0).any())

            meta["lumps"][str(i)] = entry
        print(f"  {cls:8s} {len(indices):4d} lumps -> {d}")

    (out / "meta.json").write_text(json.dumps(meta), encoding="utf-8")
    print(f"\nwrote {len(meta['lumps'])} lumps and meta.json under {out}")
    return meta


def _font_atlas(glyphs, fmeta):
    """Lay the glyphs out left to right in file order, one row tall."""
    order = fmeta["order"]
    height = fmeta["height"]
    widths = [glyphs[c].shape[1] for c in order]
    atlas = np.zeros((height, sum(widths)), np.uint8)
    table, x = [], 0
    for c, w in zip(order, widths):
        atlas[:, x : x + w] = glyphs[c]
        table.append({"char": c, "x": x, "w": w})
        x += w
    return atlas, table


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--blo", default=DEFAULT_BLO, type=Path)
    ap.add_argument("--out", default=DEFAULT_OUT, type=Path)
    ap.add_argument("--only", help="comma-separated: wall,flat,sprite,pic,font")
    a = ap.parse_args()
    extract(a.blo, a.out, set(a.only.split(",")) if a.only else None)


if __name__ == "__main__":
    main()
