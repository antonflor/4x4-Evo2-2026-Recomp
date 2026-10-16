/*
evo2026 input bridge for 4x4 Evo 2: a drop-in dinput.dll.

The game reads controllers through DirectInput 7. Modern pads (Xbox, Razer, 8BitDo in
X-input mode) speak XInput; through DirectInput Windows shows them as a generic HID
joystick with both triggers sharing one axis, so you cannot press gas and brake at once.

This DLL sits in the game folder. Keyboard, mouse and any other device (wheels, old
joysticks) go straight to the system's DirectInput. On top of that it adds one virtual
joystick, "XInput Controller (evo2026)", that reads every connected XInput pad, and hides
the pads' own HID view so the game doesn't pick the broken one.

Virtual joystick layout
  X axis   left stick X              Rx axis  right stick X
  Y axis   RT forward / LT back      Ry axis  right stick Y
  Z axis   right trigger (gas)       Rz axis  left trigger (brake)
  Slider   left stick Y
  Hat      d-pad
  Buttons  1 A  2 B  3 X  4 Y  5 LB  6 RB  7 Back  8 Start  9 L-stick  10 R-stick
           11 LT pressed  12 RT pressed

Settings, system\evo2026.ini, section [evo2026]:
  Controller=1          0 turns the bridge off (plain DirectInput)
  HideXInputPads=1      hide the pads' HID joystick view from the game
  StickDeadzone=10      left/right stick dead zone in percent
  ControllerSetup=1     1 = on the next start, switch the game to "Joystick/Analog Gamepad",
                        set the button layout below and assign the pad in control.txt
                        (done once, then set to 0)
  ControllerLog=0       1 = write system\evo2026-input.log

Button layout written by ControllerSetup (change it in the game's Controller Setup)
  A  emergency brake     X  rescue (helicopter)    Y  change view
  LB downshift           RB upshift                Back  look back
  Start  pause menu      L-stick  headlights       R-stick  horn
  In menus: d-pad moves, A selects, B or Start go back (sent as arrows, Enter, Escape;
  in a race the d-pad works like the arrow keys). PadMenuKeys=0 turns that off.
  FakePad=0             testing without a pad: read system\evo2026-fakepad.txt
*/
#define INITGUID
#define DIRECTINPUT_VERSION 0x0700
#include <windows.h>
#include <dinput.h>
#include <xinput.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

#include <vector>
#include <string>

// ---------------------------------------------------------------- settings / log

static const char* IniPath = ".\\system\\evo2026.ini";

static int Setting(const char* name, int value)
{
    return (int)GetPrivateProfileIntA("evo2026", name, value, IniPath);
}

static int LogOn = -1;
static FILE* LogFile = NULL;

static void Log(const char* fmt, ...)
{
    if (LogOn < 0) { LogOn = Setting("ControllerLog", 0); }
    if (!LogOn) { return; }
    static unsigned lines = 0;
    if (++lines > 20000) { return; }
    if (LogFile == NULL) { LogFile = fopen(".\\system\\evo2026-input.log", "w"); if (LogFile == NULL) { LogOn = 0; return; } }
    va_list a; va_start(a, fmt); vfprintf(LogFile, fmt, a); va_end(a);
    fputc('\n', LogFile); fflush(LogFile);
}

static std::string GuidText(REFGUID g)
{
    char b[64];
    snprintf(b, sizeof(b), "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        g.Data1, g.Data2, g.Data3, g.Data4[0], g.Data4[1], g.Data4[2], g.Data4[3], g.Data4[4], g.Data4[5], g.Data4[6], g.Data4[7]);
    return b;
}

// ---------------------------------------------------------------- XInput

typedef DWORD(WINAPI* XInputGetStateFn)(DWORD, XINPUT_STATE*);
typedef DWORD(WINAPI* XInputGetCapabilitiesFn)(DWORD, DWORD, XINPUT_CAPABILITIES*);

static XInputGetStateFn XGetState = NULL;
static XInputGetCapabilitiesFn XGetCaps = NULL;

static void LoadXInput(void)
{
    static BOOL done = FALSE;
    if (done) { return; }
    done = TRUE;

    const char* names[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    for (const char* n : names)
    {
        HMODULE m = LoadLibraryA(n);
        if (m == NULL) { continue; }
        XGetState = (XInputGetStateFn)GetProcAddress(m, "XInputGetState");
        XGetCaps = (XInputGetCapabilitiesFn)GetProcAddress(m, "XInputGetCapabilities");
        if (XGetState != NULL) { Log("xinput: using %s", n); return; }
    }
    Log("xinput: not available");
}

struct Pad
{
    float lx = 0, ly = 0, rx = 0, ry = 0;   // -1..1, y up = +1
    float lt = 0, rt = 0;                   // 0..1
    WORD buttons = 0;                       // XINPUT_GAMEPAD_* bits
    int connected = 0;
};

static float Stick(SHORT v, float dead)
{
    float f = v < 0 ? v / 32768.0f : v / 32767.0f;
    const float a = fabsf(f);
    if (a <= dead) { return 0; }
    const float s = (a - dead) / (1 - dead);
    return f < 0 ? -s : s;
}

static float Trigger(BYTE v)
{
    const float t = 8.0f; // small trigger dead zone
    return v <= t ? 0 : (v - t) / (255.0f - t);
}

static int FakePad = -1;
static CRITICAL_SECTION PadLock;

static Pad ReadPadsUnlocked(void);

// Reads every connected pad (thread-safe wrapper)
static Pad ReadPads(void)
{
    EnterCriticalSection(&PadLock);
    const Pad p = ReadPadsUnlocked();
    LeaveCriticalSection(&PadLock);
    return p;
}

// Reads every connected pad and merges them: the stick/trigger furthest from rest wins,
// buttons are OR-ed, so either controller can drive.
static Pad ReadPadsUnlocked(void)
{
    Pad out;

    if (FakePad < 0) { FakePad = Setting("FakePad", 0); }
    if (FakePad)
    {
        // "lx ly rx ry lt rt buttons(hex)" in -1..1 / 0..1
        FILE* f = fopen(".\\system\\evo2026-fakepad.txt", "r");
        if (f != NULL)
        {
            unsigned b = 0;
            if (fscanf(f, "%f %f %f %f %f %f %x", &out.lx, &out.ly, &out.rx, &out.ry, &out.lt, &out.rt, &b) >= 6) { out.buttons = (WORD)b; out.connected = 1; }
            fclose(f);
        }
        return out;
    }

    LoadXInput();
    if (XGetState == NULL) { return out; }

    static int dead = -1;
    if (dead < 0) { dead = Setting("StickDeadzone", 10); if (dead > 90) { dead = 90; } }
    const float dz = dead / 100.0f;

    static DWORD lastProbe[XUSER_MAX_COUNT] = { 0 };
    static BOOL present[XUSER_MAX_COUNT] = { TRUE, TRUE, TRUE, TRUE };
    static BOOL wasConnected[XUSER_MAX_COUNT] = { 0 };
    const DWORD now = GetTickCount();

    for (DWORD i = 0; i < XUSER_MAX_COUNT; i++)
    {
        // probing an empty slot is slow on some systems: retry empty slots once a second
        if (!present[i] && now - lastProbe[i] < 1000) { continue; }

        XINPUT_STATE st;
        ZeroMemory(&st, sizeof(st));
        const DWORD r = XGetState(i, &st);
        present[i] = r == ERROR_SUCCESS;
        if (!present[i]) { lastProbe[i] = now; }

        if (present[i] != wasConnected[i])
        {
            wasConnected[i] = present[i];
            if (present[i])
            {
                XINPUT_CAPABILITIES c; ZeroMemory(&c, sizeof(c));
                if (XGetCaps != NULL) { XGetCaps(i, 0, &c); }
                Log("pad %lu connected (type %u subtype %u)", i, c.Type, c.SubType);
            }
            else { Log("pad %lu disconnected", i); }
        }
        if (!present[i]) { continue; }

        const XINPUT_GAMEPAD& g = st.Gamepad;
        Pad p;
        p.lx = Stick(g.sThumbLX, dz); p.ly = Stick(g.sThumbLY, dz);
        p.rx = Stick(g.sThumbRX, dz); p.ry = Stick(g.sThumbRY, dz);
        p.lt = Trigger(g.bLeftTrigger); p.rt = Trigger(g.bRightTrigger);

        auto pick = [](float& a, float b) { if (fabsf(b) > fabsf(a)) { a = b; } };
        pick(out.lx, p.lx); pick(out.ly, p.ly); pick(out.rx, p.rx); pick(out.ry, p.ry);
        pick(out.lt, p.lt); pick(out.rt, p.rt);
        out.buttons |= g.wButtons;
        out.connected++;
    }

    return out;
}

// ---------------------------------------------------------------- virtual device description

// {E2026000-4D45-5642-2026-584950414431} "evo2026 XInput pad"
DEFINE_GUID(GUID_Evo2026Pad, 0xE2026000, 0x4D45, 0x5642, 0x20, 0x26, 0x58, 0x49, 0x50, 0x41, 0x44, 0x31);
DEFINE_GUID(GUID_Evo2026PadProduct, 0xE2026001, 0x4D45, 0x5642, 0x20, 0x26, 0x58, 0x49, 0x50, 0x41, 0x44, 0x31);

static const char* PadName = "XInput Controller (evo2026)";

enum Kind { KAxis, KButton, KPov };

struct Obj
{
    Kind kind;
    int index;          // instance number within its kind
    const GUID* guid;
    const char* name;
};

static const Obj Objects[] =
{
    { KAxis, 0, &GUID_XAxis,  "X Axis (left stick)" },
    { KAxis, 1, &GUID_YAxis,  "Y Axis (gas RT, brake LT)" },
    { KAxis, 2, &GUID_ZAxis,  "Z Axis (right trigger)" },
    { KAxis, 3, &GUID_RxAxis, "X Rotation (right stick)" },
    { KAxis, 4, &GUID_RyAxis, "Y Rotation (right stick)" },
    { KAxis, 5, &GUID_RzAxis, "Z Rotation (left trigger)" },
    { KAxis, 6, &GUID_Slider, "Slider (left stick Y)" },
    { KPov,  0, &GUID_POV,    "D-pad" },
    { KButton, 0, &GUID_Button, "A" },
    { KButton, 1, &GUID_Button, "B" },
    { KButton, 2, &GUID_Button, "X" },
    { KButton, 3, &GUID_Button, "Y" },
    { KButton, 4, &GUID_Button, "LB" },
    { KButton, 5, &GUID_Button, "RB" },
    { KButton, 6, &GUID_Button, "Back" },
    { KButton, 7, &GUID_Button, "Start" },
    { KButton, 8, &GUID_Button, "Left stick" },
    { KButton, 9, &GUID_Button, "Right stick" },
    { KButton, 10, &GUID_Button, "LT" },
    { KButton, 11, &GUID_Button, "RT" },
};
static const int ObjectCount = sizeof(Objects) / sizeof(Objects[0]);
static const int AxisCount = 7, ButtonCount = 12, PovCount = 1;

// default offsets = the c_dfDIJoystick layout
static DWORD DefaultOffset(const Obj& o)
{
    if (o.kind == KAxis) { return o.index * 4; }               // lX lY lZ lRx lRy lRz rglSlider[0]
    if (o.kind == KPov) { return 32; }                          // rgdwPOV[0]
    return 48 + o.index;                                        // rgbButtons[i]
}

static DWORD ObjType(const Obj& o)
{
    if (o.kind == KAxis) { return DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE(o.index); }
    if (o.kind == KPov) { return DIDFT_POV | DIDFT_MAKEINSTANCE(o.index); }
    return DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(o.index);
}

static void FillDeviceInstance(LPDIDEVICEINSTANCEA d)
{
    const DWORD size = d->dwSize;
    DIDEVICEINSTANCEA t;
    ZeroMemory(&t, sizeof(t));
    t.dwSize = size;
    t.guidInstance = GUID_Evo2026Pad;
    t.guidProduct = GUID_Evo2026PadProduct;
    t.dwDevType = DIDEVTYPE_JOYSTICK | (DIDEVTYPEJOYSTICK_GAMEPAD << 8) | DIDEVTYPE_HID;
    lstrcpynA(t.tszInstanceName, PadName, MAX_PATH);
    lstrcpynA(t.tszProductName, PadName, MAX_PATH);
    memcpy(d, &t, size < sizeof(t) ? size : sizeof(t));
}

// ---------------------------------------------------------------- virtual device

static void MenuKeys(const Pad& p); // pad buttons -> menu keys, below

class PadDevice : public IDirectInputDevice7A
{
    LONG refs = 1;

    struct Slot { int obj; DWORD ofs; };
    std::vector<Slot> slots;
    DWORD dataSize = 0;

    LONG rmin[AxisCount], rmax[AxisCount];

    BOOL formatSet = FALSE;

    // buffered data (DIPROP_BUFFERSIZE / GetDeviceData)
    DWORD bufferSize = 0;
    std::vector<DIDEVICEOBJECTDATA> queue;
    DWORD seq = 0;
    LONG last[ObjectCount];
    BOOL haveLast = FALSE;

public:
    PadDevice()
    {
        for (int i = 0; i < AxisCount; i++) { rmin[i] = 0; rmax[i] = 65535; }
        for (int i = 0; i < ObjectCount; i++) { slots.push_back({ i, DefaultOffset(Objects[i]) }); }
        dataSize = sizeof(DIJOYSTATE);
    }

    // ------------------------------------------------ helpers

    LONG AxisValue(int axis, float v) // v in -1..1 (sticks) mapped onto [min,max]
    {
        const double lo = rmin[axis], hi = rmax[axis];
        double r = lo + (v + 1.0) * 0.5 * (hi - lo);
        if (r < lo) { r = lo; } if (r > hi) { r = hi; }
        return (LONG)floor(r + 0.5);
    }

    LONG TriggerValue(int axis, float t) // t in 0..1, released = min
    {
        return AxisValue(axis, t * 2 - 1);
    }

    LONG Value(int obj, const Pad& p)
    {
        const Obj& o = Objects[obj];
        if (o.kind == KAxis)
        {
            switch (o.index)
            {
            case 0: return AxisValue(0, p.lx);
            case 1: return AxisValue(1, p.lt - p.rt);    // gas = forward (min), brake = back (max)
            case 2: return TriggerValue(2, p.rt);
            case 3: return AxisValue(3, p.rx);
            case 4: return AxisValue(4, -p.ry);
            case 5: return TriggerValue(5, p.lt);
            case 6: return AxisValue(6, -p.ly);          // DirectInput: up = min
            }
        }
        if (o.kind == KPov)
        {
            const WORD b = p.buttons;
            const int up = (b & XINPUT_GAMEPAD_DPAD_UP) != 0, dn = (b & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
            const int lf = (b & XINPUT_GAMEPAD_DPAD_LEFT) != 0, rt = (b & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
            const int x = rt - lf, y = up - dn;
            if (x == 0 && y == 0) { return -1; }
            static const LONG ang[3][3] = { { 22500, 18000, 13500 }, { 27000, -1, 9000 }, { 31500, 0, 4500 } }; // [y+1][x+1]
            return ang[y + 1][x + 1];
        }
        static const WORD map[10] = { XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
            XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER, XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_START,
            XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB };
        if (o.index < 10) { return (p.buttons & map[o.index]) ? 0x80 : 0; }
        if (o.index == 10) { return p.lt > 0.3f ? 0x80 : 0; }
        return p.rt > 0.3f ? 0x80 : 0;
    }

    int FindObject(DWORD how, DWORD what)
    {
        if (how == DIPH_BYOFFSET)
        {
            for (auto& s : slots) { if (s.ofs == what) { return s.obj; } }
            return -1;
        }
        if (how == DIPH_BYID)
        {
            for (int i = 0; i < ObjectCount; i++)
            {
                const DWORD t = ObjType(Objects[i]);
                if ((t & 0xffffff) == (what & 0xffffff) || (DIDFT_GETINSTANCE(t) == DIDFT_GETINSTANCE(what) && (DIDFT_GETTYPE(t) & DIDFT_GETTYPE(what)))) { return i; }
            }
        }
        return -1;
    }

    void Capture(void)
    {
        if (bufferSize == 0) { return; }
        const Pad p = ReadPads();
        for (auto& s : slots)
        {
            const LONG v = Value(s.obj, p);
            if (haveLast && last[s.obj] == v) { continue; }
            if (haveLast)
            {
                DIDEVICEOBJECTDATA d; ZeroMemory(&d, sizeof(d));
                d.dwOfs = s.ofs; d.dwData = (DWORD)v; d.dwTimeStamp = GetTickCount(); d.dwSequence = ++seq;
                if (queue.size() >= bufferSize) { queue.erase(queue.begin()); }
                queue.push_back(d);
            }
            last[s.obj] = v;
        }
        haveLast = TRUE;
    }

    // ------------------------------------------------ IUnknown

    STDMETHOD(QueryInterface)(REFIID riid, LPVOID* out)
    {
        if (out == NULL) { return E_POINTER; }
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IDirectInputDeviceA) || IsEqualGUID(riid, IID_IDirectInputDevice2A) || IsEqualGUID(riid, IID_IDirectInputDevice7A))
        {
            *out = this; AddRef(); return S_OK;
        }
        Log("pad: QueryInterface %s -> no", GuidText(riid).c_str());
        *out = NULL; return E_NOINTERFACE;
    }
    STDMETHOD_(ULONG, AddRef)() { return (ULONG)InterlockedIncrement(&refs); }
    STDMETHOD_(ULONG, Release)() { const LONG r = InterlockedDecrement(&refs); if (r == 0) { delete this; } return (ULONG)r; }

    // ------------------------------------------------ IDirectInputDeviceA

    STDMETHOD(GetCapabilities)(LPDIDEVCAPS c)
    {
        if (c == NULL) { return E_POINTER; }
        const DWORD size = c->dwSize;
        DIDEVCAPS t; ZeroMemory(&t, sizeof(t));
        t.dwSize = size;
        t.dwFlags = DIDC_ATTACHED;
        t.dwDevType = DIDEVTYPE_JOYSTICK | (DIDEVTYPEJOYSTICK_GAMEPAD << 8) | DIDEVTYPE_HID;
        t.dwAxes = AxisCount; t.dwButtons = ButtonCount; t.dwPOVs = PovCount;
        memcpy(c, &t, size < sizeof(t) ? size : sizeof(t));
        Log("pad: GetCapabilities");
        return DI_OK;
    }

    STDMETHOD(EnumObjects)(LPDIENUMDEVICEOBJECTSCALLBACKA cb, LPVOID ref, DWORD flags)
    {
        Log("pad: EnumObjects flags=%lx", flags);
        for (auto& s : slots)
        {
            const Obj& o = Objects[s.obj];
            const DWORD type = ObjType(o);
            if (flags != DIDFT_ALL)
            {
                const DWORD want = flags & 0xff;
                if (want != 0 && (DIDFT_GETTYPE(type) & want) == 0) { continue; }
            }
            DIDEVICEOBJECTINSTANCEA d; ZeroMemory(&d, sizeof(d));
            d.dwSize = sizeof(d);
            d.guidType = *o.guid;
            d.dwOfs = s.ofs;
            d.dwType = type;
            d.dwFlags = o.kind == KAxis ? DIDOI_ASPECTPOSITION : 0;
            lstrcpynA(d.tszName, o.name, MAX_PATH);
            if (cb(&d, ref) == DIENUM_STOP) { break; }
        }
        return DI_OK;
    }

    STDMETHOD(GetProperty)(REFGUID prop, LPDIPROPHEADER h)
    {
        const DWORD id = (DWORD)(ULONG_PTR)&prop;
        if (h == NULL) { return E_POINTER; }
        if (id == (DWORD)(ULONG_PTR)&DIPROP_RANGE)
        {
            const int obj = h->dwHow == DIPH_DEVICE ? 0 : FindObject(h->dwHow, h->dwObj);
            if (obj < 0 || Objects[obj].kind != KAxis) { return DIERR_OBJECTNOTFOUND; }
            LPDIPROPRANGE r = (LPDIPROPRANGE)h; r->lMin = rmin[Objects[obj].index]; r->lMax = rmax[Objects[obj].index];
            return DI_OK;
        }
        if (id == (DWORD)(ULONG_PTR)&DIPROP_BUFFERSIZE) { ((LPDIPROPDWORD)h)->dwData = bufferSize; return DI_OK; }
        if (id == (DWORD)(ULONG_PTR)&DIPROP_DEADZONE || id == (DWORD)(ULONG_PTR)&DIPROP_SATURATION)
        {
            ((LPDIPROPDWORD)h)->dwData = id == (DWORD)(ULONG_PTR)&DIPROP_DEADZONE ? 0 : 10000; return DI_OK;
        }
        if (id == (DWORD)(ULONG_PTR)&DIPROP_GRANULARITY) { ((LPDIPROPDWORD)h)->dwData = 1; return DI_OK; }
        if (id == (DWORD)(ULONG_PTR)&DIPROP_AXISMODE) { ((LPDIPROPDWORD)h)->dwData = DIPROPAXISMODE_ABS; return DI_OK; }
        Log("pad: GetProperty %lu (unsupported)", id);
        return DIERR_UNSUPPORTED;
    }

    STDMETHOD(SetProperty)(REFGUID prop, LPCDIPROPHEADER h)
    {
        const DWORD id = (DWORD)(ULONG_PTR)&prop;
        if (h == NULL) { return E_POINTER; }
        if (id == (DWORD)(ULONG_PTR)&DIPROP_RANGE)
        {
            const LPCDIPROPRANGE r = (LPCDIPROPRANGE)h;
            Log("pad: SetProperty RANGE how=%lu obj=%lx %ld..%ld", h->dwHow, h->dwObj, r->lMin, r->lMax);
            if (h->dwHow == DIPH_DEVICE) { for (int i = 0; i < AxisCount; i++) { rmin[i] = r->lMin; rmax[i] = r->lMax; } return DI_OK; }
            const int obj = FindObject(h->dwHow, h->dwObj);
            if (obj < 0 || Objects[obj].kind != KAxis) { return DIERR_OBJECTNOTFOUND; }
            rmin[Objects[obj].index] = r->lMin; rmax[Objects[obj].index] = r->lMax;
            return DI_OK;
        }
        if (id == (DWORD)(ULONG_PTR)&DIPROP_BUFFERSIZE)
        {
            bufferSize = ((LPCDIPROPDWORD)h)->dwData; Log("pad: SetProperty BUFFERSIZE %lu", bufferSize); return DI_OK;
        }
        Log("pad: SetProperty %lu how=%lu obj=%lx data=%lu (accepted)", id, h->dwHow, h->dwObj, h->dwSize >= sizeof(DIPROPDWORD) ? ((LPCDIPROPDWORD)h)->dwData : 0);
        return DI_OK;
    }

    STDMETHOD(Acquire)() { return DI_OK; }
    STDMETHOD(Unacquire)() { return DI_OK; }

    STDMETHOD(GetDeviceState)(DWORD size, LPVOID data)
    {
        if (data == NULL) { return E_POINTER; }
        static unsigned calls = 0;
        if (calls++ < 3) { Log("pad: GetDeviceState size=%lu", size); }

        ZeroMemory(data, size);
        const Pad p = ReadPads();
        for (auto& s : slots)
        {
            const Obj& o = Objects[s.obj];
            const LONG v = Value(s.obj, p);
            if (o.kind == KButton) { if (s.ofs < size) { ((BYTE*)data)[s.ofs] = (BYTE)v; } }
            else if (s.ofs + 4 <= size) { *(LONG*)((BYTE*)data + s.ofs) = v; }
        }

        static DWORD polls = 0, lastReport = 0;
        polls++;
        if (GetTickCount() - lastReport >= 2000)
        {
            lastReport = GetTickCount();
            Log("poll #%lu: lx=%.2f ly=%.2f lt=%.2f rt=%.2f buttons=%04x", polls, p.lx, p.ly, p.lt, p.rt, p.buttons);
        }

        static WORD lastButtons = 0xffff; static int lastConn = -1;
        if (p.buttons != lastButtons || p.connected != lastConn)
        {
            Log("state: pads=%d buttons=%04x lx=%.2f ly=%.2f lt=%.2f rt=%.2f", p.connected, p.buttons, p.lx, p.ly, p.lt, p.rt);
            lastButtons = p.buttons; lastConn = p.connected;
        }
        return DI_OK;
    }

    STDMETHOD(GetDeviceData)(DWORD size, LPDIDEVICEOBJECTDATA out, LPDWORD count, DWORD flags)
    {
        if (count == NULL) { return E_POINTER; }
        Capture();
        const DWORD want = *count;
        DWORD n = 0;
        if (out == NULL && want == INFINITE) // flush / count
        {
            n = (DWORD)queue.size();
            if (!(flags & DIGDD_PEEK)) { queue.clear(); }
            *count = n; return DI_OK;
        }
        while (n < want && n < queue.size())
        {
            if (out != NULL) { memcpy((BYTE*)out + n * size, &queue[n], size < sizeof(DIDEVICEOBJECTDATA) ? size : sizeof(DIDEVICEOBJECTDATA)); }
            n++;
        }
        if (!(flags & DIGDD_PEEK)) { queue.erase(queue.begin(), queue.begin() + n); }
        *count = n;
        return DI_OK;
    }

    STDMETHOD(SetDataFormat)(LPCDIDATAFORMAT f)
    {
        if (f == NULL) { return E_POINTER; }
        Log("pad: SetDataFormat size=%lu objects=%lu flags=%lx", f->dwDataSize, f->dwNumObjs, f->dwFlags);
        if (f->dwFlags & DIDF_ABSAXIS) { /* fine */ }

        slots.clear();
        BOOL used[ObjectCount] = { 0 };
        for (DWORD i = 0; i < f->dwNumObjs; i++)
        {
            const DIOBJECTDATAFORMAT& d = f->rgodf[i];
            const DWORD type = DIDFT_GETTYPE(d.dwType);
            const DWORD inst = DIDFT_GETINSTANCE(d.dwType);
            const BOOL any = (d.dwType & DIDFT_ANYINSTANCE) == DIDFT_ANYINSTANCE;
            int found = -1;
            for (int k = 0; k < ObjectCount && found < 0; k++)
            {
                if (used[k]) { continue; }
                const Obj& o = Objects[k];
                const DWORD ot = DIDFT_GETTYPE(ObjType(o));
                if ((ot & type) == 0) { continue; }
                if (d.pguid != NULL && !IsEqualGUID(*d.pguid, *o.guid)) { continue; }
                if (!any)
                {
                    // instance numbers count within the object's kind for buttons/POVs;
                    // for axes with a guid, the instance is within that guid type (always 0 here)
                    const int want = (int)inst;
                    if (d.pguid == NULL && o.index != want) { continue; }
                    if (d.pguid != NULL && o.kind != KAxis && o.index != want) { continue; }
                    if (d.pguid != NULL && o.kind == KAxis && want != 0) { continue; }
                }
                found = k;
            }
            Log("  fmt[%lu] ofs=%lu type=%lx guid=%s -> %s", i, d.dwOfs, d.dwType,
                d.pguid ? GuidText(*d.pguid).c_str() : "-", found >= 0 ? Objects[found].name : "(none)");
            if (found >= 0) { used[found] = TRUE; slots.push_back({ found, d.dwOfs }); }
        }
        dataSize = f->dwDataSize;
        formatSet = TRUE;
        haveLast = FALSE; queue.clear();
        return DI_OK;
    }

    STDMETHOD(SetEventNotification)(HANDLE) { return DI_OK; }
    STDMETHOD(SetCooperativeLevel)(HWND, DWORD flags) { Log("pad: SetCooperativeLevel %lx", flags); return DI_OK; }

    STDMETHOD(GetObjectInfo)(LPDIDEVICEOBJECTINSTANCEA d, DWORD obj, DWORD how)
    {
        if (d == NULL) { return E_POINTER; }
        const int k = FindObject(how, obj);
        if (k < 0) { return DIERR_OBJECTNOTFOUND; }
        const DWORD size = d->dwSize;
        DIDEVICEOBJECTINSTANCEA t; ZeroMemory(&t, sizeof(t));
        t.dwSize = size; t.guidType = *Objects[k].guid; t.dwType = ObjType(Objects[k]);
        for (auto& s : slots) { if (s.obj == k) { t.dwOfs = s.ofs; } }
        lstrcpynA(t.tszName, Objects[k].name, MAX_PATH);
        memcpy(d, &t, size < sizeof(t) ? size : sizeof(t));
        return DI_OK;
    }

    STDMETHOD(GetDeviceInfo)(LPDIDEVICEINSTANCEA d) { if (d == NULL) { return E_POINTER; } FillDeviceInstance(d); return DI_OK; }
    STDMETHOD(RunControlPanel)(HWND, DWORD) { return DI_OK; }
    STDMETHOD(Initialize)(HINSTANCE, DWORD, REFGUID) { return DI_OK; }

    // ------------------------------------------------ IDirectInputDevice2A

    STDMETHOD(CreateEffect)(REFGUID g, LPCDIEFFECT, LPDIRECTINPUTEFFECT* e, LPUNKNOWN)
    {
        Log("pad: CreateEffect %s (no force feedback)", GuidText(g).c_str());
        if (e != NULL) { *e = NULL; }
        return DIERR_UNSUPPORTED;
    }
    STDMETHOD(EnumEffects)(LPDIENUMEFFECTSCALLBACKA, LPVOID, DWORD) { return DI_OK; }
    STDMETHOD(GetEffectInfo)(LPDIEFFECTINFOA, REFGUID) { return DIERR_UNSUPPORTED; }
    STDMETHOD(GetForceFeedbackState)(LPDWORD s) { if (s) { *s = DIGFFS_EMPTY | DIGFFS_STOPPED; } return DIERR_UNSUPPORTED; }
    STDMETHOD(SendForceFeedbackCommand)(DWORD) { return DIERR_UNSUPPORTED; }
    STDMETHOD(EnumCreatedEffectObjects)(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK, LPVOID, DWORD) { return DI_OK; }
    STDMETHOD(Escape)(LPDIEFFESCAPE) { return DIERR_UNSUPPORTED; }
    STDMETHOD(Poll)() { return DI_NOEFFECT; }
    STDMETHOD(SendDeviceData)(DWORD, LPCDIDEVICEOBJECTDATA, LPDWORD, DWORD) { return DIERR_UNSUPPORTED; }

    // ------------------------------------------------ IDirectInputDevice7A

    STDMETHOD(EnumEffectsInFile)(LPCSTR, LPDIENUMEFFECTSINFILECALLBACK, LPVOID, DWORD) { return DIERR_UNSUPPORTED; }
    STDMETHOD(WriteEffectToFile)(LPCSTR, DWORD, LPDIFILEEFFECT, DWORD) { return DIERR_UNSUPPORTED; }
};


// ---------------------------------------------------------------- keyboard: pad -> menu keys

// The game's menus only read the keyboard. The pad's d-pad, A and B are added to the
// keyboard as the arrow keys, Enter and Escape (B and Start), so the menus work from the couch.
// (In a race the d-pad then acts like the arrow keys too: digital steering, gas and brake.)
struct PadKey { WORD button; BYTE dik; };
static const PadKey PadKeys[] =
{
    { XINPUT_GAMEPAD_DPAD_UP, DIK_UP }, { XINPUT_GAMEPAD_DPAD_DOWN, DIK_DOWN },
    { XINPUT_GAMEPAD_DPAD_LEFT, DIK_LEFT }, { XINPUT_GAMEPAD_DPAD_RIGHT, DIK_RIGHT },
    { XINPUT_GAMEPAD_A, DIK_RETURN }, { XINPUT_GAMEPAD_B, DIK_ESCAPE }, { XINPUT_GAMEPAD_START, DIK_ESCAPE },
};
static const int PadKeyCount = sizeof(PadKeys) / sizeof(PadKeys[0]);

// Keys are pressed with SendInput, and only while one of the game's windows is in front.
static void MenuKeys(const Pad& p)
{
    static int on = -1;
    if (on < 0) { on = Setting("PadMenuKeys", 1); }
    if (!on) { return; }

    static WORD held = 0;
    WORD now = 0;
    for (int i = 0; i < PadKeyCount; i++) { if (p.buttons & PadKeys[i].button) { now |= PadKeys[i].button; } }

    // release everything if the game isn't in front
    DWORD pid = 0;
    const HWND fg = GetForegroundWindow();
    if (fg != NULL) { GetWindowThreadProcessId(fg, &pid); }
    if (pid != GetCurrentProcessId())
    {
        static WORD warned = 0;
        if (now != 0 && now != warned) { Log("menu keys: game not in front (foreground pid %lu), %04x ignored", pid, now); }
        warned = now;
        now = 0;
    }
    if (now == held) { return; }

    for (int i = 0; i < PadKeyCount; i++)
    {
        const WORD b = PadKeys[i].button;
        if ((now & b) == (held & b)) { continue; }
        const BYTE dik = PadKeys[i].dik;
        INPUT in; ZeroMemory(&in, sizeof(in));
        in.type = INPUT_KEYBOARD;
        in.ki.wScan = dik & 0x7f;
        in.ki.wVk = (WORD)MapVirtualKeyA(dik & 0x7f, 1 /* MAPVK_VSC_TO_VK */);
        in.ki.dwFlags = KEYEVENTF_SCANCODE | ((dik & 0x80) ? KEYEVENTF_EXTENDEDKEY : 0) | ((now & b) ? 0 : KEYEVENTF_KEYUP);
        if (dik == DIK_UP) { in.ki.wVk = VK_UP; } else if (dik == DIK_DOWN) { in.ki.wVk = VK_DOWN; }
        else if (dik == DIK_LEFT) { in.ki.wVk = VK_LEFT; } else if (dik == DIK_RIGHT) { in.ki.wVk = VK_RIGHT; }
        SendInput(1, &in, sizeof(in));
        Log("menu key %02x %s", dik, (now & b) ? "down" : "up");
    }
    held = now;
}

// The game stops polling the joystick in menus and while paused, so the menu keys come
// from a small thread of our own (60 times a second).
static DWORD WINAPI MenuKeyThread(LPVOID)
{
    for (;;)
    {
        MenuKeys(ReadPads());
        Sleep(16);
    }
    return 0;
}

static void StartMenuKeys(void)
{
    static BOOL started = FALSE;
    if (started || Setting("Controller", 1) == 0 || Setting("PadMenuKeys", 1) == 0) { return; }
    started = TRUE;
    CloseHandle(CreateThread(NULL, 0, MenuKeyThread, NULL, 0, NULL));
    Log("menu keys: thread started");
}

// ---------------------------------------------------------------- hiding the pads' own HID view

// XInput pads have "IG_" in their raw-input device path; collect their VID/PID so the
// matching DirectInput joysticks (guidProduct.Data1 = PID<<16 | VID) can be hidden.
static std::vector<DWORD> XInputProducts(void)
{
    std::vector<DWORD> out;
    UINT n = 0;
    if (GetRawInputDeviceList(NULL, &n, sizeof(RAWINPUTDEVICELIST)) != 0 || n == 0) { return out; }
    std::vector<RAWINPUTDEVICELIST> list(n);
    if (GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST)) == (UINT)-1) { return out; }
    for (UINT i = 0; i < n; i++)
    {
        char name[512]; UINT len = sizeof(name);
        if (GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICENAME, name, &len) == (UINT)-1) { continue; }
        name[sizeof(name) - 1] = 0;
        for (char* c = name; *c; c++) { if (*c >= 'a' && *c <= 'z') { *c -= 32; } }
        if (strstr(name, "IG_") == NULL) { continue; }
        const char* v = strstr(name, "VID_"); const char* p = strstr(name, "PID_");
        if (v == NULL || p == NULL) { continue; }
        const DWORD vid = strtoul(v + 4, NULL, 16) & 0xffff, pid = strtoul(p + 4, NULL, 16) & 0xffff;
        out.push_back((pid << 16) | vid);
        Log("xinput pad on HID: VID_%04lX PID_%04lX", vid, pid);
    }
    return out;
}

// ---------------------------------------------------------------- IDirectInput wrapper

class Input : public IDirectInput7A
{
    LONG refs = 1;
    IDirectInputA* real;          // always valid
    IDirectInput2A* real2 = NULL; // when available
    IDirectInput7A* real7 = NULL; // when available
    BOOL bridge;
    BOOL hide;
    std::vector<DWORD> hidden;

public:
    Input(IDirectInputA* r) : real(r)
    {
        real->QueryInterface(IID_IDirectInput2A, (void**)&real2);
        real->QueryInterface(IID_IDirectInput7A, (void**)&real7);
        bridge = Setting("Controller", 1) != 0;
        hide = bridge && Setting("HideXInputPads", 1) != 0;
        if (hide) { hidden = XInputProducts(); }
        Log("bridge=%d hide=%d", bridge, hide);
    }
    ~Input()
    {
        if (real7) { real7->Release(); }
        if (real2) { real2->Release(); }
        real->Release();
    }

    STDMETHOD(QueryInterface)(REFIID riid, LPVOID* out)
    {
        if (out == NULL) { return E_POINTER; }
        if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IDirectInputA)
            || (IsEqualGUID(riid, IID_IDirectInput2A) && real2) || (IsEqualGUID(riid, IID_IDirectInput7A) && real7))
        {
            *out = this; AddRef(); return S_OK;
        }
        Log("QueryInterface %s -> real", GuidText(riid).c_str());
        return real->QueryInterface(riid, out);
    }
    STDMETHOD_(ULONG, AddRef)() { return (ULONG)InterlockedIncrement(&refs); }
    STDMETHOD_(ULONG, Release)() { const LONG r = InterlockedDecrement(&refs); if (r == 0) { delete this; } return (ULONG)r; }

    STDMETHOD(CreateDevice)(REFGUID g, LPDIRECTINPUTDEVICEA* out, LPUNKNOWN outer)
    {
        Log("CreateDevice %s", GuidText(g).c_str());
        if (bridge && IsEqualGUID(g, GUID_Evo2026Pad)) { *out = (LPDIRECTINPUTDEVICEA)new PadDevice(); return DI_OK; }
        return real->CreateDevice(g, out, outer);
    }

    struct EnumCtx { Input* self; LPDIENUMDEVICESCALLBACKA cb; LPVOID ref; BOOL stopped; };

    static BOOL CALLBACK Filter(LPCDIDEVICEINSTANCEA d, LPVOID p)
    {
        EnumCtx* c = (EnumCtx*)p;
        const DWORD type = GET_DIDEVICE_TYPE(d->dwDevType);
        Log("enum: %s type=%lx product=%08lX", d->tszInstanceName, d->dwDevType, d->guidProduct.Data1);
        if (c->self->hide && type == DIDEVTYPE_JOYSTICK)
        {
            for (DWORD id : c->self->hidden) { if (id == d->guidProduct.Data1) { Log("  hidden (XInput pad)"); return DIENUM_CONTINUE; } }
        }
        if (c->cb(d, c->ref) == DIENUM_STOP) { c->stopped = TRUE; return DIENUM_STOP; }
        return DIENUM_CONTINUE;
    }

    STDMETHOD(EnumDevices)(DWORD type, LPDIENUMDEVICESCALLBACKA cb, LPVOID ref, DWORD flags)
    {
        Log("EnumDevices type=%lu flags=%lx", type, flags);
        EnumCtx c = { this, cb, ref, FALSE };

        // our pad first, so a game that takes "the first joystick" gets it
        if (bridge && (type == 0 || type == DIDEVTYPE_JOYSTICK))
        {
            DIDEVICEINSTANCEA d; ZeroMemory(&d, sizeof(d)); d.dwSize = sizeof(d);
            FillDeviceInstance(&d);
            Log("enum: %s (virtual)", d.tszInstanceName);
            if (cb(&d, ref) == DIENUM_STOP) { return DI_OK; }
        }
        return real->EnumDevices(type, Filter, &c, flags);
    }

    STDMETHOD(GetDeviceStatus)(REFGUID g)
    {
        if (bridge && IsEqualGUID(g, GUID_Evo2026Pad)) { return DI_OK; }
        return real->GetDeviceStatus(g);
    }
    STDMETHOD(RunControlPanel)(HWND w, DWORD f) { return real->RunControlPanel(w, f); }
    STDMETHOD(Initialize)(HINSTANCE h, DWORD v) { return real->Initialize(h, v); }

    STDMETHOD(FindDevice)(REFGUID g, LPCSTR name, LPGUID out)
    {
        return real2 ? real2->FindDevice(g, name, out) : DIERR_UNSUPPORTED;
    }

    STDMETHOD(CreateDeviceEx)(REFGUID g, REFIID iid, LPVOID* out, LPUNKNOWN outer)
    {
        Log("CreateDeviceEx %s", GuidText(g).c_str());
        if (bridge && IsEqualGUID(g, GUID_Evo2026Pad))
        {
            PadDevice* d = new PadDevice();
            const HRESULT r = d->QueryInterface(iid, out);
            d->Release();
            return r;
        }
        return real7 ? real7->CreateDeviceEx(g, iid, out, outer) : DIERR_UNSUPPORTED;
    }
};


// ---------------------------------------------------------------- one-time game setup

static const char* PadDeviceKey = "XInput Controller (evo2026) (E2026000-4D45-5642-20-26-58-49-50-41-44-31)";

// game function codes for [Control] buttonFunctionN in system\METAL.INI
enum { FnUpShift = 8, FnDownShift = 9, FnNextView = 11, FnHorn = 22, FnRescue = 23, FnPause = 27, FnEscape = 28,
       FnHeadlights = 31, FnEBrake = 32, FnDriveMode = 33, FnWinch = 43, FnLookBack = 44, FnNone = 45 };

static void SetupMetalIni(void)
{
    const char* ini = ".\\system\\METAL.INI";
    const int layout[12] = { FnEBrake, FnNone, FnRescue, FnNextView, FnDownShift, FnUpShift,
                             FnLookBack, FnNone, FnHeadlights, FnHorn, FnNone, FnNone }; // Start and B: Escape key
    WritePrivateProfileStringA("Control", "controllerType", "1", ini);
    char key[32], val[16];
    for (int i = 0; i < 32; i++)
    {
        snprintf(key, sizeof(key), "buttonFunction%d", i);
        snprintf(val, sizeof(val), "%d", i < 12 ? layout[i] : FnNone);
        WritePrivateProfileStringA("Control", key, val, ini);
    }
}

static std::string CompatSection(const char* nl)
{
    const char* axes[4] = { "X Axis (left stick)", "Y Axis (gas RT, brake LT)", NULL, NULL };
    const char* buttons[10] = { "A", "B", "X", "Y", "LB", "RB", "Back", "Start", "Left stick", "Right stick" };
    std::string s = std::string("Begin Compatibility Settings") + nl;
    auto entry = [&](const char* kind, int i, const char* el)
    {
        char head[32]; snprintf(head, sizeof(head), "%s %d", kind, i);
        s += head; s += nl;
        if (el == NULL) { s += std::string("  Interface: undefined") + nl + "  Device: undefined" + nl + "  Element: undefined" + nl; return; }
        s += std::string("  Interface: DirectInput") + nl + "  Device: " + PadDeviceKey + nl + "  Element: " + el + nl;
    };
    for (int i = 0; i < 4; i++) { entry("Axis", i, axes[i]); }
    entry("Hat", 0, NULL); // the d-pad is sent as arrow keys instead
    for (int i = 0; i < 32; i++) { entry("Button", i, i < 10 ? buttons[i] : NULL); }
    s += std::string("End Compatibility Settings") + nl;
    return s;
}

// Assign the pad in control.txt unless the player already assigned axes to something.
static void SetupControlTxt(void)
{
    const char* path = ".\\control.txt";
    std::string text;
    FILE* f = fopen(path, "rb");
    if (f != NULL)
    {
        char buf[4096]; size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) { text.append(buf, n); }
        fclose(f);
    }
    const char* nl = text.find("\r\n") != std::string::npos || text.empty() ? "\r\n" : "\n";

    const size_t a = text.find("Begin Compatibility Settings");
    const size_t b = text.find("End Compatibility Settings");
    if (a != std::string::npos && b != std::string::npos && b > a)
    {
        const std::string old = text.substr(a, b - a);
        // any axis already assigned (by the player or an earlier setup)? leave it alone
        const size_t hat = old.find("Hat 0");
        const std::string axes = old.substr(0, hat == std::string::npos ? old.size() : hat);
        for (size_t at = axes.find("Interface:"); at != std::string::npos; at = axes.find("Interface:", at + 1))
        {
            if (axes.compare(at, strlen("Interface: undefined"), "Interface: undefined") != 0) { Log("setup: control.txt already has an axis assignment, kept"); return; }
        }
        size_t end = b + strlen("End Compatibility Settings");
        if (text.compare(end, strlen(nl), nl) == 0) { end += strlen(nl); }
        text = text.substr(0, a) + CompatSection(nl) + text.substr(end);
    }
    else if (text.empty())
    {
        text = std::string("Begin Controller Settings") + nl + "Version: 65536" + nl + nl
            + "*** Begin Interface" + nl + "Name: DirectInput" + nl + "Version: 65536" + nl + nl
            + "--- End Interface (DirectInput)" + nl + nl + nl + CompatSection(nl) + "End Controller Settings" + nl;
    }
    else { Log("setup: control.txt has an unexpected layout, kept"); return; }

    f = fopen(path, "wb");
    if (f == NULL) { Log("setup: cannot write control.txt"); return; }
    fwrite(text.data(), 1, text.size(), f);
    fclose(f);
    Log("setup: control.txt pad assignment written");
}

static void FirstRunSetup(void)
{
    if (Setting("Controller", 1) == 0 || Setting("ControllerSetup", 1) == 0) { return; }
    SetupMetalIni();
    SetupControlTxt();
    WritePrivateProfileStringA("evo2026", "ControllerSetup", "0", IniPath);
    Log("setup: gamepad mode and button layout set");
}

// ---------------------------------------------------------------- exports

static HMODULE Real = NULL;

static FARPROC RealProc(const char* name)
{
    if (Real == NULL)
    {
        char path[MAX_PATH];
        GetSystemDirectoryA(path, MAX_PATH);
        lstrcatA(path, "\\dinput.dll");
        Real = LoadLibraryA(path);
        Log("system dinput: %s (%s)", path, Real ? "loaded" : "FAILED");
    }
    return Real ? GetProcAddress(Real, name) : NULL;
}

typedef HRESULT(WINAPI* CreateAFn)(HINSTANCE, DWORD, LPDIRECTINPUTA*, LPUNKNOWN);
typedef HRESULT(WINAPI* CreateWFn)(HINSTANCE, DWORD, LPDIRECTINPUTW*, LPUNKNOWN);
typedef HRESULT(WINAPI* CreateExFn)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInputCreateA(HINSTANCE h, DWORD version, LPDIRECTINPUTA* out, LPUNKNOWN outer)
{
    Log("DirectInputCreateA version=%lx", version);
    StartMenuKeys();
    CreateAFn f = (CreateAFn)RealProc("DirectInputCreateA");
    if (f == NULL) { return DIERR_GENERIC; }
    LPDIRECTINPUTA r = NULL;
    const HRESULT hr = f(h, version, &r, outer);
    if (FAILED(hr) || r == NULL) { Log("  real failed %lx", hr); return hr; }
    *out = (LPDIRECTINPUTA) new Input(r);
    return hr;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInputCreateW(HINSTANCE h, DWORD v, LPDIRECTINPUTW* out, LPUNKNOWN outer)
{
    CreateWFn f = (CreateWFn)RealProc("DirectInputCreateW");
    return f ? f(h, v, out, outer) : DIERR_GENERIC;
}

extern "C" __declspec(dllexport) HRESULT WINAPI DirectInputCreateEx(HINSTANCE h, DWORD v, REFIID iid, LPVOID* out, LPUNKNOWN outer)
{
    Log("DirectInputCreateEx version=%lx", v);
    CreateExFn f = (CreateExFn)RealProc("DirectInputCreateEx");
    if (f == NULL) { return DIERR_GENERIC; }
    if (IsEqualGUID(iid, IID_IDirectInputA) || IsEqualGUID(iid, IID_IDirectInput2A) || IsEqualGUID(iid, IID_IDirectInput7A))
    {
        LPDIRECTINPUTA r = NULL;
        const HRESULT hr = f(h, v, IID_IDirectInputA, (LPVOID*)&r, outer);
        if (FAILED(hr) || r == NULL) { return hr; }
        Input* i = new Input(r);
        const HRESULT q = i->QueryInterface(iid, out);
        i->Release();
        return q;
    }
    return f(h, v, iid, out, outer);
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    // runs before the game reads its settings, so the one-time setup takes effect right away
    if (reason == DLL_PROCESS_ATTACH) { InitializeCriticalSection(&PadLock); FirstRunSetup(); }
    if (reason == DLL_PROCESS_DETACH && LogFile != NULL) { fclose(LogFile); LogFile = NULL; }
    return TRUE;
}
