"""Prove the codecs by decoding and re-encoding every graphic lump in GREED.BLO.

A bit-exact repack is the strongest available test: it exercises every decoder
against real data and catches any layout assumption that is merely plausible.
Run this before trusting anything else in this directory.

    python roundtrip.py [path/to/GREED.BLO]
"""

import sys
from pathlib import Path

import blo
import formats
from assets import classify

DEFAULT = Path(__file__).resolve().parents[2] / "greed_final" / "GREED.BLO"


def main(path):
    archive = blo.Blo.read(path)
    groups = classify(archive)

    total = bad = skipped = 0
    failures = []

    for cls, indices in groups.items():
        n = ok = 0
        for i in indices:
            lump = archive[i]
            if not lump.data:
                skipped += 1
                continue
            n += 1
            total += 1
            try:
                if cls == "sprite":
                    arr, mask, meta = formats.decode_sprite(lump.data)
                    out = formats.encode_sprite(arr, mask, meta)
                elif cls == "font":
                    glyphs, meta = formats.decode_font(lump.data)
                    out = formats.encode_font(glyphs, meta)
                else:
                    decode = getattr(formats, f"decode_{cls}")
                    encode = getattr(formats, f"encode_{cls}")
                    arr, meta = decode(lump.data)
                    out = encode(arr, meta)
            except Exception as exc:                      # noqa: BLE001
                failures.append((cls, i, lump.name, f"{type(exc).__name__}: {exc}"))
                bad += 1
                continue
            if out != lump.data:
                where = next((k for k in range(min(len(out), len(lump.data)))
                              if out[k] != lump.data[k]), min(len(out), len(lump.data)))
                failures.append((cls, i, lump.name,
                                 f"differs at byte {where} "
                                 f"({len(lump.data)} -> {len(out)} bytes)"))
                bad += 1
            else:
                ok += 1
        print(f"  {cls:8s} {ok:4d}/{n:<4d} exact")

    print(f"\n{total - bad}/{total} lumps re-encoded byte-for-byte "
          f"({skipped} empty markers skipped)")
    for cls, i, name, why in failures[:20]:
        print(f"  FAIL {cls} lump {i} ({name or 'unnamed'}): {why}")
    if len(failures) > 20:
        print(f"  ... and {len(failures) - 20} more")

    # The archive-level rebuild, on top of the per-lump one.
    rebuilt = archive.tobytes()
    original = Path(path).read_bytes()
    print(f"\nfull archive rebuild: "
          f"{'IDENTICAL' if rebuilt == original else 'DIFFERS'} "
          f"({len(original)} bytes)")

    return 1 if (bad or rebuilt != original) else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else DEFAULT))
