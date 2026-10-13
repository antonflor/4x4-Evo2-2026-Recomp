/*
evo2026 — in-memory hooks into 4x42.exe (4x4 Evo 2, Build 139) for true widescreen. See EvoHooks.hxx.

Engine facts (from disassembly of Build 139; Evo 1 Build 57 addresses in brackets):
  0x4969b0  viewport(f32 x0, f32 y0, f32 x1, f32 y1) [0x481720]  stores rect at 0xaa843c..,
            half sizes at 0xaa844c/50, centre at 0xaa8454/58; projection is  sx = x'/z * halfW + cx.
            (If [0xaae17c] == 0 the coordinates are first multiplied by the scale at 0xaa84b4.)
  0x495e10  set zoom (raw)       -> [0xaa8498] = v              [0x4810a0]
  0x495e20  set zoom from FOV    -> [0xaa8498] = v / 18.0f (0x63b193)  [0x4810b0]
  0x495f30 / 0x4963b0  camera matrix setup: horizontal column scaled by zoom (0xaa849c),
            vertical column by zoom * (viewport width / viewport height) (0xaa84a0).
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

    static f32* const Zoom = (f32*)0x00aa8498;
    static const f32* const FovDivisor = (const f32*)0x0063b193;

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

        // viewport: sub esp,8 / cmp dword [0xaae17c],0 (10 bytes, position independent);
        // the je that follows stays in place and the gateway jumps back to it.
        static const u8 viewport[] = { 0x83, 0xEC, 0x08, 0x83, 0x3D, 0x7C, 0xE1, 0xAA, 0x00, 0x00 };
        static const u8 viewportNext[] = { 0x0F, 0x84, 0xC4, 0x00, 0x00, 0x00 };
        static const u8 zoomRaw[] = { 0x8B, 0x44, 0x24, 0x04, 0xA3, 0x98, 0x84, 0xAA, 0x00, 0xC3 };
        static const u8 zoomFov[] = { 0xD9, 0x44, 0x24, 0x04, 0xD8, 0x35, 0x93, 0xB1, 0x63, 0x00, 0xD9, 0x1D, 0x98, 0x84, 0xAA, 0x00, 0xC3 };

        // The exe has a fixed image base (no relocation), so these addresses are stable;
        // anything else (another build/patch) fails the byte check and we stay 4:3.
        if (GetModuleHandleA(NULL) != (HMODULE)0x00400000
            || !Matches(0x4969b0, viewport, sizeof(viewport))
            || !Matches(0x4969b0 + sizeof(viewport), viewportNext, sizeof(viewportNext))
            || !Matches(0x495e10, zoomRaw, sizeof(zoomRaw))
            || !Matches(0x495e20, zoomFov, sizeof(zoomFov)))
        {
            EVO_TRACE("evo2026 hooks: 4x42.exe is not 4x4 Evo 2 Build 139 as expected - widescreen disabled");
            return FALSE;
        }

        H.ViewportGateway = MakeGateway(0x4969b0, sizeof(viewport));
        if (H.ViewportGateway == NULL) { return FALSE; }

        if (!WriteJump(0x4969b0, (const void*)&ViewportHook, sizeof(viewport))) { return FALSE; }
        WriteJump(0x495e10, (const void*)&SetZoomRawHook, sizeof(zoomRaw));
        WriteJump(0x495e20, (const void*)&SetZoomFovHook, sizeof(zoomFov));

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
