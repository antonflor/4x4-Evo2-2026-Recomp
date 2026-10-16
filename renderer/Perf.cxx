/*
evo2026 — lightweight performance counters. See Perf.hxx.
*/
#include "Perf.hxx"
#include "Evo.hxx"

#include <stdio.h>

namespace Evo::Perf
{
    BOOL Enabled = FALSE;
    s64 Ticks[AreaCount];
    u32 Counts[CountCount];

    static s64 Frequency = 1;
    static s64 SecondStart = 0, FrameStart = 0, FrameMax = 0;
    static u32 Frames = 0;
    static FILE* Log = NULL;
    static BOOL Ready = FALSE;

    s64 Now(void)
    {
        LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart;
    }

    void Init(void)
    {
        if (Ready) { return; }
        Ready = TRUE;
        Enabled = ReadSetting("LogFps", 0) != 0;
        LARGE_INTEGER f; QueryPerformanceFrequency(&f); Frequency = f.QuadPart;
        ZeroMemory(Ticks, sizeof(Ticks)); ZeroMemory(Counts, sizeof(Counts));
    }

    static s32 Stack[16];
    static u32 Depth = 0;
    static s64 Mark = 0;

    void Enter(const Area a)
    {
        const s64 now = Now();
        if (Depth > 0) { Ticks[Stack[Depth - 1]] += now - Mark; }
        if (Depth < 16) { Stack[Depth] = a; }
        Depth++;
        Mark = now;
    }

    void Leave(void)
    {
        const s64 now = Now();
        if (Depth > 0 && Depth <= 16) { Ticks[Stack[Depth - 1]] += now - Mark; }
        if (Depth > 0) { Depth--; }
        Mark = now;
    }

    static f64 Ms(const s64 t) { return t * 1000.0 / (f64)Frequency; }

    void Frame(void)
    {
        Init();
        if (!Enabled) { return; }

        const s64 now = Now();
        if (SecondStart == 0) { SecondStart = FrameStart = now; return; }

        const s64 frame = now - FrameStart;
        if (frame > FrameMax) { FrameMax = frame; }
        FrameStart = now;
        Frames++;

        const s64 span = now - SecondStart;
        if (Ms(span) < 1000.0) { return; }

        if (Log == NULL)
        {
            Log = fopen(".\\system\\evo2026-fps.log", "w");
            if (Log == NULL) { Enabled = FALSE; return; }
            fprintf(Log, "  fps  frame avg/max ms | game ms | draw upload  hd decode present ms | draws/frame uploads hd-hits hd-loads (per second)\n");
        }

        s64 inside = 0;
        for (u32 i = 0; i < AreaCount; i++) { inside += Ticks[i]; }

        const f64 n = Frames > 0 ? Frames : 1;
        fprintf(Log, "%5.1f  %6.2f / %6.2f | %6.2f | %5.2f %6.2f %5.2f %6.2f %6.2f | %6.0f %5u %5u %4u\n",
            Frames * 1000.0 / Ms(span),
            Ms(span) / n, Ms(FrameMax),
            Ms(span - inside) / n,
            Ms(Ticks[Draw]) / n, Ms(Ticks[Upload]) / n, Ms(Ticks[Hd]) / n, Ms(Ticks[Decode]) / n, Ms(Ticks[Present]) / n,
            Counts[DrawCalls] / n, Counts[Uploads], Counts[HdHits], Counts[HdLoads]);
        fflush(Log);

        ZeroMemory(Ticks, sizeof(Ticks)); ZeroMemory(Counts, sizeof(Counts));
        Frames = 0; FrameMax = 0; SecondStart = now;
    }
}
