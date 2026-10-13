/*
evo2026 — HD extensions for the 4x4 Evolution Direct3D 9 renderer. See Evo.hxx.
*/
#include "Evo.hxx"
#include "Trace.hxx"
#include "EvoHooks.hxx"
#include "Mathematics.Basic.hxx"
using namespace Mathematics;

#include <stdlib.h>
#include <string.h>

namespace Evo
{
    ConfigState Config;
    ViewState View;

    u32 ReadSetting(const char* name, const u32 value)
    {
        const u32 fallback = (u32)GetPrivateProfileIntA(EVO_SECTION, name, (INT)value, EVO_INI_FALLBACK);
        return (u32)GetPrivateProfileIntA(EVO_SECTION, name, (INT)fallback, EVO_INI);
    }

    static u32 ReadInt(const char* name, const u32 value) { return ReadSetting(name, value); }

    void LoadConfig(void)
    {
        Config.Width = ReadInt("Width", Config.Width);
        Config.Height = ReadInt("Height", Config.Height);
        Config.Window = ReadInt("Window", Config.Window);
        Config.Aspect = ReadInt("Aspect", Config.Aspect);
        Config.VSync = ReadInt("VSync", Config.VSync);
        Config.Anisotropy = ReadInt("Anisotropy", Config.Anisotropy);
        Config.Filter2D = ReadInt("Filter2D", Config.Filter2D);
        Config.Widescreen = ReadInt("Widescreen", Config.Widescreen);
        Config.HudToEdges = ReadInt("HudToEdges", Config.HudToEdges);

        if (Config.Window > 2) { Config.Window = 2; }
        if (Config.Anisotropy > 16) { Config.Anisotropy = 16; }

        EVO_TRACE("evo2026 config: %ux%u window=%u aspect=%u vsync=%u aniso=%u filter2d=%u",
            Config.Width, Config.Height, Config.Window, Config.Aspect, Config.VSync, Config.Anisotropy, Config.Filter2D);
    }

    void ComputeView(const u32 gameWidth, const u32 gameHeight, const u32 outWidth, const u32 outHeight)
    {
        View.GameWidth = gameWidth;
        View.GameHeight = gameHeight;
        View.OutWidth = outWidth;
        View.OutHeight = outHeight;

        const f64 game = (f64)gameWidth / (f64)gameHeight;
        const f64 out = (f64)outWidth / (f64)outHeight;

        // Widescreen: widen the logical image so it matches the output's aspect (Hor+).
        f64 factor = 1.0;
        if (Config.Widescreen && Hooks::Installed() && out > game * 1.001) { factor = out / game; }
        Hooks::SetWidescreen((f32)factor, gameWidth, gameHeight);

        View.Extra = Hooks::ExtraWidth();
        const f64 logicalWidth = (f64)gameWidth + 2.0 * (f64)View.Extra;
        const f64 logical = logicalWidth / (f64)gameHeight;

        if (Config.Aspect == 0 && View.Extra <= 0.0f)
        {
            View.X = 0; View.Y = 0; View.W = outWidth; View.H = outHeight;
        }
        else if (out > logical)
        {
            View.H = outHeight;
            View.W = (u32)((f64)outHeight * logical + 0.5);
        }
        else
        {
            View.W = outWidth;
            View.H = (u32)((f64)outWidth / logical + 0.5);
        }

        if (!(Config.Aspect == 0 && View.Extra <= 0.0f))
        {
            View.X = (outWidth - View.W) / 2;
            View.Y = (outHeight - View.H) / 2;
        }

        View.SX = (f32)((f64)View.W / logicalWidth);
        View.SY = (f32)View.H / (f32)gameHeight;
        View.OX = (f32)View.X + View.Extra * View.SX;
        View.OY = (f32)View.Y;

        View.CoreX = (u32)(View.OX + 0.5f);
        View.CoreY = View.Y;
        View.CoreW = (u32)((f32)gameWidth * View.SX + 0.5f);
        View.CoreH = View.H;

        EVO_TRACE("evo2026 view: game %ux%u (+%.1f each side) -> out %ux%u, image %u,%u %ux%u, 4:3 core %u..%u, scale %.3f,%.3f",
            gameWidth, gameHeight, View.Extra, outWidth, outHeight, View.X, View.Y, View.W, View.H,
            View.CoreX, View.CoreX + View.CoreW, View.SX, View.SY);
    }

    // ---------------------------------------------------------------------------------
    // Widescreen 2D overlay placement.
    //
    // The game draws its HUD as screen-space polygons with a constant depth value inside
    // the 640x480 image. In widescreen that image sits in the middle of a wider one, so:
    //   * pieces spanning (nearly) the full width - bars, fades - are stretched edge to edge;
    //   * in races, pieces in the left/right third move to the left/right screen edge,
    //     centred pieces stay. A draw call that sits on one side moves as a unit (e.g. the
    //     minimap); a call spanning both sides (e.g. the numbers of both gauges) is split
    //     into its connected pieces and each piece is placed on its own.
    // Races are recognised by the gauge textures (needle/tick/odometer) being drawn.
    // ---------------------------------------------------------------------------------

    static u32 RaceHudFrames = 0;
    static BOOL RaceHudSeen = FALSE;

    void OverlayFrame(void)
    {
        if (RaceHudSeen) { RaceHudFrames = 30; }
        else if (RaceHudFrames > 0) { RaceHudFrames--; }
        RaceHudSeen = FALSE;
    }

    static BOOL IsRaceHudTexture(const char* name)
    {
        if (name == NULL) { return FALSE; }
        return _strnicmp(name, "NEEDLE", 6) == 0 || _strnicmp(name, "TICK", 4) == 0 || _strnicmp(name, "ODOM", 4) == 0;
    }

    static f32 AnchorShift(const f32 x0, const f32 x1)
    {
        const f32 w = (f32)View.GameWidth;
        const f32 c = (x0 + x1) * 0.5f;
        if (c < w / 3.0f) { return -View.Extra; }
        if (c > w * 2.0f / 3.0f) { return View.Extra; }
        return 0.0f;
    }

    void PlaceOverlay(const f32* xs, const f32* ys, const u32 count, const u16* indexes, const u32 triangles,
        const char* texture, f32* dx, f32* sx)
    {
        for (u32 i = 0; i < count; i++) { dx[i] = 0.0f; sx[i] = 1.0f; }
        if (View.Extra <= 0.0f || count == 0) { return; }

        if (IsRaceHudTexture(texture)) { RaceHudSeen = TRUE; }

        const f32 w = (f32)View.GameWidth;
        f32 x0 = 1e9f, x1 = -1e9f;
        for (u32 i = 0; i < count; i++) { x0 = Min(x0, xs[i]); x1 = Max(x1, xs[i]); }

        // Full-width element: stretch across the widescreen image.
        if (x0 <= w * 0.03f && x1 >= w * 0.97f && (x1 - x0) >= w * 0.95f)
        {
            const f32 scale = (w + 2.0f * View.Extra) / w;
            for (u32 i = 0; i < count; i++) { sx[i] = scale; dx[i] = -View.Extra; }
            return;
        }

        // Solid-colour panels drawn as strips (menu title/footer bars are rows of 10px quads):
        // a strip touching the left/right border of the 4:3 image is extended to the
        // widescreen border, so the bar reaches the screen edge. Plain colour only, so
        // nothing gets visibly stretched.
        const BOOL solid = texture == NULL || texture[0] == '\0' || _strnicmp(texture, "BBWHITE", 7) == 0;
        if (solid)
        {
            for (u32 i = 0; i < count; i++)
            {
                if (xs[i] <= 0.5f) { dx[i] = -View.Extra; }
                else if (xs[i] >= w - 0.5f) { dx[i] = View.Extra; }
            }
        }

        if (!Config.HudToEdges || (RaceHudFrames == 0 && !RaceHudSeen)) { return; }

        // Whole call on one side: move it as a unit.
        if ((x1 - x0) < w * 0.5f)
        {
            const f32 shift = AnchorShift(x0, x1);
            if (shift != 0.0f) { for (u32 i = 0; i < count; i++) { dx[i] = shift; } }
            return;
        }

        // Spans both sides: place each connected piece separately.
        static u16 parent[4096];
        if (count > 4096 || indexes == NULL) { return; }
        for (u32 i = 0; i < count; i++) { parent[i] = (u16)i; }
        auto find = [](u16 v) -> u16 { while (parent[v] != v) { parent[v] = parent[parent[v]]; v = parent[v]; } return v; };
        for (u32 t = 0; t < triangles; t++)
        {
            const u16 a = indexes[t * 3 + 0], b = indexes[t * 3 + 1], c = indexes[t * 3 + 2];
            if (a >= count || b >= count || c >= count) { continue; }
            u16 ra = find(a), rb = find(b), rc = find(c);
            if (ra != rb) { parent[rb] = ra; }
            rc = find(c); ra = find(a);
            if (ra != rc) { parent[rc] = ra; }
        }

        static f32 lo[4096], hi[4096];
        for (u32 i = 0; i < count; i++) { lo[i] = 1e9f; hi[i] = -1e9f; }
        for (u32 i = 0; i < count; i++) { const u16 r = find((u16)i); lo[r] = Min(lo[r], xs[i]); hi[r] = Max(hi[r], xs[i]); }
        for (u32 i = 0; i < count; i++)
        {
            const u16 r = find((u16)i);
            const f32 shift = AnchorShift(lo[r], hi[r]);
            if (shift != 0.0f) { dx[i] = shift; }
        }
        (void)ys;
    }

    void PrepareWindow(const HWND hwnd, const u32 gameWidth, const u32 gameHeight, u32* outWidth, u32* outHeight)
    {
        // Old games are not DPI aware; without this, Windows reports a scaled-down desktop
        // (e.g. 2560x1440 on a 4K screen at 150%) and blurs the result.
        {
            typedef BOOL(WINAPI* SETPROCESSDPIAWARE)(void);
            const auto user = GetModuleHandleA("user32.dll");
            const auto fn = user != NULL ? (SETPROCESSDPIAWARE)GetProcAddress(user, "SetProcessDPIAware") : NULL;
            if (fn != NULL) { fn(); }
        }

        // The game's own startup code (setup screen, videos) may have switched the display to
        // 640x480. Use the user's saved desktop mode, and restore it for windowed/borderless play.
        {
            DEVMODEA saved; ZeroMemory(&saved, sizeof(saved)); saved.dmSize = sizeof(saved);
            DEVMODEA current; ZeroMemory(&current, sizeof(current)); current.dmSize = sizeof(current);

            if (Config.Window != 0
                && EnumDisplaySettingsA(NULL, ENUM_REGISTRY_SETTINGS, &saved)
                && EnumDisplaySettingsA(NULL, ENUM_CURRENT_SETTINGS, &current)
                && (saved.dmPelsWidth != current.dmPelsWidth || saved.dmPelsHeight != current.dmPelsHeight))
            {
                EVO_TRACE("evo2026: restoring desktop mode %ux%u (was %ux%u)",
                    (u32)saved.dmPelsWidth, (u32)saved.dmPelsHeight, (u32)current.dmPelsWidth, (u32)current.dmPelsHeight);
                ChangeDisplaySettingsA(NULL, 0);
            }
        }

        MONITORINFO mi;
        ZeroMemory(&mi, sizeof(mi));
        mi.cbSize = sizeof(mi);
        GetMonitorInfoA(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);

        const u32 monitorWidth = (u32)(mi.rcMonitor.right - mi.rcMonitor.left);
        const u32 monitorHeight = (u32)(mi.rcMonitor.bottom - mi.rcMonitor.top);

        u32 width = Config.Width != 0 ? Config.Width : monitorWidth;
        u32 height = Config.Height != 0 ? Config.Height : monitorHeight;

        if (Config.Window == 1 && Config.Width == 0)
        {
            // Windowed with no explicit size: largest integer multiple of the game size that fits.
            u32 scale = 1;
            while ((scale + 1) * gameWidth <= monitorWidth * 9 / 10 && (scale + 1) * gameHeight <= monitorHeight * 9 / 10) { scale++; }
            width = gameWidth * scale;
            height = gameHeight * scale;
        }

        if (hwnd != NULL)
        {
            if (Config.Window == 1)
            {
                const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_VISIBLE;
                RECT rect = { 0, 0, (LONG)width, (LONG)height };
                AdjustWindowRect(&rect, style, FALSE);
                SetWindowLongA(hwnd, GWL_STYLE, style);
                SetWindowPos(hwnd, HWND_NOTOPMOST,
                    mi.rcWork.left + ((mi.rcWork.right - mi.rcWork.left) - (rect.right - rect.left)) / 2,
                    mi.rcWork.top + ((mi.rcWork.bottom - mi.rcWork.top) - (rect.bottom - rect.top)) / 2,
                    rect.right - rect.left, rect.bottom - rect.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            }
            else
            {
                SetWindowLongA(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
                SetWindowPos(hwnd, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                    (s32)monitorWidth, (s32)monitorHeight, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
            }
        }

        *outWidth = width;
        *outHeight = height;
    }

    void SetViewViewport(IDirect3DDevice9* device)
    {
        D3DVIEWPORT9 vp = { View.X, View.Y, View.W, View.H, 0.0f, 1.0f };
        device->SetViewport(&vp);
    }

    void SetFullViewport(IDirect3DDevice9* device)
    {
        D3DVIEWPORT9 vp = { 0, 0, View.OutWidth, View.OutHeight, 0.0f, 1.0f };
        device->SetViewport(&vp);
    }

    // ---------------------------------------------------------------------------------
    // Framebuffer access.
    //
    // The game sometimes writes pixels straight into the frame (Bink videos, the setup
    // screen, some text). It expects a GameWidth x GameHeight buffer, but our back buffer
    // is the full output size. On lock we hand it a small copy of the current frame; on
    // unlock we composite back only the pixels it actually changed, so the HD 3D image
    // underneath stays sharp.
    // ---------------------------------------------------------------------------------

    static struct
    {
        IDirect3DSurface9* Target = NULL;   // GameWidth x GameHeight render target (downscaled copy)
        IDirect3DSurface9* System = NULL;   // system-memory readback of Target
        IDirect3DTexture9* Overlay = NULL;  // changed pixels, uploaded for compositing
        IDirect3DStateBlock9* States = NULL;
        u8* Pixels = NULL;                  // what the game draws into (its own pixel format)
        u8* Before = NULL;                  // copy of Pixels taken at lock time
        u32 Width = 0, Height = 0, BPP = 0, Pitch = 0;
        BOOL Locked = FALSE;
    } Frame;

    void ReleaseFrame(void)
    {
        if (Frame.Target != NULL) { Frame.Target->Release(); Frame.Target = NULL; }
        if (Frame.System != NULL) { Frame.System->Release(); Frame.System = NULL; }
        if (Frame.Overlay != NULL) { Frame.Overlay->Release(); Frame.Overlay = NULL; }
        if (Frame.States != NULL) { Frame.States->Release(); Frame.States = NULL; }
        if (Frame.Pixels != NULL) { free(Frame.Pixels); Frame.Pixels = NULL; }
        if (Frame.Before != NULL) { free(Frame.Before); Frame.Before = NULL; }
        Frame.Width = Frame.Height = Frame.BPP = Frame.Pitch = 0;
        Frame.Locked = FALSE;
    }

    static BOOL EnsureFrame(IDirect3DDevice9* device, const u32 bpp)
    {
        if (Frame.Target != NULL && Frame.Width == View.GameWidth && Frame.Height == View.GameHeight && Frame.BPP == bpp) { return TRUE; }

        ReleaseFrame();

        if (device->CreateRenderTarget(View.GameWidth, View.GameHeight, D3DFMT_X8R8G8B8, D3DMULTISAMPLE_NONE, 0, FALSE, &Frame.Target, NULL) != D3D_OK) { ReleaseFrame(); return FALSE; }
        if (device->CreateOffscreenPlainSurface(View.GameWidth, View.GameHeight, D3DFMT_X8R8G8B8, D3DPOOL_SYSTEMMEM, &Frame.System, NULL) != D3D_OK) { ReleaseFrame(); return FALSE; }
        if (device->CreateTexture(View.GameWidth, View.GameHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &Frame.Overlay, NULL) != D3D_OK) { ReleaseFrame(); return FALSE; }
        if (device->CreateStateBlock(D3DSBT_ALL, &Frame.States) != D3D_OK) { ReleaseFrame(); return FALSE; }

        Frame.Width = View.GameWidth;
        Frame.Height = View.GameHeight;
        Frame.BPP = bpp == 16 ? 16 : 32;
        Frame.Pitch = Frame.Width * (Frame.BPP / 8);

        Frame.Pixels = (u8*)malloc((size_t)Frame.Pitch * Frame.Height);
        Frame.Before = (u8*)malloc((size_t)Frame.Pitch * Frame.Height);
        if (Frame.Pixels == NULL || Frame.Before == NULL) { ReleaseFrame(); return FALSE; }

        return TRUE;
    }

    // Draws Frame.Overlay over the 4:3 core rectangle (premultiplied blend, or opaque).
    static void DrawOverlay(IDirect3DDevice9* device, const BOOL blend)
    {
        Frame.States->Capture();

        struct QuadVertex { f32 X, Y, Z, RHW, U, V; };
        const f32 x0 = (f32)View.CoreX - 0.5f, y0 = (f32)View.CoreY - 0.5f;
        const f32 x1 = x0 + (f32)View.CoreW, y1 = y0 + (f32)View.CoreH;
        const QuadVertex quad[4] =
        {
            { x0, y0, 0.0f, 1.0f, 0.0f, 0.0f },
            { x1, y0, 0.0f, 1.0f, 1.0f, 0.0f },
            { x0, y1, 0.0f, 1.0f, 0.0f, 1.0f },
            { x1, y1, 0.0f, 1.0f, 1.0f, 1.0f },
        };

        device->BeginScene();
        device->SetVertexShader(NULL);
        device->SetPixelShader(NULL);
        device->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
        device->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        device->SetRenderState(D3DRS_ZWRITEENABLE, FALSE);
        device->SetRenderState(D3DRS_FOGENABLE, FALSE);
        device->SetRenderState(D3DRS_LIGHTING, FALSE);
        device->SetRenderState(D3DRS_SPECULARENABLE, FALSE);
        device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
        // Overlay is premultiplied (unchanged pixels are 0,0,0,0), so filtered edges blend cleanly.
        device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
        device->SetRenderState(D3DRS_ALPHABLENDENABLE, blend);
        device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
        device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
        device->SetTexture(0, Frame.Overlay);
        device->SetTexture(1, NULL);
        device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
        device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
        device->SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
        device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
        device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
        const DWORD filter = Config.Filter2D ? D3DTEXF_LINEAR : D3DTEXF_POINT;
        device->SetSamplerState(0, D3DSAMP_MAGFILTER, filter);
        device->SetSamplerState(0, D3DSAMP_MINFILTER, filter);
        device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
        device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
        device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
        device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(QuadVertex));
        device->EndScene();

        Frame.States->Apply();
    }

    BOOL LockFrame(IDirect3DDevice9* device, const u32 bpp, u8** bits, u32* pitch)
    {
        if (Frame.Locked) { return FALSE; }
        if (!EnsureFrame(device, bpp)) { return FALSE; }

        // Give the game a GameWidth x GameHeight copy of the current (HD) frame.
        IDirect3DSurface9* back = NULL;
        if (device->GetRenderTarget(0, &back) == D3D_OK)
        {
            const RECT source = { (LONG)View.CoreX, (LONG)View.CoreY, (LONG)(View.CoreX + View.CoreW), (LONG)(View.CoreY + View.CoreH) };
            device->StretchRect(back, &source, Frame.Target, NULL, D3DTEXF_LINEAR);
            back->Release();
        }

        D3DLOCKED_RECT rect;
        if (device->GetRenderTargetData(Frame.Target, Frame.System) == D3D_OK
            && Frame.System->LockRect(&rect, NULL, D3DLOCK_READONLY) == D3D_OK)
        {
            for (u32 y = 0; y < Frame.Height; y++)
            {
                const u32* in = (const u32*)((const u8*)rect.pBits + (size_t)y * rect.Pitch);
                u8* out = Frame.Pixels + (size_t)y * Frame.Pitch;

                if (Frame.BPP == 32) { memcpy(out, in, Frame.Pitch); continue; }

                u16* o = (u16*)out;
                for (u32 x = 0; x < Frame.Width; x++)
                {
                    const u32 c = in[x];
                    o[x] = (u16)(((c >> 8) & 0xf800) | ((c >> 5) & 0x07e0) | ((c >> 3) & 0x001f));
                }
            }
            Frame.System->UnlockRect();
        }
        else
        {
            memset(Frame.Pixels, 0, (size_t)Frame.Pitch * Frame.Height);
        }

        memcpy(Frame.Before, Frame.Pixels, (size_t)Frame.Pitch * Frame.Height);

        Frame.Locked = TRUE;
        *bits = Frame.Pixels;
        *pitch = Frame.Pitch;

        return TRUE;
    }

    void UnlockFrame(IDirect3DDevice9* device)
    {
        if (!Frame.Locked) { return; }
        Frame.Locked = FALSE;

        D3DLOCKED_RECT dst;
        if (Frame.Overlay->LockRect(0, &dst, NULL, D3DLOCK_DISCARD) != D3D_OK) { return; }

        u32 changed = 0;

        for (u32 y = 0; y < Frame.Height; y++)
        {
            const u8* now = Frame.Pixels + (size_t)y * Frame.Pitch;
            const u8* was = Frame.Before + (size_t)y * Frame.Pitch;
            u32* out = (u32*)((u8*)dst.pBits + (size_t)y * dst.Pitch);

            if (memcmp(now, was, Frame.Pitch) == 0)
            {
                memset(out, 0, (size_t)Frame.Width * 4);
                continue;
            }

            if (Frame.BPP == 16)
            {
                const u16* n = (const u16*)now; const u16* w = (const u16*)was;
                for (u32 x = 0; x < Frame.Width; x++)
                {
                    if (n[x] == w[x]) { out[x] = 0; continue; }
                    const u32 c = n[x];
                    const u32 r = ((c >> 11) & 0x1f) * 255 / 31, g = ((c >> 5) & 0x3f) * 255 / 63, b = (c & 0x1f) * 255 / 31;
                    out[x] = 0xff000000 | (r << 16) | (g << 8) | b;
                    changed++;
                }
            }
            else
            {
                const u32* n = (const u32*)now; const u32* w = (const u32*)was;
                for (u32 x = 0; x < Frame.Width; x++)
                {
                    if ((n[x] & 0xffffff) == (w[x] & 0xffffff)) { out[x] = 0; continue; }
                    out[x] = 0xff000000 | (n[x] & 0xffffff);
                    changed++;
                }
            }
        }

        Frame.Overlay->UnlockRect(0);

        if (changed == 0) { return; }

        // Composite the changed pixels over the HD frame.
        DrawOverlay(device, TRUE);
    }
}
