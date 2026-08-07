"""Download and verify the Real-ESRGAN weights.

Two models, because the art is not all the same kind of image:

  x4plus       the general photo/art model.  Good on the walls and flats, whose
               source is painted/rendered texture with real gradients.
  x4plus_anime the anime6B model.  Trained on flat-shaded line art, which is
               what the sprites and menu art actually are; the general model
               invents film grain on them.

Which one wins per class is a judgement call to make against contact sheets, so
both are fetched and either can be selected with make_hd.py --model.
"""

import hashlib
import sys
import urllib.request
from pathlib import Path

WEIGHTS = Path(__file__).resolve().parent / "weights"

MODELS = {
    "x4plus": {
        "url": "https://github.com/xinntao/Real-ESRGAN/releases/download/"
               "v0.1.0/RealESRGAN_x4plus.pth",
        "file": "RealESRGAN_x4plus.pth",
        "sha256": "4fa0d38905f75ac06eb49a7951b426670021be3018265fd191d2125df9d682f1",
        "blocks": 23,
        "features": 64,
    },
    "x4plus_anime": {
        "url": "https://github.com/xinntao/Real-ESRGAN/releases/download/"
               "v0.2.2.4/RealESRGAN_x4plus_anime_6B.pth",
        "file": "RealESRGAN_x4plus_anime_6B.pth",
        "sha256": "f872d837d3c90ed2e05227bed711af5671a6fd1c9f7d7e91c911a61f155e99da",
        "blocks": 6,
        "features": 64,
    },
}


def _sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def path_for(name):
    """Local path of a model's weights, downloading if needed."""
    if name not in MODELS:
        raise KeyError(f"unknown model {name!r}; have {', '.join(MODELS)}")
    spec = MODELS[name]
    dest = WEIGHTS / spec["file"]

    if dest.exists():
        got = _sha256(dest)
        if got == spec["sha256"]:
            return dest
        print(f"  {spec['file']}: checksum mismatch, re-downloading", file=sys.stderr)
        dest.unlink()

    WEIGHTS.mkdir(exist_ok=True)
    print(f"  downloading {spec['file']} ...")
    tmp = dest.with_suffix(".part")
    with urllib.request.urlopen(spec["url"]) as r, open(tmp, "wb") as f:
        total = int(r.headers.get("content-length", 0))
        done = 0
        while chunk := r.read(1 << 20):
            f.write(chunk)
            done += len(chunk)
            if total:
                print(f"\r    {done * 100 // total:3d}%  "
                      f"{done >> 20}/{total >> 20} MB", end="", flush=True)
    print()

    got = _sha256(tmp)
    if got != spec["sha256"]:
        tmp.unlink()
        raise RuntimeError(
            f"{spec['file']}: sha256 {got}, expected {spec['sha256']}"
        )
    tmp.rename(dest)
    return dest


def main():
    for name in MODELS:
        p = path_for(name)
        print(f"  {name:14s} {p.name}  {p.stat().st_size >> 20} MB  ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
