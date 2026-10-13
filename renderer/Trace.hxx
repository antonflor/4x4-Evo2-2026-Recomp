#pragma once
// Lightweight call tracer for evo2026.
//   [evo2026] Trace=1        enable .\system\evo2026.log
//             TraceEvery=N   only log every Nth frame (default 1)
namespace Evo { void Trace(const char* fmt, ...); void TraceFrame(void); extern int TraceEnabled; extern int TraceActive; }
#define EVO_TRACE(...) do { if (Evo::TraceEnabled && Evo::TraceActive) { Evo::Trace(__VA_ARGS__); } } while (0)
