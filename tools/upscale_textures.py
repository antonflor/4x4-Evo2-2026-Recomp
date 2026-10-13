#!/usr/bin/env python3
"""
evo2026 HD texture pipeline — AI-upscale the textures dumped by the evo2026 renderer.

    1. In system\\render.ini set  DumpTextures=1  under [evo2026], play the tracks/trucks you
       want, then set it back to 0. PNGs land in .\\textures_dump\\ (one per texture variant).
    2. python upscale_textures.py "<game>\\textures_dump" "<game>\\textures"
    3. Launch the game: the renderer loads .\\textures\\*.png automatically.

Requires: Python 3.9+, torch (CUDA build for NVIDIA GPUs), pillow, numpy.
    pip install torch --index-url https://download.pytorch.org/whl/cu128
    pip install pillow numpy

Models (Real-ESRGAN, BSD-3-Clause, by Xintao Wang et al.) are downloaded on first use:
    general  realesr-general-x4v3.pth   fast, natural textures (default)
    x4plus   RealESRGAN_x4plus.pth      heavier, sharper, can look "painted"

What it does for each texture:
  * Tileable textures (terrain tiles) are wrap-padded before upscaling so the result still
    tiles without seams; non-tiling atlases (truck skins, UI) are edge-padded instead.
  * Colour goes through the AI model; alpha is resized separately (cut-out masks such as
    bushes/fences stay crisp instead of going soft).
  * Output keeps the exact dump filename (<name>__<crc>.png), so it replaces only that
    variant. Rename to <name>.png to make one image apply to every palette variant.
"""
import argparse
import math
import os
import sys
import time
import urllib.request

import numpy as np
from PIL import Image

try:
    import torch
    import torch.nn as nn
    import torch.nn.functional as F
except ImportError:  # pragma: no cover
    sys.exit("PyTorch is required: pip install torch (see the header of this script)")

MODELS = {
    "general": ("realesr-general-x4v3.pth",
                "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.2.5.0/realesr-general-x4v3.pth"),
    "x4plus": ("RealESRGAN_x4plus.pth",
               "https://github.com/xinntao/Real-ESRGAN/releases/download/v0.1.0/RealESRGAN_x4plus.pth"),
}


# ----------------------------------------------------------------------------------------
# Network definitions (re-implemented from Real-ESRGAN / BasicSR so no extra packages
# are needed; parameter names match the published checkpoints).
# ----------------------------------------------------------------------------------------
class SRVGGNetCompact(nn.Module):
    def __init__(self, num_in_ch=3, num_out_ch=3, num_feat=64, num_conv=32, upscale=4):
        super().__init__()
        self.upscale = upscale
        self.body = nn.ModuleList()
        self.body.append(nn.Conv2d(num_in_ch, num_feat, 3, 1, 1))
        self.body.append(nn.PReLU(num_parameters=num_feat))
        for _ in range(num_conv):
            self.body.append(nn.Conv2d(num_feat, num_feat, 3, 1, 1))
            self.body.append(nn.PReLU(num_parameters=num_feat))
        self.body.append(nn.Conv2d(num_feat, num_out_ch * upscale * upscale, 3, 1, 1))
        self.upsampler = nn.PixelShuffle(upscale)

    def forward(self, x):
        out = x
        for layer in self.body:
            out = layer(out)
        out = self.upsampler(out)
        return out + F.interpolate(x, scale_factor=self.upscale, mode="nearest")


class ResidualDenseBlock(nn.Module):
    def __init__(self, nf=64, gc=32):
        super().__init__()
        self.conv1 = nn.Conv2d(nf, gc, 3, 1, 1)
        self.conv2 = nn.Conv2d(nf + gc, gc, 3, 1, 1)
        self.conv3 = nn.Conv2d(nf + 2 * gc, gc, 3, 1, 1)
        self.conv4 = nn.Conv2d(nf + 3 * gc, gc, 3, 1, 1)
        self.conv5 = nn.Conv2d(nf + 4 * gc, nf, 3, 1, 1)
        self.lrelu = nn.LeakyReLU(0.2, True)

    def forward(self, x):
        x1 = self.lrelu(self.conv1(x))
        x2 = self.lrelu(self.conv2(torch.cat((x, x1), 1)))
        x3 = self.lrelu(self.conv3(torch.cat((x, x1, x2), 1)))
        x4 = self.lrelu(self.conv4(torch.cat((x, x1, x2, x3), 1)))
        x5 = self.conv5(torch.cat((x, x1, x2, x3, x4), 1))
        return x5 * 0.2 + x


class RRDB(nn.Module):
    def __init__(self, nf, gc=32):
        super().__init__()
        self.rdb1 = ResidualDenseBlock(nf, gc)
        self.rdb2 = ResidualDenseBlock(nf, gc)
        self.rdb3 = ResidualDenseBlock(nf, gc)

    def forward(self, x):
        return self.rdb3(self.rdb2(self.rdb1(x))) * 0.2 + x


class RRDBNet(nn.Module):
    def __init__(self, num_in_ch=3, num_out_ch=3, nf=64, nb=23, gc=32):
        super().__init__()
        self.conv_first = nn.Conv2d(num_in_ch, nf, 3, 1, 1)
        self.body = nn.Sequential(*[RRDB(nf, gc) for _ in range(nb)])
        self.conv_body = nn.Conv2d(nf, nf, 3, 1, 1)
        self.conv_up1 = nn.Conv2d(nf, nf, 3, 1, 1)
        self.conv_up2 = nn.Conv2d(nf, nf, 3, 1, 1)
        self.conv_hr = nn.Conv2d(nf, nf, 3, 1, 1)
        self.conv_last = nn.Conv2d(nf, num_out_ch, 3, 1, 1)
        self.lrelu = nn.LeakyReLU(0.2, True)

    def forward(self, x):
        feat = self.conv_first(x)
        feat = feat + self.conv_body(self.body(feat))
        feat = self.lrelu(self.conv_up1(F.interpolate(feat, scale_factor=2, mode="nearest")))
        feat = self.lrelu(self.conv_up2(F.interpolate(feat, scale_factor=2, mode="nearest")))
        return self.conv_last(self.lrelu(self.conv_hr(feat)))


def load_model(name, model_dir, device, half):
    filename, url = MODELS[name]
    path = os.path.join(model_dir, filename)
    if not os.path.exists(path):
        os.makedirs(model_dir, exist_ok=True)
        print(f"downloading {filename} ...")
        urllib.request.urlretrieve(url, path)
    state = torch.load(path, map_location="cpu", weights_only=True)
    state = state.get("params_ema", state.get("params", state))
    net = SRVGGNetCompact() if name == "general" else RRDBNet()
    net.load_state_dict(state, strict=True)
    net.eval().to(device)
    if half:
        net.half()
    return net


# ----------------------------------------------------------------------------------------
def is_tileable(rgb):
    """True if the image's opposite edges continue into each other (terrain tiles)."""
    a = rgb.astype(np.float32)
    inner_x = np.abs(a[:, 1:] - a[:, :-1]).mean()
    inner_y = np.abs(a[1:] - a[:-1]).mean()
    wrap_x = np.abs(a[:, 0] - a[:, -1]).mean()
    wrap_y = np.abs(a[0] - a[-1]).mean()
    return wrap_x <= 2.0 * inner_x + 2 and wrap_y <= 2.0 * inner_y + 2


@torch.no_grad()
def upscale_rgb(net, rgb, device, half, tile_wrap, pad):
    h, w, _ = rgb.shape
    mode = "wrap" if tile_wrap else "edge"
    padded = np.pad(rgb, ((pad, pad), (pad, pad), (0, 0)), mode=mode)
    t = torch.from_numpy(padded).permute(2, 0, 1).unsqueeze(0).float().div(255.0).to(device)
    if half:
        t = t.half()
    out = net(t).float().clamp(0, 1)
    scale = out.shape[-1] // padded.shape[1]
    out = out[..., pad * scale:(pad + h) * scale, pad * scale:(pad + w) * scale]
    return (out.squeeze(0).permute(1, 2, 0).cpu().numpy() * 255.0 + 0.5).astype(np.uint8), scale


def upscale_alpha(alpha, size, tile_wrap):
    """Resize alpha separately; binary cut-out masks are kept crisp."""
    unique = np.unique(alpha)
    img = Image.fromarray(alpha, "L")
    up = np.asarray(img.resize(size, Image.BICUBIC))
    if len(unique) <= 2:  # cut-out mask: smooth the edge, then re-binarise
        lo, hi = (int(unique[0]), int(unique[-1])) if len(unique) == 2 else (0, int(unique[0]))
        up = np.where(up >= (lo + hi) / 2, hi, lo).astype(np.uint8)
    return up


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("input", help="folder of dumped textures (textures_dump)")
    ap.add_argument("output", help="folder to write replacements to (the game's textures folder)")
    ap.add_argument("--model", choices=sorted(MODELS), default="general")
    ap.add_argument("--model-dir", default=os.path.join(os.path.dirname(os.path.abspath(__file__)), "models"))
    ap.add_argument("--min-size", type=int, default=16, help="skip textures smaller than this")
    ap.add_argument("--max-out", type=int, default=2048, help="cap on output size (pixels per side)")
    ap.add_argument("--passes", type=int, default=1, help="run the 4x model this many times (2 = 16x)")
    ap.add_argument("--match", default="", help="only process files whose name contains this text")
    ap.add_argument("--overwrite", action="store_true", help="redo textures that already exist in output")
    ap.add_argument("--cpu", action="store_true", help="force CPU even if a GPU is available")
    args = ap.parse_args()

    device = torch.device("cuda" if torch.cuda.is_available() and not args.cpu else "cpu")
    half = device.type == "cuda"
    print(f"device: {device}{' (fp16)' if half else ''}, model: {args.model}")
    net = load_model(args.model, args.model_dir, device, half)

    files = sorted(f for f in os.listdir(args.input) if f.lower().endswith(".png") and args.match.lower() in f.lower())
    os.makedirs(args.output, exist_ok=True)

    done = skipped = 0
    started = time.time()
    for i, name in enumerate(files, 1):
        dst = os.path.join(args.output, name)
        if os.path.exists(dst) and not args.overwrite:
            skipped += 1
            continue
        img = Image.open(os.path.join(args.input, name)).convert("RGBA")
        if min(img.size) < args.min_size:
            skipped += 1
            continue

        rgba = np.asarray(img)
        rgb, alpha = rgba[..., :3], rgba[..., 3]
        tile_wrap = is_tileable(rgb)

        out = rgb
        for _ in range(max(1, args.passes)):
            if out.shape[0] * 4 > args.max_out:
                break
            pad = max(4, out.shape[0] // 8)
            out, _ = upscale_rgb(net, out, device, half, tile_wrap, pad)

        size = (out.shape[1], out.shape[0])
        if alpha.min() < 255:
            a = upscale_alpha(alpha, size, tile_wrap)
            Image.fromarray(np.dstack([out, a]), "RGBA").save(dst, optimize=True)
        else:
            Image.fromarray(out, "RGB").save(dst, optimize=True)

        done += 1
        if done % 25 == 0 or i == len(files):
            rate = done / max(1e-6, time.time() - started)
            print(f"[{i}/{len(files)}] {name} -> {size[0]}x{size[1]}  ({rate:.1f} textures/s)")

    print(f"finished: {done} upscaled, {skipped} skipped, {time.time() - started:.0f}s")


if __name__ == "__main__":
    main()
