/*
evo2026 — in-memory hooks into 4x4.exe (Build 57) for true widescreen. See EvoHooks.hxx.

Engine facts (from disassembly of Build 57):
  0x481720  viewport(f32 x0, f32 y0, f32 x1, f32 y1)  stores rect at 0xae25a0.., centre and
            half sizes at 0xae25b0..0xae25bc; projection is  sx = x'/z * halfW + cx.
  0x4810a0  set zoom (raw)       -> [0xae25fc] = v
  0x4810b0  set zoom from FOV    -> [0xae25fc] = v / 18.0f   (0x5c9a79)
  0x481100  camera matrix setup: horizontal column scaled by zoom, vertical column by
            zoom * (viewport width / viewport height).
  => widen the viewport by k and divide zoom by k: horizontal FOV grows by k, vertical stays.

*/
#include "EvoHooks.hxx"
#include "Native.Basic.hxx"
#include "Trace.hxx"

#include <math.h>
#include <string.h>

namespace Evo::Hooks
{
    static struct
    {
        BOOL Tried = FALSE;
        BOOL Installed = FALSE;
        f32 Factor = 1.0f;        // k
        f32 Extra = 0.0f;         // logical pixels added on each side
        u32 GameWidth = 640, GameHeight = 480;
        BOOL WideViewport = FALSE; // last viewport set was the widened full-screen one
        void* ViewportGateway = NULL;
    } H;

    typedef void(__cdecl* VIEWPORTFN)(f32, f32, f32, f32);

    static f32* const Zoom = (f32*)0x00ae25fc;
    static const f32* const FovDivisor = (const f32*)0x005c9a79;

    static BOOL Matches(const u32 address, const u8* bytes, const u32 length)
    {
        return memcmp((const void*)address, bytes, length) == 0;
    }

    static BOOL WriteJump(const u32 at, const void* to, const u32 length)
    {
        DWORD old = 0;
        if (!VirtualProtect((void*)at, length, PAGE_EXECUTE_READWRITE, &old)) { return FALSE; }

        u8* p = (u8*)at;
        p[0] = 0xE9;
        *(s32*)(p + 1) = (s32)((addr)to - (at + 5));
        for (u32 i = 5; i < length; i++) { p[i] = 0x90; }

        DWORD ignored = 0;
        VirtualProtect((void*)at, length, old, &ignored);
        FlushInstructionCache(GetCurrentProcess(), (void*)at, length);
        return TRUE;
    }

    // Copies `length` original bytes into executable memory followed by a jump back.
    static void* MakeGateway(const u32 at, const u32 length)
    {
        u8* g = (u8*)VirtualAlloc(NULL, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
        if (g == NULL) { return NULL; }
        memcpy(g, (const void*)at, length);
        g[length] = 0xE9;
        *(s32*)(g + length + 1) = (s32)((at + length) - ((addr)g + length + 5));
        FlushInstructionCache(GetCurrentProcess(), g, 64);
        return g;
    }

    static BOOL IsFullScreen(const f32 x0, const f32 y0, const f32 x1, const f32 y1)
    {
        const f32 w = (f32)H.GameWidth, h = (f32)H.GameHeight;
        return fabsf(x0) < 0.5f && fabsf(y0) < 0.5f
            && (fabsf(x1 - w) < 0.5f || fabsf(x1 - (w - 1.0f)) < 0.5f)
            && (fabsf(y1 - h) < 0.5f || fabsf(y1 - (h - 1.0f)) < 0.5f);
    }

    static void __cdecl ViewportHook(f32 x0, f32 y0, f32 x1, f32 y1)
    {
        const BOOL wide = H.Extra > 0.0f && IsFullScreen(x0, y0, x1, y1);

        if (wide != H.WideViewport)
        {
            // Keep the current zoom consistent with the new viewport shape.
            *Zoom = wide ? (*Zoom / H.Factor) : (*Zoom * H.Factor);
            H.WideViewport = wide;
        }

        if (wide) { x0 -= H.Extra; x1 += H.Extra; }

        EVO_TRACE("viewport %.1f,%.1f %.1f,%.1f%s", x0, y0, x1, y1, wide ? " (wide)" : "");

        ((VIEWPORTFN)H.ViewportGateway)(x0, y0, x1, y1);
    }

    static void __cdecl SetZoomRawHook(f32 value)
    {
        *Zoom = H.WideViewport ? value / H.Factor : value;
    }

    static void __cdecl SetZoomFovHook(f32 value)
    {
        const f32 zoom = value / *FovDivisor;
        *Zoom = H.WideViewport ? zoom / H.Factor : zoom;
    }

    BOOL Install(void)
    {
        if (H.Tried) { return H.Installed; }
        H.Tried = TRUE;

        static const u8 viewport[] = { 0x57, 0x83, 0xEC, 0x08, 0x83, 0x3D, 0xF4, 0x80, 0xAE, 0x00, 0x00 };
        static const u8 zoomRaw[] = { 0x8B, 0x44, 0x24, 0x04, 0xA3, 0xFC, 0x25, 0xAE, 0x00, 0xC3 };
        static const u8 zoomFov[] = { 0xD9, 0x44, 0x24, 0x04, 0xD8, 0x35, 0x79, 0x9A, 0x5C, 0x00, 0xD9, 0x1D, 0xFC, 0x25, 0xAE, 0x00, 0xC3 };

        // The exe has a fixed image base (no relocation), so these addresses are stable;
        // anything else (another build/patch) fails the byte check and we stay 4:3.
        if (GetModuleHandleA(NULL) != (HMODULE)0x00400000
            || !Matches(0x481720, viewport, sizeof(viewport))
            || !Matches(0x4810a0, zoomRaw, sizeof(zoomRaw))
            || !Matches(0x4810b0, zoomFov, sizeof(zoomFov)))
        {
            EVO_TRACE("evo2026 hooks: 4x4.exe is not Build 57 as expected - widescreen disabled");
            return FALSE;
        }

        H.ViewportGateway = MakeGateway(0x481720, sizeof(viewport));
        if (H.ViewportGateway == NULL) { return FALSE; }

        if (!WriteJump(0x481720, (const void*)&ViewportHook, sizeof(viewport))) { return FALSE; }
        WriteJump(0x4810a0, (const void*)&SetZoomRawHook, sizeof(zoomRaw));
        WriteJump(0x4810b0, (const void*)&SetZoomFovHook, sizeof(zoomFov));

        H.Installed = TRUE;
        EVO_TRACE("evo2026 hooks: installed");
        return TRUE;
    }

    void SetWidescreen(const f32 factor, const u32 gameWidth, const u32 gameHeight)
    {
        H.GameWidth = gameWidth;
        H.GameHeight = gameHeight;

        const f32 k = (H.Installed && factor > 1.001f) ? factor : 1.0f;

        // If a widened viewport is live, re-base the zoom on the new factor.
        if (H.WideViewport) { *Zoom = *Zoom * H.Factor / k; }

        H.Factor = k;
        H.Extra = (k - 1.0f) * (f32)gameWidth * 0.5f;
        if (H.Extra <= 0.0f && H.WideViewport) { H.WideViewport = FALSE; }

        EVO_TRACE("evo2026 widescreen: factor %.3f, extra %.1f px each side", H.Factor, H.Extra);
    }


    f32 ExtraWidth(void) { return H.Extra; }
    BOOL Installed(void) { return H.Installed; }
}
