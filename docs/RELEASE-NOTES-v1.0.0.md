4x4 Evo 2 (Build 139) on modern Windows: native resolution, true widescreen and a full HD texture pack, in one installer.

![Before and after](https://github.com/antonflor/4x4-Evo2-2026-Recomp/blob/main/docs/screenshots/widescreen-before-after.jpg?raw=true)

## Install
Download **`4x4Evo2-2026-Setup.exe` and all `4x4Evo2-2026-Setup-N.bin` files** (74 of them) into the same folder and run the exe. The default folder is `C:\Games\4x4 Evo 2`. You don't need a disc or a patch.

The "4x4 Setup Options" screen appears on every launch, as in the original. Press Enter.

## What's in it
- **evo2026 renderer** (Direct3D 9), installed as `tridx8.dll`. It draws the game at your monitor's native resolution in a borderless window, with 16x anisotropic filtering and vsync.
- **True widescreen (Hor+)**: you see more of the track at the sides, nothing is stretched, and the race HUD (lap times, minimap, gauges) moves out to the screen edges.
- **Correct Evo 2 blending**: the renderer handles Evo 2's alpha-ref and blend calls, so tree edges are clean and additive effects glow.
- **Full HD texture pack**: all 30,501 of the game's textures, AI-upscaled 4x (Real-ESRGAN): every track, every truck and driver, the menus and the HUD. Opaque textures are high-quality JPEG and the ones with transparency stay PNG, which brings the pack from 3.9 GB down to 1.4 GB.
- Pre-configured: 32-bit colour, music from the game files (no CD needed), and the exe marked DPI-aware.

Settings: Start menu → 4x4 Evo 2 2026 → evo2026 Settings (`system\evo2026.ini`).

## Other files
- `evo2026-kit-1.0.0.zip`: the renderer for an existing install, with `install.ps1` / `uninstall.ps1`, the texture tools and the source. See `README.md` inside.
- `textures-hd-01.zip` … `textures-hd-15.zip`: the HD pack on its own. Put them next to `install.ps1` and it unpacks them, or unzip them into the game's `textures\` folder.
- `evo2026_before_after.png`, `evo2026_widescreen_screens.png`: comparison shots.
- `SHA256SUMS.txt`

## Known limits
- Menu backgrounds are stretched to fill widescreen. The race view itself is true widescreen.
- The intro movie and legal screen run at 640x480 before the renderer starts, so your monitor may briefly switch modes.

Personal-use build from my own disc. Please don't redistribute it.
