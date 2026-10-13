4x4 Evo 2 (Build 139) on modern Windows: native resolution, true widescreen and HD textures, in one installer.

## Install
Download **`4x4Evo2-2026-Setup.exe` and all 36 `4x4Evo2-2026-Setup-N.bin` files** into the same folder and run the exe. The default folder is `C:\Games\4x4 Evo 2`. You don't need a disc or a patch.

The "4x4 Setup Options" screen appears on every launch, as in the original. Press Enter.

## What's in it
- **evo2026 renderer** (Direct3D 9), installed as `tridx8.dll`. It draws the game at your monitor's native resolution in a borderless window, with 16x anisotropic filtering and vsync.
- **True widescreen (Hor+)**: you see more of the track at the sides, nothing is stretched, and the race HUD (lap times, minimap, gauges) moves out to the screen edges.
- **Correct Evo 2 blending**: the renderer handles Evo 2's alpha-ref and blend calls, so tree edges are clean and additive effects glow.
- **HD texture starter pack** (1,671 textures, AI-upscaled 4x): menus and UI, all trucks and drivers, and the Construction Junction track. The other tracks keep their original textures, still drawn at full resolution.
- Pre-configured: 32-bit colour, music from the game files (no CD needed), and the exe marked DPI-aware.

Settings: Start menu → 4x4 Evo 2 2026 → evo2026 Settings (`system\evo2026.ini`).

## Other files
- `evo2026-evo2-renderer-v1.0.0.zip`: the renderer only, for an existing install (see INSTALL.txt inside).
- `evo2-hd-textures-starter-v1.0.0.zip`: the starter pack only (unzip into `textures\`).
- `SHA256SUMS.txt`

## Known limits
- Menu backgrounds are stretched to fill widescreen. The race view itself is true widescreen.
- The intro movie and legal screen run at 640x480 before the renderer starts, so your monitor may briefly switch modes.
- To get HD textures for every track, run `evo2026\tools\make_texture_pack.bat` on an NVIDIA GPU (instructions inside).

Personal-use build from my own disc. Please don't redistribute it.
