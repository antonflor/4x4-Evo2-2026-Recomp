/*
evo2026 — texture dumping and HD texture replacement. See EvoTextures.hxx.

HD textures are decoded (PNG/JPEG) and mip-mapped on background worker threads. The
render thread only creates the Direct3D texture (system-memory staging texture ->
UpdateTexture -> video memory), so a track's few hundred HD textures no longer hold
up loading. Until a replacement is ready the game's own texture is shown.
*/
#include "EvoTextures.hxx"
#include "Evo.hxx"
#include "Trace.hxx"
#include "Perf.hxx"
#include "Mathematics.Basic.hxx"
using namespace Mathematics;

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO_DEFAULTS
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define MAX_SLOTS 4096

namespace Evo::Textures
{
    struct CacheEntry
    {
        IDirect3DTexture9* Texture = NULL;  // holds one reference
        u64 Bytes = 0;
        u32 LastUsed = 0;                   // frame number
    };

    struct Job { std::string Key, Path; BOOL Opaque; };

    struct Result
    {
        std::string Key;
        u32 Width = 0, Height = 0, Levels = 0;
        std::vector<u32> Pixels;            // all mip levels, level 0 first (ARGB)
        BOOL Failed = FALSE;
    };

    static struct
    {
        BOOL Initialized = FALSE;
        u32 Dump = 0;
        u32 Replace = 1;
        u32 Async = 1;
        u64 Budget = 3072ull << 20;

        // lower-case file stem -> full path, for every PNG/JPEG in the textures folder
        // (PNG wins if both exist: JPEG is the space-saving form of opaque textures)
        std::unordered_map<std::string, std::string> Files;

        std::unordered_map<std::string, CacheEntry> Cache;
        u64 CacheBytes = 0;
        u32 FrameNumber = 0;

        // keys that have no replacement (or failed), so we don't keep probing
        std::unordered_set<std::string> Missing;

        std::unordered_set<std::string> Dumped;

        // background decoding
        std::unordered_set<std::string> Pending;     // queued or decoding
        std::string SlotKey[MAX_SLOTS];              // what each game texture slot is waiting for
        CRITICAL_SECTION Lock;
        CONDITION_VARIABLE WorkReady, RoomReady;
        std::deque<Job> Jobs;
        std::deque<Result*> Done;
        u32 Workers = 0;
        BOOL Stopping = FALSE;

        u32 Loaded = 0;
    } Store;

    static const size_t MaxDone = 32; // finished decodes waiting for the render thread

    static u32 Crc32(const u8* data, const size_t length)
    {
        static u32 table[256];
        static BOOL ready = FALSE;

        if (!ready)
        {
            for (u32 i = 0; i < 256; i++)
            {
                u32 c = i;
                for (u32 k = 0; k < 8; k++) { c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1); }
                table[i] = c;
            }
            ready = TRUE;
        }

        u32 crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < length; i++) { crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8); }
        return crc ^ 0xFFFFFFFFu;
    }

    static std::string Sanitize(const char* name)
    {
        std::string out;
        for (const char* p = name; *p != 0; p++)
        {
            const char c = *p;
            if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.') { out += (char)tolower((unsigned char)c); }
            else { out += '_'; }
        }
        // drop a trailing image extension the game may include (e.g. "foo.raw")
        const auto dot = out.rfind('.');
        if (dot != std::string::npos && out.size() - dot <= 4) { out.erase(dot); }
        if (out.empty()) { out = "unnamed"; }
        return out;
    }

    static void Scan(const std::string& folder)
    {
        WIN32_FIND_DATAA fd;
        const HANDLE h = FindFirstFileA((folder + "\\*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE) { return; }

        do
        {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) { continue; }

            const std::string path = folder + "\\" + fd.cFileName;

            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) { Scan(path); continue; }

            std::string stem = fd.cFileName;
            const auto dot = stem.rfind('.');
            if (dot == std::string::npos) { continue; }

            std::string ext = stem.substr(dot + 1);
            for (auto& c : ext) { c = (char)tolower((unsigned char)c); }
            const BOOL png = ext == "png";
            if (!png && ext != "jpg" && ext != "jpeg") { continue; }

            stem.erase(dot);
            for (auto& c : stem) { c = (char)tolower((unsigned char)c); }

            // Canvas textures: the game draws different pictures into these (every track's
            // preview goes into "notrack"), so a replacement would hide them all.
            if (stem == "notrack") { continue; }

            if (!png && Store.Files.count(stem) != 0) { continue; }
            Store.Files[stem] = path;
        } while (FindNextFileA(h, &fd));

        FindClose(h);
    }

    // ------------------------------------------------------------ decoding (any thread)

    static Result* Decode(const Job& job)
    {
        Result* r = new Result();
        r->Key = job.Key;

        FILE* f = fopen(job.Path.c_str(), "rb");
        int w = 0, h = 0, n = 0;
        u8* rgba = NULL;
        if (f != NULL) { rgba = stbi_load_from_file(f, &w, &h, &n, 4); fclose(f); }

        if (rgba == NULL || w <= 0 || h <= 0)
        {
            if (rgba != NULL) { stbi_image_free(rgba); }
            r->Failed = TRUE;
            return r;
        }

        // full mip chain, box-filtered
        u32 levels = 1;
        for (u32 lw = (u32)w, lh = (u32)h; lw > 1 || lh > 1; lw = Max<u32>(lw / 2, 1), lh = Max<u32>(lh / 2, 1)) { levels++; }

        size_t total = 0;
        for (u32 l = 0, lw = (u32)w, lh = (u32)h; l < levels; l++, lw = Max<u32>(lw / 2, 1), lh = Max<u32>(lh / 2, 1)) { total += (size_t)lw * lh; }

        r->Pixels.resize(total);
        r->Width = (u32)w; r->Height = (u32)h; r->Levels = levels;

        u32* level = r->Pixels.data();
        for (size_t i = 0; i < (size_t)w * h; i++)
        {
            const u8* p = &rgba[i * 4];
            const u32 a = job.Opaque ? 0xff : p[3];
            level[i] = (a << 24) | ((u32)p[0] << 16) | ((u32)p[1] << 8) | (u32)p[2];
        }
        stbi_image_free(rgba);

        u32 lw = (u32)w, lh = (u32)h;
        for (u32 l = 1; l < levels; l++)
        {
            const u32 nw = Max<u32>(lw / 2, 1), nh = Max<u32>(lh / 2, 1);
            u32* next = level + (size_t)lw * lh;

            for (u32 y = 0; y < nh; y++)
            {
                for (u32 x = 0; x < nw; x++)
                {
                    const u32 x0 = Min<u32>(x * 2, lw - 1), x1 = Min<u32>(x * 2 + 1, lw - 1);
                    const u32 y0 = Min<u32>(y * 2, lh - 1), y1 = Min<u32>(y * 2 + 1, lh - 1);
                    const u32 c[4] = { level[y0 * lw + x0], level[y0 * lw + x1], level[y1 * lw + x0], level[y1 * lw + x1] };
                    u32 out = 0;
                    for (u32 s = 0; s < 32; s += 8)
                    {
                        const u32 v = (((c[0] >> s) & 0xff) + ((c[1] >> s) & 0xff) + ((c[2] >> s) & 0xff) + ((c[3] >> s) & 0xff) + 2) / 4;
                        out |= v << s;
                    }
                    next[y * nw + x] = out;
                }
            }

            level = next; lw = nw; lh = nh;
        }

        return r;
    }

    static DWORD WINAPI Worker(LPVOID)
    {
        for (;;)
        {
            EnterCriticalSection(&Store.Lock);
            while (!Store.Stopping && (Store.Jobs.empty() || Store.Done.size() >= MaxDone))
            {
                SleepConditionVariableCS(Store.Jobs.empty() ? &Store.WorkReady : &Store.RoomReady, &Store.Lock, INFINITE);
            }
            if (Store.Stopping) { LeaveCriticalSection(&Store.Lock); return 0; }
            const Job job = Store.Jobs.front();
            Store.Jobs.pop_front();
            LeaveCriticalSection(&Store.Lock);

            Result* r = Decode(job);

            EnterCriticalSection(&Store.Lock);
            Store.Done.push_back(r);
            LeaveCriticalSection(&Store.Lock);
        }
    }

    static void StartWorkers(void)
    {
        if (Store.Workers != 0) { return; }
        SYSTEM_INFO si; GetSystemInfo(&si);
        u32 n = si.dwNumberOfProcessors > 2 ? si.dwNumberOfProcessors - 1 : 1;
        if (n > 8) { n = 8; }
        for (u32 i = 0; i < n; i++)
        {
            const HANDLE t = CreateThread(NULL, 256 * 1024, Worker, NULL, 0, NULL);
            if (t != NULL) { SetThreadPriority(t, THREAD_PRIORITY_BELOW_NORMAL); CloseHandle(t); Store.Workers++; }
        }
        EVO_TRACE("evo2026 textures: %u decode threads", Store.Workers);
    }

    // ------------------------------------------------------------ setup

    void Initialize(void)
    {
        if (Store.Initialized) { return; }
        Store.Initialized = TRUE;

        Store.Dump = ReadSetting("DumpTextures", 0);
        Store.Replace = ReadSetting("ReplaceTextures", 1);
        Store.Async = ReadSetting("AsyncTextures", 1);
        Store.Budget = (u64)ReadSetting("HdTextureMemory", 3072) << 20;

        InitializeCriticalSection(&Store.Lock);
        InitializeConditionVariable(&Store.WorkReady);
        InitializeConditionVariable(&Store.RoomReady);

        if (Store.Replace) { Scan(".\\textures"); }
        if (Store.Dump) { CreateDirectoryA(".\\textures_dump", NULL); }

        EVO_TRACE("evo2026 textures: dump=%u replace=%u async=%u, %u replacement file(s) found",
            Store.Dump, Store.Replace, Store.Async, (u32)Store.Files.size());
    }

    static void Evict(void)
    {
        if (Store.CacheBytes <= Store.Budget) { return; }

        // drop least recently used entries (the cache only holds its own reference; game
        // texture slots that still show one keep theirs)
        std::vector<std::pair<u32, std::string>> order;
        order.reserve(Store.Cache.size());
        for (auto& it : Store.Cache) { order.push_back({ it.second.LastUsed, it.first }); }
        std::sort(order.begin(), order.end());

        for (auto& o : order)
        {
            if (Store.CacheBytes <= Store.Budget * 3 / 4) { break; }
            if (o.first + 2 >= Store.FrameNumber) { break; } // in use right now
            auto it = Store.Cache.find(o.second);
            Store.CacheBytes -= it->second.Bytes;
            it->second.Texture->Release();
            Store.Cache.erase(it);
        }
    }

    void Release(void)
    {
        if (Store.Initialized)
        {
            EnterCriticalSection(&Store.Lock);
            for (Result* r : Store.Done) { delete r; }
            Store.Done.clear();
            Store.Jobs.clear();
            WakeAllConditionVariable(&Store.RoomReady);
            LeaveCriticalSection(&Store.Lock);
        }
        for (auto& it : Store.Cache) { if (it.second.Texture != NULL) { it.second.Texture->Release(); } }
        Store.Cache.clear();
        Store.CacheBytes = 0;
        Store.Missing.clear();
        Store.Pending.clear();
        for (u32 i = 0; i < MAX_SLOTS; i++) { Store.SlotKey[i].clear(); }
    }

    void Frame(void) { Store.FrameNumber++; }

    static void Dump(const std::string& key, const u32* argb, const u32 dims, const BOOL opaque)
    {
        if (Store.Dumped.count(key) != 0) { return; }
        Store.Dumped.insert(key);

        u8* rgba = (u8*)malloc((size_t)dims * dims * 4);
        if (rgba == NULL) { return; }

        for (u32 i = 0; i < dims * dims; i++)
        {
            const u32 c = argb[i];
            rgba[i * 4 + 0] = (u8)(c >> 16);
            rgba[i * 4 + 1] = (u8)(c >> 8);
            rgba[i * 4 + 2] = (u8)(c);
            rgba[i * 4 + 3] = opaque ? 0xff : (u8)(c >> 24);
        }

        const std::string path = ".\\textures_dump\\" + key + ".png";
        stbi_write_png(path.c_str(), (int)dims, (int)dims, 4, rgba, (int)dims * 4);
        free(rgba);
    }

    // ------------------------------------------------------------ render thread

    // Video-memory texture from a decoded result: fill a system-memory texture, then let
    // the driver copy it over (no managed-pool copy kept in our 32-bit address space).
    static IDirect3DTexture9* Create(IDirect3DDevice9* device, const Result* r)
    {
        IDirect3DTexture9* staging = NULL;
        if (device->CreateTexture(r->Width, r->Height, r->Levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &staging, NULL) != D3D_OK) { return NULL; }

        const u32* src = r->Pixels.data();
        u32 lw = r->Width, lh = r->Height;
        for (u32 l = 0; l < r->Levels; l++)
        {
            D3DLOCKED_RECT rect;
            if (staging->LockRect(l, &rect, NULL, D3DLOCK_NOSYSLOCK) == D3D_OK)
            {
                for (u32 y = 0; y < lh; y++) { memcpy((u8*)rect.pBits + (size_t)y * rect.Pitch, src + (size_t)y * lw, (size_t)lw * 4); }
                staging->UnlockRect(l);
            }
            src += (size_t)lw * lh;
            lw = Max<u32>(lw / 2, 1); lh = Max<u32>(lh / 2, 1);
        }

        IDirect3DTexture9* texture = NULL;
        if (device->CreateTexture(r->Width, r->Height, r->Levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &texture, NULL) != D3D_OK
            || device->UpdateTexture(staging, texture) != D3D_OK)
        {
            if (texture != NULL) { texture->Release(); texture = NULL; }
        }
        staging->Release();
        return texture;
    }

    static IDirect3DTexture9* Finish(IDirect3DDevice9* device, Result* r)
    {
        IDirect3DTexture9* texture = NULL;
        if (!r->Failed) { EVO_PERF(Decode); texture = Create(device, r); }

        Store.Pending.erase(r->Key);
        if (texture == NULL)
        {
            EVO_TRACE("evo2026 textures: could not load %s", r->Key.c_str());
            Store.Missing.insert(r->Key);
            return NULL;
        }

        CacheEntry e;
        e.Texture = texture;
        e.Bytes = (u64)r->Pixels.size() * 4;
        e.LastUsed = Store.FrameNumber;
        Store.Cache[r->Key] = e;
        Store.CacheBytes += e.Bytes;
        Store.Loaded++;
        Perf::Add(Perf::HdLoads);
        return texture;
    }

    void Pump(IDirect3DDevice9* device, ApplyFn apply, const f64 budgetMs)
    {
        if (!Store.Initialized || Store.Workers == 0) { return; }

        const s64 start = Perf::Now();
        LARGE_INTEGER freq; QueryPerformanceFrequency(&freq);

        for (;;)
        {
            EnterCriticalSection(&Store.Lock);
            Result* r = NULL;
            if (!Store.Done.empty()) { r = Store.Done.front(); Store.Done.pop_front(); WakeConditionVariable(&Store.RoomReady); }
            LeaveCriticalSection(&Store.Lock);
            if (r == NULL) { break; }

            IDirect3DTexture9* texture = Finish(device, r);
            if (texture != NULL)
            {
                for (u32 i = 0; i < MAX_SLOTS; i++)
                {
                    if (Store.SlotKey[i] == r->Key) { apply(i, texture); Store.SlotKey[i].clear(); }
                }
            }
            else
            {
                for (u32 i = 0; i < MAX_SLOTS; i++) { if (Store.SlotKey[i] == r->Key) { Store.SlotKey[i].clear(); } }
            }
            delete r;

            if (budgetMs > 0 && (Perf::Now() - start) * 1000.0 / (f64)freq.QuadPart > budgetMs) { break; }
        }

        Evict();
    }

    void Forget(const u32 slot)
    {
        if (slot < MAX_SLOTS) { Store.SlotKey[slot].clear(); }
    }

    IDirect3DTexture9* Process(IDirect3DDevice9* device, const char* name, const u32* argb, const u32 dimensions, const BOOL opaque, const u32 slot)
    {
        Initialize();

        if (slot < MAX_SLOTS) { Store.SlotKey[slot].clear(); }
        if (!Store.Dump && (!Store.Replace || Store.Files.empty())) { return NULL; }
        EVO_PERF(Hd);

        const u32 crc = Crc32((const u8*)argb, (size_t)dimensions * dimensions * 4);
        const std::string base = Sanitize(name);

        char suffix[16];
        snprintf(suffix, sizeof(suffix), "__%08x", crc);
        const std::string key = base + suffix;

        if (Store.Dump) { Dump(key, argb, dimensions, opaque); }

        if (!Store.Replace || Store.Files.empty()) { return NULL; }

        const auto cached = Store.Cache.find(key);
        if (cached != Store.Cache.end())
        {
            Perf::Add(Perf::HdHits);
            cached->second.LastUsed = Store.FrameNumber;
            cached->second.Texture->AddRef();
            return cached->second.Texture;
        }
        if (Store.Missing.count(key) != 0) { return NULL; }

        auto file = Store.Files.find(key);
        if (file == Store.Files.end()) { file = Store.Files.find(base); }
        if (file == Store.Files.end()) { Store.Missing.insert(key); return NULL; }

        const Job job = { key, file->second, opaque };

        if (!Store.Async)
        {
            Result* r;
            { EVO_PERF(Decode); r = Decode(job); }
            Store.Pending.insert(key);
            IDirect3DTexture9* texture = Finish(device, r);
            delete r;
            if (texture == NULL) { return NULL; }
            texture->AddRef();
            return texture;
        }

        StartWorkers();
        if (Store.Workers == 0) { Store.Async = 0; return Process(device, name, argb, dimensions, opaque, slot); }

        if (slot < MAX_SLOTS) { Store.SlotKey[slot] = key; }
        if (Store.Pending.insert(key).second)
        {
            EnterCriticalSection(&Store.Lock);
            Store.Jobs.push_back(job);
            WakeConditionVariable(&Store.WorkReady);
            LeaveCriticalSection(&Store.Lock);
        }
        return NULL;
    }
}
