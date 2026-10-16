/*
evo2026 — lightweight performance counters.

LogFps=1 in evo2026.ini writes one line per second to system\evo2026-fps.log:

  fps  frame ms (avg / max)  game ms  |  renderer ms by area  |  counts

"game ms" is frame time not spent inside the renderer (the game's own simulation,
transform and lighting, sound, input). The renderer areas are:
  draw     PolyListTL / DrawPolygon / line+point lists, including the D3D draw calls
  upload   texture uploads (UpdateTexture), excluding HD replacement work
  hd       HD replacement lookup (name + CRC) for uploaded textures
  decode   decoding and creating HD replacement textures (PNG/JPEG)
  present  Present (includes waiting for vsync)
*/
#pragma once

#include "Basic.hxx"
#include "Native.Basic.hxx"

namespace Evo::Perf
{
    enum Area { Draw, Upload, Hd, Decode, Present, AreaCount };
    enum Count { DrawCalls, Uploads, HdHits, HdLoads, CountCount };

    extern BOOL Enabled;
    extern s64 Ticks[AreaCount];
    extern u32 Counts[CountCount];

    s64 Now(void);
    void Init(void);
    void Frame(void);   // call once per presented frame

    inline void Add(const Count c, const u32 n = 1) { if (Enabled) { Counts[c] += n; } }

    // Exclusive timing: entering a nested area pauses the enclosing one.
    void Enter(const Area a);
    void Leave(void);

    struct Scope
    {
        Scope(const Area a) { if (Enabled) { Enter(a); } }
        ~Scope() { if (Enabled) { Leave(); } }
    };
}

#define EVO_PERF(area) Evo::Perf::Scope evoPerfScope##__LINE__(Evo::Perf::area)
