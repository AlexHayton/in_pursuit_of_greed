#!/usr/bin/env python3
"""Regenerate bundle/Greed.icns from the game's own title art.

Not part of the build -- Greed.icns is committed so that building the port
needs nothing beyond CMake and a compiler.  Run this only when the icon
should change:

    python3 bundle/make_icon.py            # needs Pillow and iconutil

Source is greed_cdrom/LOGO.PCX, the 320x200 CD-ROM installer splash.  Two
things matter in the conversion:

  * 320x200 was displayed as 4:3, so the pixels were not square.  The art is
    stretched to 320x240 first, or the wordmark comes out squashed.
  * Upscaling uses NEAREST, not a smooth filter.  The source is 264x128
    pixels of 1995 art; interpolating it up to 1024 just makes it mushy,
    whereas hard pixel edges read as deliberate.
"""

import os
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
LOGO = os.path.join(HERE, "..", "..", "greed_cdrom", "LOGO.PCX")
ICNS = os.path.join(HERE, "Greed.icns")

# The wordmark's bounding box within the aspect-corrected 320x240 image.
CROP = (28, 52, 292, 180)


def main():
    if not os.path.exists(LOGO):
        sys.exit("%s not found -- the CD-ROM tree is required" % LOGO)

    src = Image.open(LOGO).convert("RGB")
    band = src.resize((320, 240), Image.LANCZOS).crop(CROP)

    # Square canvas, wordmark centred.  The surrounding black is the logo's
    # own starfield background, so the padding is invisible.
    master = Image.new("RGB", (1024, 1024), (0, 0, 0))
    master.paste(band.resize((1024, 496), Image.NEAREST), (0, 264))

    iconset = os.path.join(HERE, "Greed.iconset")
    os.makedirs(iconset, exist_ok=True)
    for px in (16, 32, 128, 256, 512):
        for scale, suffix in ((1, ""), (2, "@2x")):
            n = px * scale
            resample = Image.NEAREST if n >= 1024 else Image.LANCZOS
            master.resize((n, n), resample).save(
                os.path.join(iconset, "icon_%dx%d%s.png" % (px, px, suffix)))

    subprocess.check_call(["iconutil", "-c", "icns", iconset, "-o", ICNS])
    for f in os.listdir(iconset):
        os.remove(os.path.join(iconset, f))
    os.rmdir(iconset)
    print("wrote %s" % ICNS)


if __name__ == "__main__":
    main()
