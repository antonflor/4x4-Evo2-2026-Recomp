/*
Copyright (c) 2023 Americus Maximus

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include "DX.hxx"
#include "Module.hxx"
#include "Trace.hxx"
#include "Evo.hxx"
#include "EvoHooks.hxx"

#define UNKNOWN_DEVICE_NAME "Unknown Device"

using namespace Renderers::Modules;

namespace Renderer::Module
{
    RendererModuleState State;

    extern "C" u32 AcquireModuleName(char* name)
    {
        EVO_TRACE("AcquireModuleName");
        if (name == NULL) { return FALSE; }

        strcpy(name, RENDERER_MODULE_TITLE_NAME);

        return TRUE;
    }

    extern "C" u32 AcquireGraphicsCardList(u32 * count, char** apis, char** names, u32 * vids, u32 * dids)
    {
        EVO_TRACE("AcquireGraphicsCardList count=%u", State.DX.Devices.Available.Count);
        // The original game expects exactly one entry and only reads count/apis/names
        // (the shipped TRIGL.DLL never touches the last two arguments).
        static char api[] = "OpenGL";
        static char name[] = "evo2026 Direct3D 9";
        *count = 1;
        apis[0] = api;
        names[0] = name;
        return TRUE;
    }

    extern "C" void AcquireRendererInformation(const HMODULE module, RendererModuleInitializeInformation * info)
    {
        EVO_TRACE("AcquireRendererInformation");
        ZeroMemory(info, sizeof(RendererModuleInitializeInformation));

        strcpy(info->Name, RENDERER_MODULE_TITLE_NAME);
        strcpy(info->Author, RENDERER_MODULE_AUTHOR);
    }

    extern "C" u32 AcquireVideoMemory(u32 * local, u32 * total, u32 * zero)
    {
        EVO_TRACE("AcquireVideoMemory");
        if (local == NULL || total == NULL || zero == NULL) { return FALSE; }

        *local = MAX_GRAPHICS_CARD_AVAILABLE_MEMORY;
        *total = MAX_GRAPHICS_CARD_AVAILABLE_MEMORY;
        *zero = 0;

        return TRUE;
    }

    extern "C" u32 Init(const HWND hwnd, const RendererModuleInitializeArguments * args)
    {
        // evo2026: hook the game's camera for widescreen as early as possible.
        if (Evo::ReadSetting("Widescreen", 1)) { Evo::Hooks::Install(); }

        EVO_TRACE("Init");
        Kill();

        State.DX.DirectX.Instance = Direct3DCreate9(D3D_SDK_VERSION);

        if (State.DX.DirectX.Instance == NULL) { return FALSE; }

        State.DX.Devices.Available.Count = State.DX.DirectX.Instance->GetAdapterCount();

        for (u32 x = 0; x < State.DX.Devices.Available.Count; x++)
        {
            D3DADAPTER_IDENTIFIER9 identifier;
            ZeroMemory(&identifier, sizeof(D3DADAPTER_IDENTIFIER9));

            auto result = State.DX.DirectX.Instance->GetAdapterIdentifier(x, D3DENUM_WHQL_LEVEL, &identifier);

            State.DX.Devices.Available.Details[x].ID = identifier.DeviceIdentifier;

            ZeroMemory(State.DX.Devices.Available.Details[x].Driver, MAX_DEVICE_IDENTIFIER_STRING);
            ZeroMemory(State.DX.Devices.Available.Details[x].Description, MAX_DEVICE_IDENTIFIER_STRING);

            if (result == D3D_OK)
            {
                State.DX.Devices.Available.Details[x].DeviceID = identifier.DeviceId;
                State.DX.Devices.Available.Details[x].VendorID = identifier.VendorId;
                CopyMemory(State.DX.Devices.Available.Details[x].Driver, identifier.Driver, MAX_DEVICE_IDENTIFIER_STRING);
                CopyMemory(State.DX.Devices.Available.Details[x].Description, identifier.Description, MAX_DEVICE_IDENTIFIER_STRING);
            }
            else
            {
                State.DX.Devices.Available.Details[x].DeviceID = 0;
                State.DX.Devices.Available.Details[x].VendorID = 0;
                CopyMemory(State.DX.Devices.Available.Details[x].Driver, UNKNOWN_DEVICE_NAME, strlen(UNKNOWN_DEVICE_NAME));
                CopyMemory(State.DX.Devices.Available.Details[x].Description, UNKNOWN_DEVICE_NAME, strlen(UNKNOWN_DEVICE_NAME));
            }
        }

        State.Window.HWND = hwnd;

        CopyMemory(&State.InitializeArguments, args, sizeof(RendererModuleInitializeArguments));

        State.DX.DirectX.Instance->Release();
        State.DX.DirectX.Instance = NULL;

        return TRUE;
    }

    extern "C" u32 SelectGraphicsCard(const u32 indx)
    {
        EVO_TRACE("SelectGraphicsCard");
        State.DX.Devices.Current.Index = indx;

        return Init(State.Window.HWND, &State.InitializeArguments);
    }

    extern "C" u32 Kill(void)
    {
        EVO_TRACE("Kill");
        DX::Release();

        if (State.DX.DirectX.Device != NULL)
        {
            State.DX.DirectX.Device->Release();
            State.DX.DirectX.Device = NULL;
        }

        if (State.DX.DirectX.Instance != NULL)
        {
            State.DX.DirectX.Instance->Release();
            State.DX.DirectX.Instance = NULL;
        }

        return TRUE;
    }
}