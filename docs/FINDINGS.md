# 4x4 Evo 2: reverse-engineering notes

## Disc
- `4x4 Evo 2.bin/.cue`: one MODE1/2352 data track (227,957 sectors). It has no Red Book audio, so the music lives in MUSIC.POD.
- Root: `launcher.exe` (autorun; runs setup.exe unless the uninstall key `4x4 Evo2` exists), InstallShield 5 setup (`Setup.exe`, `data1.hdr/.cab`, `_user1/_sys1` cabs). **`4x42.exe` is the game exe and sits on the disc root**, outside the cab (setup copies it).
- data1.cab: 93 files, 883 MB expanded (`tools/is5_extract.py`). 50 POD archives, `binkw32.dll` (Bink 1.0s), `video/4x4.bik`, `system/METAL.INI`, `system/render.ini`, renderers `Tridx8.dll` (D3D8), `tridx8tl.dll` (D3D8 hardware T&L), `TRIGL.DLL` (OpenGL), and a `drivers/` HTML help folder.

## Game exe (4x42.exe, 2,776,064 bytes)
- "4x4 EVO 2" FileVersion 1.00.139 (Build 139), linked 2001-09-25, InternalName "Metal Crush 4". Watcom C/C++, image base 0x400000, has a .reloc section.
- No copy protection seen. `GetVolumeInformationA` is only used to XOR the volume serial into an ID (0x4e0729 → 0xb0b4ec).
- Renderer chosen by filename from `system\METAL.INI rendererDLLPath`. Name table at 0x6744a8: `trigl.dll`, `tridx8.dll`, `tridx8tl.dll`.
- Renderer loader 0x5c6130: only 17 exports are required (init, kill, toggle, setVideoMode2, restoreVideoMode, beginScene, endScene, lockFrame, unlockFrame, selectTexture, updateTexture, clear, clearZBuffer, setColorTable16, selectCard, buildCardList, getTextureInfo); a missing one sets the fail flag 0xd44df8. The other ~45 are optional: isInWindowMode, toggleWindowMode, lockFrameForScreenshot, setDirtyRect, BGRA textures, viewport/lights/transforms, polyList/D3DL/D3DTL, dot/line lists, texture handles, culling/clipping/clamp/fog, dynamic lights, drawPixels, videoSave/Restore, multitexture blend, cube maps, render-to-texture, optimized meshes, alpha ref, stencil, src/dest blend, gloss.
- `APIDLLselectTexture` takes a 6th argument: the texture stage index (multitexture/detail).
- Projection (Evo 1 Build 57 equivalents in brackets): zoom global 0xaa8498 [0xae25fc]; raw zoom setter 0x495e10, zoom/18 setter 0x495e20 [0x4810a0/0x4810b0]; viewport(x0,y0,x1,y1) 0x4969bc [0x481720], rect at 0xaa843c..48, half sizes 0xaa844c / 0xaa8450 (negated), centre 0xaa8454/58. The viewport function also forwards to the renderer's `APIDLLsetViewport` (fn ptr 0xd44d20) when the T&L path is active. Scaled-viewport variant at 0x496a84 (scale at 0xaa84b4).
- mtmfont: same "Invalid screen resolution! (PIXY = %d)" check (0x4aba27), so the game stays at logical 640x480.
- Same as Evo 1: "4x4 Setup Options" screen on every launch, Bink intro, OutputDebugString logging.

## Assets
- POD2 archives, the same format as Evo 1. 8,163 .RAW (7,590 at 64x64), 8,138 .ACT, 163 .OPA, plus 1,039 .TIF and 32 .TGA (32-bit UI and backgrounds).

## Wine test rig
- 32-bit prefix (`dpkg --add-architecture i386; apt install wine wine32:i386`), Xvfb 1920x1080x24.
- Run inside a virtual desktop: `wine explorer /desktop=evo,1920x1080 4x42.exe`. Without one, the DirectDraw 640x480 mode switch fails on Xvfb and the game crashes in init2d (null surface at 0x60270e).
- Keys: Return on "Run 4x4", Escape x2 skips intros, Return x2 creates the profile, Return x2 → Quick Race, Return x3 starts a race.
