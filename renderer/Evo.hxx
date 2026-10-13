/*
evo2026 — HD extensions for the 4x4 Evolution Direct3D 9 renderer.

The game keeps rendering in its own "logical" resolution (e.g. 640x480, which keeps
menus/HUD laid out exactly as designed). This module maps that logical space onto
the real output (native desktop resolution by default), so 3D geometry is rasterised
at full HD/4K detail instead of being upscaled.

Settings live in .\system\evo2026.ini, section [evo2026] (the game rewrites render.ini
on exit and would drop them; render.ini [evo2026] is still read as a fallback):
    Width=0          output width  (0 = monitor's native width)
    Height=0         output height (0 = monitor's native height)
    Window=2         0 = exclusive fullscreen, 1 = window, 2 = borderless fullscreen window
    Aspect=1         0 = stretch to fill, 1 = keep the game's aspect (side bars)
    VSync=1          1 = sync to monitor refresh
    Anisotropy=16    texture anisotropic filtering level (0/1 = off)
    Filter2D=1       1 = smooth scaling of 2D framebuffer art, 0 = sharp pixels
    Widescreen=1     1 = true widescreen (Hor+): wider view, not stretched. 0 = 4:3 + side bars
    HudToEdges=1     1 = move the race HUD to the screen edges in widescreen
    Trace=0          1 = write .\system\evo2026.log (renderer call trace)
*/
#pragma once

#include "Basic.hxx"
#include "Native.Basic.hxx"

#include <d3d9.h>

#define EVO_INI ".\\system\\evo2026.ini"
#define EVO_INI_FALLBACK ".\\system\\render.ini"
#define EVO_SECTION "evo2026"

namespace Evo
{
    struct ConfigState
    {
        u32 Width = 0;
        u32 Height = 0;
        u32 Window = 2;
        u32 Aspect = 1;
        u32 VSync = 1;
        u32 Anisotropy = 16;
        u32 Filter2D = 1;
        u32 Widescreen = 1;
        u32 HudToEdges = 1;
    };

    struct ViewState
    {
        // Output (back buffer) size.
        u32 OutWidth = 640;
        u32 OutHeight = 480;

        // Logical game size.
        u32 GameWidth = 640;
        u32 GameHeight = 480;

        // Region of the back buffer the game image occupies.
        u32 X = 0, Y = 0, W = 640, H = 480;

        // Logical -> output transform.
        f32 SX = 1.0f, SY = 1.0f, OX = 0.0f, OY = 0.0f;

        // Widescreen: logical pixels added left and right of the game's 4:3 image.
        f32 Extra = 0.0f;

        // Output rectangle covered by the original 4:3 logical image (0..GameWidth).
        u32 CoreX = 0, CoreY = 0, CoreW = 640, CoreH = 480;
    };

    extern ConfigState Config;
    extern ViewState View;

    // Reads [evo2026] Key from evo2026.ini, falling back to render.ini, then to the default.
    u32 ReadSetting(const char* name, const u32 value);

    void LoadConfig(void);
    void ComputeView(const u32 gameWidth, const u32 gameHeight, const u32 outWidth, const u32 outHeight);

    // Picks the output size and prepares the game window (borderless/windowed/fullscreen).
    void PrepareWindow(const HWND hwnd, const u32 gameWidth, const u32 gameHeight, u32* outWidth, u32* outHeight);

    inline f32 MapX(const f32 x) { return x * View.SX + View.OX; }

    // Race HUD / 2D overlay placement for widescreen (see Evo.cxx).
    void PlaceOverlay(const f32* xs, const f32* ys, const u32 count, const u16* indexes, const u32 triangles,
        const char* texture, f32* dx, f32* sx);
    void OverlayFrame(void);
    inline f32 MapY(const f32 y) { return y * View.SY + View.OY; }

    void SetViewViewport(IDirect3DDevice9* device);
    void SetFullViewport(IDirect3DDevice9* device);

    // Framebuffer access (LockFrame/UnlockFrame) on top of a scaled back buffer.
    BOOL LockFrame(IDirect3DDevice9* device, const u32 bpp, u8** bits, u32* pitch);
    void UnlockFrame(IDirect3DDevice9* device);
    void ReleaseFrame(void);
}
