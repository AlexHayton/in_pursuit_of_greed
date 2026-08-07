"""Build the 4x texture pack: extract -> process -> pack.

    python make_hd.py --all                      everything, all four classes
    python make_hd.py --only flat                iterate on one class
    python make_hd.py --only wall --model x4plus_anime
    python make_hd.py --stage process --resume   redo the upscale, keep extracts
    python make_hd.py --all --limit 4            smoke test, 4 lumps per class

Stages are separable on purpose: extraction is deterministic and cheap, the
upscale is the slow part worth caching, and packing is seconds.  --resume skips
anything already produced, so swapping the model for one class does not redo
the other three.
"""

import argparse
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
CLASSES = ("wall", "flat", "sprite", "pic", "font")
STAGES = ("extract", "process", "pack")


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--blo", type=Path,
                    default=HERE.parents[1] / "greed_final" / "GREED.BLO")
    ap.add_argument("--out", type=Path,
                    default=HERE.parents[1] / "greed_final" / "GREED_HD.BLO")
    ap.add_argument("--work", type=Path, default=HERE / "work")
    ap.add_argument("--part-mb", type=float, default=None,
                    help="max MB of lump data per pack part; 0 writes one file")
    ap.add_argument("--all", action="store_true", help="every class")
    ap.add_argument("--only", help=f"comma-separated: {', '.join(CLASSES)}")
    ap.add_argument("--stage", help=f"comma-separated: {', '.join(STAGES)}")
    ap.add_argument("--model", help="override the model for every class")
    ap.add_argument("--device", default="auto")
    ap.add_argument("--resume", action="store_true")
    ap.add_argument("--limit", type=int)
    ap.add_argument("--report", action="store_true")
    ap.add_argument("--sharp-threshold", type=float,
                    help="Oklab edge threshold for EPX-sharpening pics; 0 disables")
    ap.add_argument("--no-preview", action="store_true")
    a = ap.parse_args()

    if not a.all and not a.only:
        ap.error("pass --all or --only CLASS")
    classes = set(a.only.split(",")) if a.only else set(CLASSES)
    unknown = classes - set(CLASSES)
    if unknown:
        ap.error(f"unknown class(es): {', '.join(sorted(unknown))}")
    stages = a.stage.split(",") if a.stage else list(STAGES)

    if "extract" in stages:
        import extract
        print("== extract")
        t = time.time()
        extract.extract(a.blo, a.work, classes)
        print(f"   {time.time() - t:.1f}s\n")

    if "process" in stages:
        import process
        print("== upscale + quantize")
        t = time.time()
        # Keyed on DEFAULT_MODELS, not CLASSES: fonts have no model entry and
        # adding one would load weights that _do_font never calls.
        models = ({c: a.model for c in process.DEFAULT_MODELS} if a.model
                  else None)
        kw = ({} if a.sharp_threshold is None
              else {"sharp_threshold": a.sharp_threshold})
        p = process.Processor(a.work, models, a.device,
                              preview=not a.no_preview, **kw)
        p.run(classes, resume=a.resume, limit=a.limit, report=a.report)
        print(f"   {time.time() - t:.1f}s\n")

    if "pack" in stages:
        import pack
        print("== pack")
        t = time.time()
        pack.build(a.work, a.out, classes,
                   max_part_bytes=(pack.DEFAULT_PART_BYTES if a.part_mb is None
                                   else int(a.part_mb * 1e6)))
        print(f"   {time.time() - t:.1f}s\n")
        pack.verify(a.out, a.work)

    return 0


if __name__ == "__main__":
    sys.exit(main())
