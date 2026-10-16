# evo2026: 4x4 Evo 2 at native HD on modern Windows

evo2026 is a drop-in Direct3D 9 renderer for **4x4 Evo 2 (Build 139)**. You don't need to modify the game's exe.

The game keeps running at its own 640×480 "logical" resolution, so menus and HUD stay laid out exactly as designed. evo2026 then draws everything at your monitor's native resolution (1080p, 1440p, 4K). 3D geometry is rasterized at full detail rather than upscaled.

It also adds:

- true widescreen (Hor+): a wider view at the sides on 16:9/21:9 screens, nothing stretched
- the race HUD moved out to the screen edges in widescreen
- borderless fullscreen
- vsync
- 16× anisotropic filtering
- an HD texture-replacement system (PNG or JPEG) with an AI upscaling pipeline, and a full HD pack for every track, truck and menu

Built on the open-source [4x4e](https://github.com/americusmaximus/4x4e) reimplementation's Direct3D 9 renderer (MIT), by way of [4x4 Evolution: 2026 Edition](https://github.com/antonflor/4x4-Evo-2026-Recomp).

## Install

The easiest way is the all-in-one installer from the release page. It sets up the game, evo2026 and the HD textures in one go. The steps below are for an existing install.

1. **Install the game** from your disc. Install to a folder **outside Program Files**, e.g. `C:\Games\4x4 Evo 2`.
2. **Put the `textures-hd-*.zip` files** from the release (optional, HD pack) next to `install.ps1`.
3. **Run the installer** from this folder in PowerShell:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\install.ps1 -GamePath "C:\Games\4x4 Evo 2"
   ```
   This installs the renderer, sets up `system\METAL.INI`, writes `system\evo2026.ini` with the defaults below and unpacks the texture zips.
4. **Launch `4x42.exe`.** The game shows its "4x4 Setup Options" screen on every start (that's the original design). Renderer should read **Direct3D8**. Press Enter on **Run 4x4**.

To go back to stock, run `uninstall.ps1 -GamePath "..."`.

### Why the DLL is called tridx8.dll

The exe only accepts renderers by fixed file names. It compares `rendererDLLPath` in `system\METAL.INI` against `trigl.dll`, `tridx8.dll` and `tridx8tl.dll`. evo2026 implements the Direct3D 8 (`tridx8.dll`) slot, so that's the name it's installed under. The installer keeps your original as `Tridx8.original.dll`.

## Settings

The settings live in `system\evo2026.ini`, section `[evo2026]`. (The game rewrites `render.ini` on exit and drops anything it doesn't know, so evo2026 keeps its own file.)

| Key | Default | Meaning |
|---|---|---|
| `Width` / `Height` | `0` | Output resolution. `0` = monitor's native resolution |
| `Window` | `2` | `0` exclusive fullscreen · `1` window · `2` borderless fullscreen |
| `Widescreen` | `1` | `1` true widescreen: wider field of view, nothing stretched · `0` classic 4:3 |
| `HudToEdges` | `1` | In widescreen, move the race HUD (lap times, map, gauges) to the screen edges · `0` keep it in the 4:3 area |
| `Aspect` | `1` | With `Widescreen=0`: `1` keep 4:3 with side bars · `0` stretch to fill |
| `VSync` | `1` | Sync to the monitor's refresh rate |
| `Anisotropy` | `16` | Anisotropic filtering level (`0` = off) |
| `Filter2D` | `1` | `1` smooth scaling of 2D framebuffer art · `0` sharp pixels |
| `ReplaceTextures` | `1` | Load HD textures from `textures\` |
| `DumpTextures` | `0` | Save every texture the game uploads to `textures_dump\` |
| `Trace` | `0` | Write a renderer call log to `system\evo2026.log` |

Game-side settings are in `system\METAL.INI`. Keep `gamePIXX=640`, `gamePIXY=480` and `gameBPP=32` under `[Graphics]`. `useRedBookAudio=0` under `[Game]` plays the music from the game files ("Wave" in the options), which you need when there's no disc in the drive.

**Don't** set `gamePIXY` to 1080. The game's font code only accepts 4:3 heights and stops with "Invalid screen resolution". The logical-resolution design is how evo2026 gets around that.

## Widescreen

The game projects its 3D world on the CPU for a 640×480 screen. evo2026 patches that in memory when the game starts (nothing on disk changes):

- When the game sets its full-screen view, evo2026 widens the view rectangle left and right of the 4:3 image and narrows the camera's horizontal zoom by the same factor. You see more scenery at the sides; the vertical view and the proportions stay exactly as before ("Hor+").
- The 2D layer stays 640×480. In races the HUD pieces in the left or right third (lap times, place, map, gauges) are moved to the matching screen edge; centred pieces (the checkpoint arrow, messages) stay centred.
- Menu backgrounds are stretched to fill the screen; the menu text keeps its 4:3 layout.

The patch only applies to the exact Build 139 exe. Any other build fails a byte check and simply runs in classic 4:3.

## HD textures

The game identifies textures by name. evo2026 matches replacements by name plus a CRC of the pixels, so the same texture with a different palette can be replaced separately:

- `textures\<name>__<crc>.png` (or `.jpg`) replaces only that exact variant.
- `textures\<name>.png` (or `.jpg`) replaces every variant of that texture.

PNG wins if both exist. JPEGs load opaque, so textures with transparency (foliage, fences, HUD, the minimap ring) must stay PNG. Replacements can be any size (4× the original is a good target). Mip-maps are generated automatically, and sub-folders are fine.

### The release pack

The `textures-hd-*.zip` files hold all 30,501 of the game's textures, AI-upscaled 4× (Real-ESRGAN general-x4v3): every track, every truck and driver, the menus and the HUD. Opaque textures are high-quality JPEG (quality 92, full colour resolution); the 1,300-odd with transparency are PNG. 1.36 GB in total, down from 3.9 GB as all-PNG.

### Rebuild it yourself

On a PC with an NVIDIA GPU and Python 3.12 (PyTorch has no build for 3.15 yet):

```
pip install torch --index-url https://download.pytorch.org/whl/cu128
pip install pillow numpy
tools\make_texture_pack.bat "C:\Games\4x4 Evo 2"
```

That runs the three tools in order:

1. `pod_textures.py` exports every texture from the game's `.POD` archives to `textures_src\`. It decodes 4x4 Evo 2's TIFF layouts itself, because Pillow drops their alpha.
2. `upscale_textures.py` upscales them 4× into `textures\`. It wrap-pads tileable terrain so it stays seamless and keeps cut-out alpha masks on their original levels (Evo 2's foliage uses 0/240/255 for its two-pass alpha test).
3. `optimize_textures.py` turns the opaque ones into JPEG.

A recent GeForce does the whole set in about 1.5 hours; already-finished textures are skipped on re-runs.

## Known limitations and next steps

- **Menus are 4:3 layouts** in widescreen: the backgrounds are stretched to fill the screen and the menu text stays in the centre 4:3 area.
- **Lighting is still the original per-vertex lighting.** The game also ships a hardware T&L renderer (`tridx8tl.dll`) that takes transforms, lights, stencil shadows, cube maps and render-to-texture calls. That's the way into modern lighting and reflections.
- **Intro video, legal screen and the setup screen** are drawn by the game's own DirectDraw code before the renderer starts. The game switches the display to 640×480 for them, as it always did, so expect your monitor to change mode briefly at startup.
- **Mouse/gamepad** are unchanged from the original (DirectInput). Modern-controller support is a planned add-on.

## Troubleshooting

- **The game runs in a tiny or software-rendered mode.** `system\METAL.INI` must say `rendererDLLPath=tridx8.dll` and `useDirect3D=1`. If the game is under Program Files, Windows may be using a copy in `%LOCALAPPDATA%\VirtualStore\…\system\METAL.INI` instead.
- **No music.** Set `useRedBookAudio=0` in `[Game]` (or pick "Wave" for music in the game's sound options).
- **The image is blurry on a high-DPI screen.** The installer marks `4x42.exe` DPI-aware. Check Properties → Compatibility → "Change high DPI settings".
- **The view looks wrong or the HUD is misplaced.** Set `Widescreen=0` (classic 4:3) or `HudToEdges=0`.
- **A texture looks solid where it should be see-through.** It's probably a `.jpg`; replace it with the `.png` from a fresh export.
- **Anything else:** set `Trace=1`, reproduce, and look at `system\evo2026.log`.

## Building from source

The renderer source is in `evo2026-source.zip` (and on the repo). It cross-compiles with MinGW-w64:

```
make            # needs i686-w64-mingw32-g++ (Linux/WSL: apt install g++-mingw-w64-i686)
```

Output: `build/evo2026.dll`. 32-bit only, because the game is 32-bit.

## Credits and licenses

- **Renderer base:** 4x4e by Americus Maximus, MIT (see `LICENSE.4x4e`).
- **PNG/JPEG loading, PNG saving:** stb_image / stb_image_write by Sean Barrett, public domain.
- **Upscaling models:** Real-ESRGAN by Xintao Wang et al., BSD-3-Clause. Downloaded by the tool on first use, not bundled.
- **4x4 Evo 2** © 2001 Terminal Reality. You need your own copy of the game. No game files are included in the kit.
