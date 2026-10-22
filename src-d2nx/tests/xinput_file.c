/* XInput de teste: le o estado do controle de pad.txt (nunca vai no pacote). */
#include <windows.h>
#include <stdio.h>

typedef struct { WORD b; BYTE lt, rt; SHORT lx, ly, rx, ry; } GP;
typedef struct { DWORD pkt; GP g; } ST;
typedef struct {
    BYTE Type;
    BYTE SubType;
    WORD Flags;
    GP Gamepad;
    struct { WORD wLeftMotorSpeed, wRightMotorSpeed; } Vibration;
} XINPUT_CAPS;

__declspec(dllexport) DWORD WINAPI XInputGetState(DWORD i, ST *s)
{
    static DWORD pkt;
    unsigned b = 0, lt = 0, rt = 0; int lx = 0, ly = 0, rx = 0, ry = 0;
    FILE *f;
    if (i || !s) return ERROR_DEVICE_NOT_CONNECTED;
    f = fopen("d2nx\\pad.txt", "r");
    if (!f) f = fopen("pad.txt", "r");
    if (!f) f = fopen("Z:\\tmp\\pad.txt", "r");
    if (!f) f = fopen("C:\\pad.txt", "r");
    if (f) { if (fscanf(f, "%x %u %u %d %d %d %d", &b, &lt, &rt, &lx, &ly, &rx, &ry) < 1) b = 0; fclose(f); }
    ZeroMemory(s, sizeof(*s));
    s->pkt = ++pkt; s->g.b = (WORD)b; s->g.lt = (BYTE)lt; s->g.rt = (BYTE)rt;
    s->g.lx = (SHORT)lx; s->g.ly = (SHORT)ly; s->g.rx = (SHORT)rx; s->g.ry = (SHORT)ry;
    return ERROR_SUCCESS;
}

__declspec(dllexport) DWORD WINAPI XInputSetState(DWORD i, void *v) { (void)v; return i ? ERROR_DEVICE_NOT_CONNECTED : 0; }
__declspec(dllexport) void WINAPI XInputEnable(BOOL e) { (void)e; }

__declspec(dllexport) DWORD WINAPI XInputGetCapabilities(DWORD i, DWORD flags, XINPUT_CAPS *caps)
{
    (void)flags;
    if (i || !caps) return ERROR_DEVICE_NOT_CONNECTED;
    ZeroMemory(caps, sizeof(*caps));
    caps->Type = 1;     /* XINPUT_DEVTYPE_GAMEPAD */
    caps->SubType = 1;  /* XINPUT_DEVSUBTYPE_GAMEPAD */
    caps->Flags = 0;
    caps->Gamepad.b = 0xFFFF;
    caps->Gamepad.lt = 255;
    caps->Gamepad.rt = 255;
    caps->Gamepad.lx = (SHORT)32767;
    caps->Gamepad.ly = (SHORT)32767;
    caps->Gamepad.rx = (SHORT)32767;
    caps->Gamepad.ry = (SHORT)32767;
    return ERROR_SUCCESS;
}

__declspec(dllexport) DWORD WINAPI XInputGetBatteryInformation(DWORD i, BYTE t, void *p)
{ (void)t; (void)p; return i ? ERROR_DEVICE_NOT_CONNECTED : ERROR_SUCCESS; }

__declspec(dllexport) DWORD WINAPI XInputGetKeystroke(DWORD i, DWORD f, void *p)
{ (void)f; (void)p; return i ? ERROR_DEVICE_NOT_CONNECTED : ERROR_EMPTY; }

__declspec(dllexport) DWORD WINAPI XInputGetAudioDeviceIds(DWORD i, void *a, void *b, void *c, void *d)
{ (void)a; (void)b; (void)c; (void)d; return i ? ERROR_DEVICE_NOT_CONNECTED : ERROR_SUCCESS; }
