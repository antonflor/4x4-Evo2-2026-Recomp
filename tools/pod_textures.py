#!/usr/bin/env python3
"""
evo2026 — export every texture from 4x4 Evo 2's .POD archives as PNG.

    python pod_textures.py "<game folder>" "<output folder>" [--match ZONA]

4x4 Evo stores textures as 8-bit indexed images: NAME.RAW (pixels, square, power of two),
NAME.ACT (256-colour palette) and, for see-through textures, NAME.OPA (8-bit alpha).
4x4 Evo 2 adds 32-bit NAME.TIF / NAME.TGA images (menus, UI, some effects); those are
exported as they are.
This writes NAME.png for each one, using the same lower-case name the evo2026 renderer
uses, so an upscaled copy dropped into the game's "textures" folder replaces that texture
everywhere (all palette variants). Use the renderer's DumpTextures=1 mode instead when you
need per-variant files (e.g. a specific truck colour).

Only reads the archives; never modifies game files. Requires: pillow.
"""
import argparse
import io
import os
import struct
import sys

from PIL import Image


def read_pod(path):
    """Yield (name, bytes) for every file in a POD2 archive."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:4] != b"POD2":
        raise ValueError(f"{path}: not a POD2 archive")
    count = struct.unpack_from("<I", data, 88)[0]
    names_at = 96 + count * 20
    for i in range(count):
        name_off, size, offset, _stamp, _crc = struct.unpack_from("<5I", data, 96 + i * 20)
        end = data.index(b"\0", names_at + name_off)
        name = data[names_at + name_off:end].decode("latin-1")
        yield name, data[offset:offset + size]


def decode_palette_alpha_tif(data):
    """4x4 Evo 2 truck/driver skins are TIFFs Pillow can't read: palette colour (photometric 3)
    with 2 samples per pixel (index, alpha), uncompressed. Decode those by hand."""
    order = "<" if data[:2] == b"II" else ">"
    ifd = struct.unpack_from(order + "I", data, 4)[0]
    tags = {}
    for i in range(struct.unpack_from(order + "H", data, ifd)[0]):
        tag, typ, count, value = struct.unpack_from(order + "HHII", data, ifd + 2 + i * 12)
        if typ == 3 and count == 1:
            value = struct.unpack_from(order + "H", data, ifd + 2 + i * 12 + 8)[0]
        tags[tag] = (typ, count, value)
    width, height = tags[256][2], tags[257][2]
    if tags.get(262, (0, 0, 0))[2] != 3 or tags.get(277, (0, 0, 1))[2] != 2 or tags.get(259, (0, 0, 1))[2] != 1:
        raise ValueError("unsupported TIFF layout")
    typ, count, value = tags[273]
    offsets = [value] if count == 1 else list(struct.unpack_from(order + "%dI" % count, data, value))
    pixels = b"".join(data[o:o + width * height * 2] for o in offsets)[:width * height * 2]
    cmap = struct.unpack_from(order + "768H", data, tags[320][2])
    palette = bytes(v >> 8 for i in range(256) for v in (cmap[i], cmap[256 + i], cmap[512 + i]))
    img = Image.frombytes("P", (width, height), pixels[0::2])
    img.putpalette(palette)
    img = img.convert("RGBA")
    img.putalpha(Image.frombytes("L", (width, height), pixels[1::2]))
    return img


def load_rgba(blob):
    try:
        return Image.open(io.BytesIO(blob)).convert("RGBA")
    except Exception:
        return decode_palette_alpha_tif(blob)


def stem(name):
    """Same naming rule as the renderer (EvoTextures.cxx Sanitize)."""
    base = name.replace("/", "\\").split("\\")[-1].rsplit(".", 1)[0]
    return "".join(c.lower() if (c.isalnum() or c in "-_.") else "_" for c in base) or "unnamed"


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("game", help="4x4 Evo 2 install folder (contains *.POD)")
    ap.add_argument("output", help="where to write PNGs")
    ap.add_argument("--match", default="", help="only archives whose file name contains this (e.g. ZONA)")
    args = ap.parse_args()

    pods = sorted(f for f in os.listdir(args.game) if f.lower().endswith(".pod") and args.match.lower() in f.lower())
    if not pods:
        sys.exit("no .POD files found")
    os.makedirs(args.output, exist_ok=True)

    written = 0
    for pod in pods:
        files = {}
        for name, blob in read_pod(os.path.join(args.game, pod)):
            ext = name.rsplit(".", 1)[-1].lower()
            if ext in ("raw", "act", "opa", "tif", "tga"):
                files.setdefault(stem(name), {})[ext] = blob

        count = 0
        for key, parts in sorted(files.items()):
            rgba = parts.get("tif") or parts.get("tga")
            if rgba is not None and parts.get("raw") is None:
                out = os.path.join(args.output, key + ".png")
                if not os.path.exists(out):
                    try:
                        load_rgba(rgba).save(out, optimize=True)
                        count += 1
                    except Exception as e:  # pragma: no cover - odd/corrupt image
                        print(f"  skip {key}: {e}")
                continue
            raw, act = parts.get("raw"), parts.get("act")
            if raw is None or act is None or len(act) < 768:
                continue
            side = int(round(len(raw) ** 0.5))
            if side * side != len(raw) or side & (side - 1):
                continue  # not a square power-of-two texture (e.g. a 640x480 screen)
            img = Image.frombytes("P", (side, side), raw)
            img.putpalette(act[:768])
            img = img.convert("RGBA")
            opa = parts.get("opa")
            if opa is not None and len(opa) == len(raw):
                img.putalpha(Image.frombytes("L", (side, side), opa))
            out = os.path.join(args.output, key + ".png")
            if not os.path.exists(out):
                img.save(out, optimize=True)
                count += 1
        written += count
        print(f"{pod}: {count} textures")

    print(f"done: {written} PNGs in {args.output}")


if __name__ == "__main__":
    main()
