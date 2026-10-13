#include "Native.Basic.hxx"
#include "Trace.hxx"
#include "Evo.hxx"
#include <stdio.h>
#include <stdarg.h>
namespace Evo
{
    int TraceEnabled = -1;
    int TraceActive = 1;
    static int TraceEvery = 1;
    static unsigned long Frame = 0;

    static void Load(void)
    {
        TraceEnabled = (int)ReadSetting("Trace", 0);
        TraceEvery = (int)ReadSetting("TraceEvery", 1);
        if (TraceEvery < 1) { TraceEvery = 1; }
    }

    void TraceFrame(void)
    {
        if (TraceEnabled < 0) { Load(); }
        Frame++;
        TraceActive = (Frame % (unsigned long)TraceEvery) == 0;
        if (TraceActive && TraceEnabled) { Trace("--- frame %lu", Frame); }
    }

    void Trace(const char* fmt, ...)
    {
        static FILE* f = NULL;
        static unsigned long count = 0;
        if (TraceEnabled < 0) { Load(); if (!TraceEnabled) return; }
        if (!TraceEnabled || ++count > 200000) return; // cap log size
        if (f == NULL) { f = fopen(".\\system\\evo2026.log", "w"); if (f == NULL) return; }
        va_list a; va_start(a, fmt); vfprintf(f, fmt, a); va_end(a); fputc('\n', f); fflush(f);
    }
}
