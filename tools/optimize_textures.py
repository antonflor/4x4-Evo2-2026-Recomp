#!/usr/bin/env python3
"""
evo2026 — shrink an HD texture folder: opaque textures become high-quality JPEG, textures with
transparency stay PNG.

    python optimize_textures.py "<game>\\textures" [--quality 92] [--dry-run]

Opaque textures (no alpha channel, or alpha 255 everywhere) are by far the most of the pack
(terrain, roads, rocks, buildings). As JPEG at quality 92 with full colour resolution (4:4:4)
they take about a fifth of the space and look the same in game. Textures with any transparency
(foliage, fences, HUD, parts of truck skins) are left as PNG, because JPEG has no alpha.

Each JPEG is written next to its PNG, read back to check it decodes at the same size, and only
then is the PNG deleted. Safe to stop and re-run. Needs the evo2026 renderer from v1.0.1 or later
(earlier builds only load .png). Requires: pillow, numpy.
"""
import argparse
import os
import sys

import numpy as np
from PIL import Image


def is_opaque(img):
    if img.mode in ("RGB", "L", "P") and "transparency" not in img.info:
        return True
    if img.mode in ("RGBA", "LA", "P"):
        alpha = np.asarray(img.convert("RGBA"))[..., 3]
        return int(alpha.min()) == 255
    return False


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("folder", help="the game's textures folder")
    ap.add_argument("--quality", type=int, default=92, help="JPEG quality (default 92)")
    ap.add_argument("--dry-run", action="store_true", help="only report what would be saved")
    args = ap.parse_args()

    files = sorted(f for f in os.listdir(args.folder) if f.lower().endswith(".png"))
    if not files:
        sys.exit("no .png files found in " + args.folder)

    before = after = 0
    converted = kept = failed = 0
    for i, name in enumerate(files, 1):
        src = os.path.join(args.folder, name)
        size = os.path.getsize(src)
        before += size
        try:
            with Image.open(src) as img:
                img.load()
                opaque = is_opaque(img)
                if not opaque:
                    kept += 1
                    after += size
                    continue
                dst = src[:-4] + ".jpg"
                if args.dry_run:
                    import io
                    buf = io.BytesIO()
                    img.convert("RGB").save(buf, "JPEG", quality=args.quality, subsampling=0, optimize=True)
                    after += buf.tell()
                    converted += 1
                    continue
                img.convert("RGB").save(dst, "JPEG", quality=args.quality, subsampling=0, optimize=True)
                width, height = img.size
            with Image.open(dst) as check:
                check.load()
                if check.size != (width, height):
                    raise ValueError("JPEG read-back size mismatch")
            os.remove(src)
            after += os.path.getsize(dst)
            converted += 1
        except Exception as e:  # keep the PNG on any problem
            failed += 1
            after += size
            print(f"  kept {name}: {e}")
        if i % 1000 == 0 or i == len(files):
            print(f"[{i}/{len(files)}] {before / 2**20:.0f} MB -> {after / 2**20:.0f} MB", flush=True)

    print(f"done: {converted} -> JPEG, {kept} kept as PNG (transparency), {failed} unchanged after errors")
    print(f"size: {before / 2**30:.2f} GB -> {after / 2**30:.2f} GB")


if __name__ == "__main__":
    main()
