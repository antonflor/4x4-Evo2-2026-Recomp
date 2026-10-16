/*
evo2026 — texture dumping and HD texture replacement.

[evo2026] in .\system\evo2026.ini:
    DumpTextures=0      1 = save every texture the game uploads to .\textures_dump\
    ReplaceTextures=1   1 = load replacements from .\textures\
    AsyncTextures=1     1 = decode HD textures on background threads; the game's own
                        texture is shown until the HD one is ready (a moment later).
                        0 = decode while the game waits (slower loading)
    HdTextureMemory=3072  MB of video memory kept for HD textures; the least recently
                        used ones are dropped beyond that

Every texture is identified by the game's own name plus a CRC32 of its pixels
(the same image can appear with different palettes, e.g. truck colours):

    textures_dump\<name>__<crc>.png       what the game uploaded

A replacement is picked up from .\textures\ (any sub-folder) if a PNG or JPEG named
either "<name>__<crc>" (this exact variant) or "<name>" (every variant) exists.
Replacements may be any size (power of two recommended, e.g. 4x the original);
mip-maps are generated automatically.
*/
#pragma once

#include "Basic.hxx"
#include "Native.Basic.hxx"

#include <d3d9.h>

namespace Evo::Textures
{
    void Initialize(void);

    // Called after the game's texture in slot `slot` has been converted to 32-bit ARGB.
    // Returns a ready replacement texture (caller takes one reference) or NULL. With
    // AsyncTextures, NULL may mean "being decoded": the replacement is handed to the
    // Apply callback given to Pump once it is ready.
    IDirect3DTexture9* Process(IDirect3DDevice9* device, const char* name,
        const u32* argb, const u32 dimensions, const BOOL opaque, const u32 slot);

    // The slot's content changed to something without a replacement.
    void Forget(const u32 slot);

    // Turns finished background decodes into textures and hands each one to `apply` for
    // every slot still showing that texture (apply takes its own reference).
    // budgetMs limits the time spent (0 = no limit).
    typedef void (*ApplyFn)(u32 slot, IDirect3DTexture9* texture);
    void Pump(IDirect3DDevice9* device, ApplyFn apply, const f64 budgetMs);

    // Called once per frame (for least-recently-used bookkeeping).
    void Frame(void);

    // Releases every cached replacement (device going away).
    void Release(void);
}
