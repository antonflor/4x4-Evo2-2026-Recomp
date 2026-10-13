/*
evo2026 — in-memory hooks into 4x4.exe (Build 57) for true widescreen.

The game projects 3D on the CPU for a 640x480 (4:3) screen. To get Hor+ widescreen
(same vertical view, extra scenery at the sides) we:
  1. widen the engine viewport (0x481720) left and right of the 4:3 image whenever the
     game sets the full-screen view, so projection centre stays put and clipping opens up;
  2. compress the camera's horizontal axis (camera matrix setup, 0x481100) by the same
     factor, so objects keep their proportions and the frustum gets wider.
The renderer then maps the wider logical range onto the whole output.

Nothing on disk is modified. Every hook site is checked against the exact Build 57 bytes;
if anything differs (other build, other patch), no hook is installed and the game simply
runs 4:3.

[evo2026] Widescreen=1   (0 = classic 4:3 with side bars)
          HudToEdges=1   (move the race HUD to the screen edges)
*/
#pragma once

#include "Basic.hxx"
#include "Native.Basic.hxx"

namespace Evo::Hooks
{
    // Installs the hooks once (safe to call repeatedly). Returns TRUE if active.
    BOOL Install(void);

    // Horizontal widening factor (1.0 = 4:3). Set from the output aspect at mode set.
    void SetWidescreen(const f32 factor, const u32 gameWidth, const u32 gameHeight);

    // Extra logical pixels on each side of the 640-wide image (0 when not widescreen).
    f32 ExtraWidth(void);

    BOOL Installed(void);
}
