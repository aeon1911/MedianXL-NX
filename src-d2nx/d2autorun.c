/* D2Autorun.dll v2 - camada de compatibilidade generica para Diablo II no AUTORUN (Switch).
 *
 * Carregado por import explicito no D2SE.exe e no Game.exe do ProjectD2 (sem injecao remota).
 * Em vez de corrigir cada mod por hash, corrige os pontos em que o runtime do Switch difere
 * do Windows, para QUALQUER core ou mod:
 *   - token/privilegios (OpenProcessToken/AdjustTokenPrivileges): o servidor Horizon do AUTORUN
 *     nao implementa abrir token; muitos loaders (P113c, D2Sigma, ProjectDiablo, D2Mod...) pedem
 *     SeDebugPrivilege e abortam. Se a chamada real falhar, devolvemos um token falso "elevado".
 *   - escrita em secao somente-leitura de DLLs (tabelas de hooks como a do PD2): um tratador de
 *     excecao torna a pagina gravavel e repete a instrucao.
 *   - validacao de secao critica da Fog.dll (crash no Wine): desativada em memoria por assinatura,
 *     em qualquer Fog de qualquer versao/mod (a copia em disco nao e alterada).
 *   - d2gl.mpq: o D2GL procura na pasta do core/mod; se nao existir la, usa o da raiz.
 *   - controle: qualquer D2Pad.dll pedida por D2SE/PlugY/mod vira o D2NXPad (mantendo o
 *     ChainLoad do D2Pad.ini original, ex. P113c do Median); cores vanilla tambem recebem.
 *   - perfil de video para 60 FPS (D2GL, ver d2nx.ini).
 *   - log de diagnostico: D2Autorun.log (modulos, caixas de erro, falhas).
 * Copyright 2026. Licenca MIT. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>
#include "hook.h"

#define AUTORUN_VERSION "0.4.4"

static FILE *g_log;
static CRITICAL_SECTION g_log_cs;
static char g_root[MAX_PATH];        /* pasta do D2SE.exe / Game.exe, com barra */
static char g_entry[MAX_PATH];       /* pasta do core/mod ativo, com barra */
static char g_folder[128];           /* ActiveFolder */
static char g_core[16];
static int g_is_d2se, g_is_pd2, g_is_mxl, g_is_vanilla, g_safe, g_entry_pad;
static char g_cfg[MAX_PATH];         /* d2nx\d2nx.ini */

static void lg(const char *fmt, ...)
{
    va_list ap; SYSTEMTIME t;
    if (!g_log) return;
    EnterCriticalSection(&g_log_cs);
    GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
    LeaveCriticalSection(&g_log_cs);
}

static int cfg_int(const char *sec, const char *key, int def)
{
    char k[200];
    /* sobrescrita por entrada: secao "<Pasta>" */
    if (g_folder[0]) {
        int v = GetPrivateProfileIntA(g_folder, key, -12345, g_cfg);
        if (v != -12345) return v;
    }
    (void)k;
    return GetPrivateProfileIntA(sec, key, def, g_cfg);
}
static void cfg_str(const char *sec, const char *key, const char *def, char *out, int n)
{
    if (g_folder[0]) {
        GetPrivateProfileStringA(g_folder, key, "\x01", out, n, g_cfg);
        if (out[0] != '\x01') return;
    }
    GetPrivateProfileStringA(sec, key, def, out, n, g_cfg);
}

static const char *base_name(const char *p)
{
    const char *a = strrchr(p, '\\'), *b = strrchr(p, '/');
    if (b > a) a = b;
    return a ? a + 1 : p;
}
static const WCHAR *base_nameW(const WCHAR *p)
{
    const WCHAR *a = wcsrchr(p, L'\\'), *b = wcsrchr(p, L'/');
    if (b > a) a = b;
    return a ? a + 1 : p;
}
static int file_exists(const char *p) { DWORD a = GetFileAttributesA(p); return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY); }

/* ======================================================================================
 * 1. token / privilegios
 * ==================================================================================== */
#define MAX_FAKE 64
static HANDLE g_fake[MAX_FAKE];
static LONG g_fake_count;
static int is_fake(HANDLE h) { int i; for (i = 0; i < MAX_FAKE; i++) if (h && g_fake[i] == h) return 1; return 0; }
static HANDLE new_fake(void)
{
    HANDLE h = CreateEventA(NULL, TRUE, FALSE, NULL);
    LONG i = InterlockedIncrement(&g_fake_count) - 1;
    g_fake[i % MAX_FAKE] = h;
    return h;
}

typedef BOOL (WINAPI *OpenProcessToken_t)(HANDLE, DWORD, PHANDLE);
typedef BOOL (WINAPI *OpenThreadToken_t)(HANDLE, DWORD, BOOL, PHANDLE);
typedef BOOL (WINAPI *AdjustTokenPrivileges_t)(HANDLE, BOOL, PTOKEN_PRIVILEGES, DWORD, PTOKEN_PRIVILEGES, PDWORD);
typedef BOOL (WINAPI *GetTokenInformation_t)(HANDLE, TOKEN_INFORMATION_CLASS, LPVOID, DWORD, PDWORD);
static OpenProcessToken_t real_OpenProcessToken;
static OpenThreadToken_t real_OpenThreadToken;
static AdjustTokenPrivileges_t real_AdjustTokenPrivileges;
static GetTokenInformation_t real_GetTokenInformation;
static LONG g_token_logged;

static BOOL WINAPI my_OpenProcessToken(HANDLE p, DWORD access, PHANDLE out)
{
    if (real_OpenProcessToken(p, access, out)) return TRUE;
    if (p != GetCurrentProcess() && GetProcessId(p) != GetCurrentProcessId()) return FALSE;
    *out = new_fake();
    if (InterlockedIncrement(&g_token_logged) <= 4) lg("OpenProcessToken falhou no runtime: token simulado %p", *out);
    SetLastError(ERROR_SUCCESS);
    return *out != NULL;
}
static BOOL WINAPI my_OpenThreadToken(HANDLE t, DWORD access, BOOL self, PHANDLE out)
{
    if (real_OpenThreadToken(t, access, self, out)) return TRUE;
    if (GetLastError() == ERROR_NO_TOKEN) return FALSE;      /* comportamento normal do Windows */
    *out = new_fake();
    SetLastError(ERROR_SUCCESS);
    return *out != NULL;
}
static BOOL WINAPI my_AdjustTokenPrivileges(HANDLE h, BOOL dis, PTOKEN_PRIVILEGES n, DWORD len, PTOKEN_PRIVILEGES prev, PDWORD ret)
{
    if (is_fake(h)) {
        if (prev && len >= sizeof(DWORD)) prev->PrivilegeCount = 0;
        if (ret) *ret = sizeof(DWORD);
        SetLastError(ERROR_SUCCESS);
        return TRUE;
    }
    return real_AdjustTokenPrivileges(h, dis, n, len, prev, ret);
}
static BOOL WINAPI my_GetTokenInformation(HANDLE h, TOKEN_INFORMATION_CLASS c, LPVOID buf, DWORD len, PDWORD ret)
{
    if (is_fake(h)) {
        if (c == TokenElevation || c == TokenElevationType) {
            if (ret) *ret = sizeof(DWORD);
            if (len < sizeof(DWORD) || !buf) { SetLastError(ERROR_INSUFFICIENT_BUFFER); return FALSE; }
            *(DWORD *)buf = c == TokenElevation ? 1 : 2 /* TokenElevationTypeFull */;
            return TRUE;
        }
        /* demais classes: usa o token real do processo, se existir */
        {
            HANDLE real;
            if (real_OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &real)) {
                BOOL r = real_GetTokenInformation(real, c, buf, len, ret);
                CloseHandle(real);
                return r;
            }
        }
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
    return real_GetTokenInformation(h, c, buf, len, ret);
}

/* ======================================================================================
 * 2. MessageBox (registra o texto do erro no log)
 * ==================================================================================== */
typedef int (WINAPI *MessageBoxA_t)(HWND, LPCSTR, LPCSTR, UINT);
typedef int (WINAPI *MessageBoxW_t)(HWND, LPCWSTR, LPCWSTR, UINT);
typedef int (WINAPI *MessageBoxIndirectA_t)(const MSGBOXPARAMSA *);
typedef int (WINAPI *MessageBoxIndirectW_t)(const MSGBOXPARAMSW *);
static MessageBoxA_t real_MessageBoxA;
static MessageBoxW_t real_MessageBoxW;
static MessageBoxIndirectA_t real_MessageBoxIndirectA;
static MessageBoxIndirectW_t real_MessageBoxIndirectW;
/* So o Halt da linha #904 do Fog e falta de memoria (Applet Mode). Outros Halts tem outra causa:
 * antes todo "Halt" era rotulado como #904, o que escondeu um #1696 na criacao de personagem. */
static void log_vm_layout(const char *tag);
static void storm_last_opened(void);
static void dump_mxl_reads(void);
static void halt_diag(const char *t)
{
    if (!t || !strstr(t, "Halt")) return;
    log_vm_layout("no Halt");
    lg("====================================================================");
    if (strstr(t, "#904")) {
        lg("[DIAGNOSTICO DO HALT] Halt line #904 em Fog.dll: a memoria do processo esgotou.");
        lg("O Homebrew Menu aberto pelo ALBUM (Applet Mode) tem so ~400 MB; o Median XL + D2GL usa ~900 MB.");
        lg("Abra o hbmenu por Title Override: segure [R] ao abrir qualquer jogo instalado.");
    } else {
        lg("[DIAGNOSTICO DO HALT] Halt que NAO e o #904 (falta de memoria): veja a linha acima e o log anterior.");
    }
    lg("====================================================================");
}
static int WINAPI my_MessageBoxA(HWND w, LPCSTR t, LPCSTR c, UINT f)
{
    (void)w; (void)f;
    lg("MessageBox [%s]: %s (auto-dispensado)", c ? c : "", t ? t : "");
    if (t && !strstr(t, "Halt")) { log_vm_layout("erro"); storm_last_opened(); dump_mxl_reads(); }
    halt_diag(t);
    return IDOK;
}
static int WINAPI my_MessageBoxW(HWND w, LPCWSTR t, LPCWSTR c, UINT f)
{
    (void)w; (void)f;
    lg("MessageBox [%ls]: %ls (auto-dispensado)", c ? c : L"", t ? t : L"");
    return IDOK;
}
static int WINAPI my_MessageBoxIndirectA(const MSGBOXPARAMSA *p)
{
    if (p) {
        lg("MessageBoxIndirect [%s]: %s (auto-dispensado)", p->lpszCaption ? p->lpszCaption : "", p->lpszText ? p->lpszText : "");
        halt_diag(p->lpszText);
    }
    return IDOK;
}
static int WINAPI my_MessageBoxIndirectW(const MSGBOXPARAMSW *p)
{
    if (p) lg("MessageBoxIndirect [%ls]: %ls (auto-dispensado)", p->lpszCaption ? p->lpszCaption : L"", p->lpszText ? p->lpszText : L"");
    return IDOK;
}

/* ======================================================================================
 * 2b. Version (evita saida fatal no SGD2FreeRes em 1.10f quando version.dll nao acha dados)
 * ==================================================================================== */
typedef DWORD (WINAPI *GetFileVersionInfoSizeA_t)(LPCSTR, LPDWORD);
typedef DWORD (WINAPI *GetFileVersionInfoSizeW_t)(LPCWSTR, LPDWORD);
typedef BOOL (WINAPI *GetFileVersionInfoA_t)(LPCSTR, DWORD, DWORD, LPVOID);
typedef BOOL (WINAPI *GetFileVersionInfoW_t)(LPCWSTR, DWORD, DWORD, LPVOID);
static GetFileVersionInfoSizeA_t real_GetFileVersionInfoSizeA;
static GetFileVersionInfoSizeW_t real_GetFileVersionInfoSizeW;
static GetFileVersionInfoA_t real_GetFileVersionInfoA;
static GetFileVersionInfoW_t real_GetFileVersionInfoW;

static DWORD WINAPI my_GetFileVersionInfoSizeA(LPCSTR fn, LPDWORD h)
{
    DWORD res = real_GetFileVersionInfoSizeA ? real_GetFileVersionInfoSizeA(fn, h) : 0;
    if (!res) { if (h) *h = 0; return 512; }
    return res;
}
static DWORD WINAPI my_GetFileVersionInfoSizeW(LPCWSTR fn, LPDWORD h)
{
    DWORD res = real_GetFileVersionInfoSizeW ? real_GetFileVersionInfoSizeW(fn, h) : 0;
    if (!res) { if (h) *h = 0; return 512; }
    return res;
}
static BOOL WINAPI my_GetFileVersionInfoA(LPCSTR fn, DWORD h, DWORD len, LPVOID data)
{
    BOOL res = real_GetFileVersionInfoA ? real_GetFileVersionInfoA(fn, h, len, data) : FALSE;
    if (!res && data && len >= 512) { memset(data, 0, len); return TRUE; }
    return res;
}
static BOOL WINAPI my_GetFileVersionInfoW(LPCWSTR fn, DWORD h, DWORD len, LPVOID data)
{
    BOOL res = real_GetFileVersionInfoW ? real_GetFileVersionInfoW(fn, h, len, data) : FALSE;
    if (!res && data && len >= 512) { memset(data, 0, len); return TRUE; }
    return res;
}

/* ======================================================================================
 * 3. excecoes: escrita em pagina somente-leitura de imagem / log de falhas
 * ==================================================================================== */
static LONG g_fault_logged, g_unprotect_count;
static void module_of(DWORD addr, char *out, int n, DWORD *off)
{
    HMODULE m = NULL;
    out[0] = 0; *off = addr;
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           (LPCSTR)addr, &m) && m) {
        char p[MAX_PATH];
        GetModuleFileNameA(m, p, sizeof(p));
        lstrcpynA(out, base_name(p), n);
        *off = addr - (DWORD)m;
    }
}
static LONG CALLBACK veh(EXCEPTION_POINTERS *e)
{
    EXCEPTION_RECORD *r = e->ExceptionRecord;
    if (r->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && r->NumberParameters >= 2) {
        DWORD kind = (DWORD)r->ExceptionInformation[0], addr = (DWORD)r->ExceptionInformation[1];
        MEMORY_BASIC_INFORMATION mbi;
        if ((kind == 1 || kind == 8) && addr >= 0x10000 && VirtualQuery((void *)addr, &mbi, sizeof(mbi)) &&
            mbi.State == MEM_COMMIT && (mbi.Type == MEM_IMAGE || (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ)))) {
            DWORD old;
            BOOL ok = VirtualProtect(mbi.BaseAddress, mbi.RegionSize, PAGE_EXECUTE_READWRITE, &old);
            if (!ok) {
                ok = VirtualProtect((void *)(addr & ~0xFFFUL), 0x1000, PAGE_EXECUTE_READWRITE, &old);
            }
            if (ok) {
                FlushInstructionCache(GetCurrentProcess(), mbi.BaseAddress, mbi.RegionSize);
                if (InterlockedIncrement(&g_unprotect_count) <= 32) {
                    char m[64]; DWORD off;
                    module_of(addr, m, sizeof(m), &off);
                    lg("Pagina protegida %s+%lX (base %p, tam 0x%lX, %s, eip %08lX): liberada PAGE_EXECUTE_READWRITE",
                       m[0] ? m : "?", off, mbi.BaseAddress, (DWORD)mbi.RegionSize,
                       kind == 1 ? "escrita" : (kind == 8 ? "execucao" : "leitura"), (DWORD)r->ExceptionAddress);
                }
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }
        if (InterlockedIncrement(&g_fault_logged) <= 16) {
            char m[64]; DWORD off;
            module_of((DWORD)r->ExceptionAddress, m, sizeof(m), &off);
            lg("Excecao c0000005 em %s+%lX (%s %08lX)", m[0] ? m : "?", off,
               kind == 1 ? "escrita" : (kind == 8 ? "execucao" : "leitura"), addr);
        }
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

/* ======================================================================================
 * 3b. FreeLibrary (evita descarregamento de modulos criticos com callbacks pendentes)
 * ==================================================================================== */
typedef BOOL (WINAPI *FreeLibrary_t)(HMODULE);
static FreeLibrary_t real_FreeLibrary;
static BOOL WINAPI my_FreeLibrary(HMODULE m)
{
    if (m) {
        char name[MAX_PATH];
        if (GetModuleFileNameA(m, name, sizeof(name))) {
            const char *b = base_name(name);
            if (!lstrcmpiA(b, "D2Pad.dll") || !lstrcmpiA(b, "D2NXPad.dll") ||
                !lstrcmpiA(b, "P113c.dll") || !lstrcmpiA(b, "D2Sigma.dll") ||
                !lstrcmpiA(b, "PlugY.dll") || !lstrcmpiA(b, "glide3x.dll")) {
                lg("FreeLibrary interceptado para %s: modulo mantido em memoria (anti-crash)", b);
                return TRUE;
            }
        }
    }
    return real_FreeLibrary ? real_FreeLibrary(m) : TRUE;
}

/* ======================================================================================
 * 4. Fog: desativa a validacao de secao critica
 * ==================================================================================== */
static void patch_fog(HMODULE fog)
{
    IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)((BYTE *)fog + ((IMAGE_DOS_HEADER *)fog)->e_lfanew);
    IMAGE_SECTION_HEADER *s = IMAGE_FIRST_SECTION(nt);
    int i, found = 0;
    BYTE *hit = NULL;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++, s++) {
        BYTE *p, *end;
        if (!(s->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        p = (BYTE *)fog + s->VirtualAddress;
        end = p + s->Misc.VirtualSize - 24;
        for (; p < end; p++) {
            BYTE *q;
            int k;
            if (p[0] != 0xA1 && !(p[0] == 0x31 && p[1] == 0xC0 && p[2] == 0x90 && p[3] == 0x90 && p[4] == 0x90)) continue;
            q = p + 5;
            for (k = 0; k < 4 && *q >= 0x50 && *q <= 0x57; k++) q++;
            if (q[0] != 0x85 || q[1] != 0xC0) continue;
            q += 2;
            for (k = 0; k < 4 && *q >= 0x50 && *q <= 0x57; k++) q++;
            if (q[0] != 0x8B || q[1] != 0xF1 || q[2] != 0x0F || q[3] != 0x84) continue;
            found++;
            hit = p;
        }
    }
    if (found == 1 && hit[0] == 0xA1) {
        DWORD old;
        if (VirtualProtect(hit, 5, PAGE_EXECUTE_READWRITE, &old)) {
            static const BYTE fix[5] = { 0x31, 0xC0, 0x90, 0x90, 0x90 };
            memcpy(hit, fix, 5);
            VirtualProtect(hit, 5, old, &old);
            FlushInstructionCache(GetCurrentProcess(), hit, 5);
            lg("Fog.dll: validacao de secao critica desativada em memoria (+%lX)", (DWORD)(hit - (BYTE *)fog));
        }
    } else if (found == 1) {
        lg("Fog.dll: validacao ja estava desativada");
    } else if (found > 1) {
        lg("Fog.dll: assinatura ambigua (%d), nada alterado", found);
    }
}


/* ======================================================================================
 * 5. LoadLibrary: D2Pad -> D2NXPad, glide3x -> D2GL do pacote
 * ==================================================================================== */
typedef HMODULE (WINAPI *LoadLibraryExW_t)(LPCWSTR, HANDLE, DWORD);
static LoadLibraryExW_t real_LoadLibraryExW;
static char g_pad_path[MAX_PATH];
static char g_glide_path[MAX_PATH];
static int g_redirect_pad = 1;

static void chain_load(const char *orig_dir)
{
    char ini[MAX_PATH], list[512], *tok, *ctx = NULL;
    snprintf(ini, sizeof(ini), "%sD2Pad.ini", orig_dir);
    GetPrivateProfileStringA("General", "ChainLoad", "", list, sizeof(list), ini);
    for (tok = strtok_r(list, ",|;", &ctx); tok; tok = strtok_r(NULL, ",|;", &ctx)) {
        char p[MAX_PATH];
        HMODULE h;
        while (*tok == ' ') tok++;
        if (!*tok) continue;
        if (strchr(tok, ':') || tok[0] == '\\') lstrcpynA(p, tok, sizeof(p));
        else snprintf(p, sizeof(p), "%s%s", orig_dir, tok);
        h = LoadLibraryA(p);
        lg("ChainLoad do D2Pad.ini: %s -> %s (erro %lu)", p, h ? "carregada" : "FALHOU", h ? 0 : GetLastError());
    }
}

static HMODULE WINAPI my_LoadLibraryExW(LPCWSTR name, HANDLE file, DWORD flags)
{
    if (name && !(flags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE))) {
        const WCHAR *b = base_nameW(name);
        if (g_redirect_pad && g_pad_path[0] && !lstrcmpiW(b, L"D2Pad.dll")) {
            char orig[MAX_PATH], dir[MAX_PATH], *s;
            HMODULE h;
            WideCharToMultiByte(CP_ACP, 0, name, -1, orig, sizeof(orig), NULL, NULL);
            if (strchr(orig, '\\') || strchr(orig, '/')) { lstrcpynA(dir, orig, sizeof(dir)); s = (char *)base_name(dir); *s = 0; }
            else lstrcpynA(dir, g_entry, sizeof(dir));
            {
                WCHAR w[MAX_PATH];
                MultiByteToWideChar(CP_ACP, 0, g_pad_path, -1, w, MAX_PATH);
                h = real_LoadLibraryExW(w, file, flags & ~LOAD_WITH_ALTERED_SEARCH_PATH);
            }
            lg("D2Pad.dll pedida (%s): usando D2NXPad %s", orig, h ? "ok" : "FALHOU");
            if (h) { chain_load(dir); return h; }
        } else if (g_glide_path[0] && !lstrcmpiW(b, L"glide3x.dll")) {
            WCHAR w[MAX_PATH];
            HMODULE h;
            MultiByteToWideChar(CP_ACP, 0, g_glide_path, -1, w, MAX_PATH);
            h = real_LoadLibraryExW(w, file, flags & ~LOAD_WITH_ALTERED_SEARCH_PATH);
            lg("glide3x.dll -> %s (%s)", g_glide_path, h ? "ok" : "FALHOU");
            if (h) return h;
        }
    }
    return real_LoadLibraryExW(name, file, flags);
}

/* ---- CreateFileW: d2gl.mpq ausente na pasta do core/mod -> raiz ---- */
typedef HANDLE (WINAPI *CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static CreateFileW_t real_CreateFileW;
static HANDLE WINAPI my_CreateFileW(LPCWSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE t)
{
    HANDLE h = real_CreateFileW(n, a, s, sa, c, f, t);
    if (h == INVALID_HANDLE_VALUE && n && c == OPEN_EXISTING && !lstrcmpiW(base_nameW(n), L"d2gl.mpq")) {
        DWORD err = GetLastError();
        WCHAR alt[MAX_PATH];
        char root[MAX_PATH];
        snprintf(root, sizeof(root), "%sd2nx\\d2gl\\d2gl.mpq", g_root);
        if (!file_exists(root)) snprintf(root, sizeof(root), "%sd2gl.mpq", g_root);
        MultiByteToWideChar(CP_ACP, 0, root, -1, alt, MAX_PATH);
        h = real_CreateFileW(alt, a, s, sa, c, f, t);
        if (h != INVALID_HANDLE_VALUE) lg("d2gl.mpq ausente na pasta do mod: usando %s", root);
        else SetLastError(err);
    }
    return h;
}

/* ======================================================================================
 * 5b. memoria abaixo de 2 GB (comportamento de programa nao-LAA)
 * O Wine do Autorun trata todo programa 32-bit como "large address aware" (WINE_LARGE_ADDRESS_AWARE
 * vem ligado, como no Proton, e o runtime nao passa variavel de ambiente). O Game.exe 1.13c nao e LAA:
 * no Ryujinx o heap entregou ao Fog blocos em 0xC2xxxxxx e a criacao de personagem parou em Halt #1696
 * (realloc de bloco "nao alocado": o pool do Fog usa aritmetica com sinal); no console virou "Generic
 * bad character file". O Fog ja recusava ponteiro acima de 0x7FFFFFFF na validacao que o patch +11690
 * desliga. O heap do Wine 32-bit cresce pelo NtAllocateVirtualMemory do ntdll de 32 bits: o gancho no
 * stub dele pede ZeroBits = 1 (endereco abaixo de 0x80000000) quando nao ha endereco fixo, como o
 * Windows faz com um programa nao-LAA. Se faltar memoria baixa, cai no pedido original.
 * ==================================================================================== */
typedef LONG (NTAPI *NtAllocateVirtualMemory_t)(HANDLE, PVOID *, ULONG_PTR, PSIZE_T, ULONG, ULONG);
static NtAllocateVirtualMemory_t g_ntalloc_orig;
static LONG g_low_fail;

static LONG NTAPI low_NtAllocateVirtualMemory(HANDLE proc, PVOID *addr, ULONG_PTR zero_bits, PSIZE_T size,
                                              ULONG type, ULONG prot)
{
    if (!zero_bits && addr && !*addr && proc == (HANDLE)-1) {
        LONG st = g_ntalloc_orig(proc, addr, 1, size, type, prot);
        if (st >= 0) return st;
        if (InterlockedIncrement(&g_low_fail) <= 4)
            lg("memoria abaixo de 2 GB esgotada (status %08lX, %lu KB): usando a alta", st, size ? (DWORD)(*size >> 10) : 0);
    }
    return g_ntalloc_orig(proc, addr, zero_bits, size, type, prot);
}

static void low_memory_hook(void)
{
    BYTE *stub = (BYTE *)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtAllocateVirtualMemory");
    BYTE *tramp;
    DWORD old;
    /* stub de syscall do wow64: mov eax,id / mov edx,abs / call edx / ret 18h (independe da posicao) */
    if (!stub || stub[0] != 0xB8 || stub[5] != 0xBA || stub[10] != 0xFF || stub[11] != 0xD2 ||
        stub[12] != 0xC2 || stub[13] != 0x18 || stub[14] != 0x00) {
        lg("memoria abaixo de 2 GB: stub do NtAllocateVirtualMemory diferente do esperado, nada alterado");
        return;
    }
    tramp = VirtualAlloc(NULL, 16, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!tramp) return;
    memcpy(tramp, stub, 15);
    VirtualProtect(tramp, 16, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), tramp, 16);
    g_ntalloc_orig = (NtAllocateVirtualMemory_t)tramp;
    if (!VirtualProtect(stub, 5, PAGE_EXECUTE_READWRITE, &old)) return;
    stub[0] = 0xE9;
    *(DWORD *)(stub + 1) = (DWORD)((BYTE *)low_NtAllocateVirtualMemory - (stub + 5));
    VirtualProtect(stub, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), stub, 5);
    lg("memoria abaixo de 2 GB: alocacoes sem endereco fixo limitadas a 0x7FFFFFFF (programa nao-LAA)");
}

/* ======================================================================================
 * 5c. diagnostico da Storm: qual arquivo de dentro do MPQ falhou
 * "The file data is corrupt" (erro 1392) so diz o MPQ. SFileOpenFileEx (ordinal 268) guarda o nome de cada
 * arquivo aberto e SFileReadFile (269) registra nome e erro quando uma leitura falha. Ligado por padrao
 * ([MedianXL] DiagnosticoStorm=0 desliga); foi o que mostrou os sons do Median XL recomprimidos em zlib.
 * ==================================================================================== */
typedef BOOL (WINAPI *SFileOpenFileEx_t)(HANDLE, const char *, DWORD, HANDLE *);
typedef BOOL (WINAPI *SFileReadFile_t)(HANDLE, void *, DWORD, DWORD *, void *);
static SFileOpenFileEx_t g_sfo_orig;
static SFileReadFile_t g_sfr_orig;
#define STORM_NAMES 512
static struct { HANDLE h; char name[96]; } g_storm_names[STORM_NAMES];
static volatile LONG g_storm_next;
static LONG g_storm_fail;

static BOOL WINAPI my_SFileOpenFileEx(HANDLE mpq, const char *name, DWORD scope, HANDLE *file)
{
    BOOL r = g_sfo_orig(mpq, name, scope, file);
    if (r && file && *file && name) {
        LONG i = InterlockedIncrement(&g_storm_next) % STORM_NAMES;
        g_storm_names[i].h = *file;
        lstrcpynA(g_storm_names[i].name, name, sizeof(g_storm_names[0].name));
    }
    return r;
}
static BOOL WINAPI my_SFileReadFile(HANDLE file, void *buf, DWORD len, DWORD *got, void *ov)
{
    BOOL r = g_sfr_orig(file, buf, len, got, ov);
    if (!r) {
        DWORD err = GetLastError();
        if (err != ERROR_HANDLE_EOF && InterlockedIncrement(&g_storm_fail) <= 20) {
            const char *n = "?";
            int i;
            for (i = 0; i < STORM_NAMES; i++) if (g_storm_names[i].h == file) n = g_storm_names[i].name;
            lg("Storm: leitura falhou em '%s' (%lu bytes, lidos %lu, erro %lu)", n, len, got ? *got : 0, err);
        }
        SetLastError(err);
    }
    return r;
}
/* leituras do medianxl-data.mpq (deslocamento, tamanho, CRC dos dados recebidos) para comparar com o arquivo */
typedef BOOL (WINAPI *ReadFile_t)(HANDLE, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
typedef HANDLE (WINAPI *CreateFileA_t)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE (WINAPI *CreateFileW_t)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static ReadFile_t g_rf_real;
static CreateFileA_t g_cfa_real;
static CreateFileW_t g_cfw_real;
static int g_io_diag;   /* [MedianXL] DiagnosticoLeitura=1: CRC de cada leitura do MPQ (pesado, so para depurar) */
static HANDLE g_mxl_handles[8];
static struct { DWORD off, len, got, crc, ov, tid; } g_reads[64];
static volatile LONG g_reads_n;
static unsigned crc32b(const BYTE *p, DWORD n)
{
    unsigned c = 0xFFFFFFFFu; DWORD i; int k;
    for (i = 0; i < n; i++) { c ^= p[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); }
    return ~c;
}
static int is_mxl_handle(HANDLE h)
{
    int i;
    for (i = 0; i < 8; i++) if (h && g_mxl_handles[i] == h) return 1;
    return 0;
}
static void remember_mxl(HANDLE h, int named)
{
    int i;
    if (h == INVALID_HANDLE_VALUE || !named) return;
    for (i = 0; i < 8; i++) if (!g_mxl_handles[i]) { g_mxl_handles[i] = h; lg("medianxl-data.mpq aberto: handle %p", h); return; }
}
static HANDLE WINAPI diag_CreateFileA(LPCSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE t)
{
    HANDLE h = g_cfa_real(n, a, s, sa, c, f, t);
    remember_mxl(h, n && strstr(n, "medianxl-data") != NULL);
    return h;
}
static HANDLE WINAPI diag_CreateFileW(LPCWSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE t)
{
    HANDLE h = g_cfw_real(n, a, s, sa, c, f, t);
    remember_mxl(h, n && wcsstr(n, L"medianxl-data") != NULL);
    return h;
}
static BOOL WINAPI diag_ReadFile(HANDLE h, LPVOID buf, DWORD len, LPDWORD got, LPOVERLAPPED ov)
{
    DWORD off = 0, n;
    BOOL r;
    int mine = is_mxl_handle(h);
    if (mine) off = ov ? ov->Offset : SetFilePointer(h, 0, NULL, FILE_CURRENT);
    r = g_rf_real(h, buf, len, got, ov);
    if (mine) {
        LONG i = InterlockedIncrement(&g_reads_n) % 64;
        n = got ? *got : 0;
        g_reads[i].off = off; g_reads[i].len = len; g_reads[i].got = n; g_reads[i].ov = ov != NULL;
        g_reads[i].crc = (r && n && buf) ? crc32b(buf, n) : 0; g_reads[i].tid = GetCurrentThreadId();
    }
    return r;
}
static int patch_iat(HMODULE m, const char *dll, const char *func, void *repl, void **orig)
{
    BYTE *b = (BYTE *)m;
    IMAGE_NT_HEADERS32 *nt;
    IMAGE_IMPORT_DESCRIPTOR *imp;
    IMAGE_DATA_DIRECTORY *dir;
    int done = 0;
    if (!m) return 0;
    nt = (IMAGE_NT_HEADERS32 *)(b + ((IMAGE_DOS_HEADER *)b)->e_lfanew);
    dir = &nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir->VirtualAddress) return 0;
    for (imp = (IMAGE_IMPORT_DESCRIPTOR *)(b + dir->VirtualAddress); imp->Name; imp++) {
        IMAGE_THUNK_DATA32 *names, *iat;
        if (!imp->OriginalFirstThunk || lstrcmpiA((char *)(b + imp->Name), dll)) continue;
        names = (IMAGE_THUNK_DATA32 *)(b + imp->OriginalFirstThunk);
        iat = (IMAGE_THUNK_DATA32 *)(b + imp->FirstThunk);
        for (; names->u1.AddressOfData; names++, iat++) {
            DWORD old;
            if (names->u1.Ordinal & IMAGE_ORDINAL_FLAG32) continue;
            if (strcmp((char *)((IMAGE_IMPORT_BY_NAME *)(b + names->u1.AddressOfData))->Name, func)) continue;
            if (!*orig) *orig = (void *)iat->u1.Function;
            if (VirtualProtect(&iat->u1.Function, 4, PAGE_READWRITE, &old)) {
                iat->u1.Function = (DWORD)repl;
                VirtualProtect(&iat->u1.Function, 4, old, &old);
                done++;
            }
        }
    }
    return done;
}
static void io_diag_module(HMODULE m, const char *name)
{
    int n = patch_iat(m, "kernel32.dll", "ReadFile", (void *)diag_ReadFile, (void **)&g_rf_real);
    n += patch_iat(m, "kernel32.dll", "CreateFileA", (void *)diag_CreateFileA, (void **)&g_cfa_real);
    n += patch_iat(m, "kernel32.dll", "CreateFileW", (void *)diag_CreateFileW, (void **)&g_cfw_real);
    if (n) lg("diagnostico de leitura de arquivo em %s (%d entradas)", name, n);
}
static void dump_mxl_reads(void)
{
    LONG n = g_reads_n, i;
    for (i = n - 19; i <= n; i++)
        if (i > 0) lg("  leitura %ld: off %lu len %lu lidos %lu crc %08lx %s tid %lu", i, g_reads[i % 64].off,
                      g_reads[i % 64].len, g_reads[i % 64].got, (unsigned long)g_reads[i % 64].crc,
                      g_reads[i % 64].ov ? "overlapped" : "sync", g_reads[i % 64].tid);
}

static void storm_last_opened(void)
{
    LONG n = g_storm_next, i;
    if (!g_sfo_orig) return;
    for (i = n - 7; i <= n; i++)
        if (i > 0) lg("  Storm abriu (%ld): %s", i, g_storm_names[i % STORM_NAMES].name);
}
static void *storm_trampoline(BYTE *fn, int n, void *hook)
{
    BYTE *t = VirtualAlloc(NULL, 32, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    DWORD old;
    if (!t) return NULL;
    memcpy(t, fn, n);
    t[n] = 0xE9; *(DWORD *)(t + n + 1) = (DWORD)((fn + n) - (t + n + 5));
    VirtualProtect(t, 32, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), t, 32);
    if (!VirtualProtect(fn, 5, PAGE_EXECUTE_READWRITE, &old)) return NULL;
    fn[0] = 0xE9; *(DWORD *)(fn + 1) = (DWORD)((BYTE *)hook - (fn + 5));
    VirtualProtect(fn, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), fn, 5);
    return t;
}
static void storm_diag_init(void)
{
    HMODULE st = GetModuleHandleA("Storm.dll");
    BYTE *o, *r;
    if (!st) return;
    o = (BYTE *)GetProcAddress(st, (LPCSTR)268);
    r = (BYTE *)GetProcAddress(st, (LPCSTR)269);
    /* mov ecx,[esp+10h] / sub esp,110h  e  mov eax,[esp+14h] / mov ecx,[esp+10h] */
    if (o && !memcmp(o, "\x8B\x4C\x24\x10\x81\xEC\x10\x01\x00\x00", 10) &&
        r && !memcmp(r, "\x8B\x44\x24\x14\x8B\x4C\x24\x10", 8)) {
        g_sfo_orig = (SFileOpenFileEx_t)storm_trampoline(o, 10, (void *)my_SFileOpenFileEx);
        g_sfr_orig = (SFileReadFile_t)storm_trampoline(r, 8, (void *)my_SFileReadFile);
        lg("Storm: diagnostico de leitura ativo (%s)", g_sfo_orig && g_sfr_orig ? "ok" : "falhou");
        if (g_io_diag) io_diag_module(st, "Storm.dll");
    } else {
        lg("Storm: bytes de SFileOpenFileEx/SFileReadFile nao conferem, diagnostico desligado");
    }
}

/* ======================================================================================
 * 6. notificacao de carga de modulos
 * ==================================================================================== */
typedef struct { ULONG Flags; const UNICODE_STRING *FullDllName, *BaseDllName; PVOID DllBase; ULONG SizeOfImage; } LDR_NOTIF;
typedef VOID (CALLBACK *LDR_NOTIF_FN)(ULONG, const LDR_NOTIF *, PVOID);
typedef LONG (NTAPI *LdrRegisterDllNotification_t)(ULONG, LDR_NOTIF_FN, PVOID, PVOID *);
static volatile LONG g_client_loaded;

static VOID CALLBACK on_dll(ULONG reason, const LDR_NOTIF *d, PVOID ctx)
{
    char name[128];
    (void)ctx;
    if (reason != 1 || !d || !d->BaseDllName || !d->BaseDllName->Buffer) return;
    WideCharToMultiByte(CP_ACP, 0, d->BaseDllName->Buffer, d->BaseDllName->Length / 2, name, sizeof(name) - 1, NULL, NULL);
    { unsigned l = d->BaseDllName->Length / 2; name[l < sizeof(name) - 1 ? l : sizeof(name) - 1] = 0; }
    if (d->FullDllName && d->FullDllName->Buffer) {
        char full[MAX_PATH];
        int n = WideCharToMultiByte(CP_ACP, 0, d->FullDllName->Buffer, d->FullDllName->Length / 2, full, sizeof(full) - 1, NULL, NULL);
        full[n > 0 ? n : 0] = 0;
        if (!strstr(full, "\\windows\\") && !strstr(full, "\\WINDOWS\\")) lg("modulo %s em %p", full, d->DllBase);
    }
    /* Fixa modulos criticos imediatamente para que FreeLibrary nunca os descarregue */
    if (!lstrcmpiA(name, "D2Pad.dll") || !lstrcmpiA(name, "D2NXPad.dll") ||
        !lstrcmpiA(name, "P113c.dll") || !lstrcmpiA(name, "D2Sigma.dll") ||
        !lstrcmpiA(name, "PlugY.dll") || !lstrcmpiA(name, "glide3x.dll")) {
        HMODULE pin = NULL;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)d->DllBase, &pin)) {
            lg("%s fixada em memoria via LdrNotification (PIN ok)", name);
        }
    }
    if (!lstrcmpiA(name, "Fog.dll")) patch_fog((HMODULE)d->DllBase);
    if (g_io_diag && !lstrcmpiA(name, "D2Sigma.dll")) io_diag_module((HMODULE)d->DllBase, name);
    if (!lstrcmpiA(name, "D2Client.dll")) InterlockedExchange(&g_client_loaded, 1);
}

/* ======================================================================================
 * 7. D2SE: entrada ativa, video, Start Plugin automatico
 * ==================================================================================== */
static const char *cores[] = { "1.07", "1.08", "1.09b", "1.09d", "1.10f", "1.11b", "1.12a", "1.13c" };

static void set_ini_value(const char *path, const char *section, const char *key, const char *value)
{
    /* edicao textual preservando comentarios e o sentinela do D2SE (WritePrivateProfileString
     * reescreveria o arquivo com outra formatacao em algumas versoes do Wine) */
    FILE *f = fopen(path, "rb");
    char *buf, *out, *p, *line;
    long sz;
    int in = 0, done = 0;
    size_t o = 0, cap;
    if (!f) return;
    fseek(f, 0, SEEK_END); sz = ftell(f); fseek(f, 0, SEEK_SET);
    buf = malloc(sz + 1); cap = sz + 256; out = malloc(cap);
    if (!buf || !out || fread(buf, 1, sz, f) != (size_t)sz) { fclose(f); free(buf); free(out); return; }
    fclose(f);
    buf[sz] = 0;
    for (p = buf; *p; ) {
        char *nl = strchr(p, '\n'), *t;
        size_t len = nl ? (size_t)(nl - p + 1) : strlen(p);
        line = p;
        t = line; while (*t == ' ' || *t == '\t') t++;
        if (*t == '[') {
            if (in && !done) { o += sprintf(out + o, "%s=%s\r\n", key, value); done = 1; }
            in = !strncasecmp(t + 1, section, strlen(section)) && t[1 + strlen(section)] == ']';
        } else if (in && !done && !strncasecmp(t, key, strlen(key)) && t[strlen(key)] == '=') {
            o += sprintf(out + o, "%s=%s\r\n", key, value); done = 1;
            p += len; continue;
        }
        if (o + len + 64 > cap) { cap = (o + len) * 2 + 256; out = realloc(out, cap); }
        memcpy(out + o, line, len); o += len;
        p += len;
    }
    if (in && !done) o += sprintf(out + o, "%s=%s\r\n", key, value);
    else if (!done) o += sprintf(out + o, "\r\n[%s]\r\n%s=%s\r\n", section, key, value);
    if (o != (size_t)sz || memcmp(out, buf, o)) {
        f = fopen(path, "wb");
        if (f) { fwrite(out, 1, o, f); fclose(f); }
    }
    free(buf); free(out);
}

static void read_active_entry(void)
{
    char ini[MAX_PATH], setup[MAX_PATH];
    unsigned i;
    snprintf(ini, sizeof(ini), "%sD2SE.ini", g_root);
    GetPrivateProfileStringA("USERSETTINGS", "ActiveFolder", "", g_folder, sizeof(g_folder), ini);
    g_is_vanilla = 0;
    for (i = 0; i < sizeof(cores) / sizeof(cores[0]); i++)
        if (!lstrcmpiA(g_folder, cores[i])) g_is_vanilla = 1;
    if (g_is_vanilla) snprintf(g_entry, sizeof(g_entry), "%sD2SE\\CORES\\%s\\", g_root, g_folder);
    else snprintf(g_entry, sizeof(g_entry), "%sMODS\\%s\\", g_root, g_folder);
    snprintf(setup, sizeof(setup), "%sD2SE_SETUP.ini", g_entry);
    GetPrivateProfileStringA("Protected", "D2Core", "", g_core, sizeof(g_core), setup);
    if (!g_core[0] && g_is_vanilla) lstrcpynA(g_core, g_folder, sizeof(g_core));
}

/* d2gl.ini da entrada: preserva o que o mod definiu (load_dlls, mini mapa...) e aplica o
 * perfil do Switch apenas nas chaves de desempenho. */
static void d2gl_profile(void)
{
    static const char *keys[][3] = {
        /* secao, chave, valor */
        {"Screen", "fullscreen", "true"},
        {"Screen", "window_width", "1280"},
        {"Screen", "window_height", "720"},
        {"Screen", "centered_window", "true"},
        {"Screen", "vsync", "true"},
        {"Screen", "foreground_fps", "true"},
        {"Screen", "foreground_fps_value", "60"},
        {"Screen", "background_fps", "true"},
        {"Screen", "background_fps_value", "30"},
        {"Screen", "auto_minimize", "false"},
        {"Screen", "unlock_cursor", "true"},
        {"Graphic", "shader_preset", "bilinear.slangp"},
        {"Graphic", "stretched_horizontal", "true"},
        {"Graphic", "stretched_vertical", "false"},
        {"Graphic", "stretched_viewport", "true"},
        {"Graphic", "sharpen", "true"},
        {"Graphic", "sharpen_strength", "1.200"},
        {"Graphic", "sharpen_clamp", "0.250"},
        {"Graphic", "sharpen_radius", "0.350"},
        {"Graphic", "fxaa", "true"},
        {"Graphic", "fxaa_preset", "2"},
        {"Graphic", "bloom", "false"},
        {"Graphic", "lut", "0"},
        {"Feature", "hd_cursor", "true"},
        {"Feature", "hd_text", "true"},
        {"Feature", "hd_text_scale", "1.100"},
        {"Feature", "motion_prediction", "true"},
        {"Feature", "skip_intro", "true"},
        {"Feature", "show_fps", "true"},
        {"Other", "gl_ver_major", "4"},
        {"Other", "gl_ver_minor", "3"},
        {"Other", "use_compute_shader", "false"},
        {"Other", "frame_latency", "1"},
    };
    char ini[MAX_PATH], v[64];
    unsigned i;
    snprintf(ini, sizeof(ini), "%sd2gl.ini", g_entry);
    if (!file_exists(ini)) {
        char src[MAX_PATH];
        snprintf(src, sizeof(src), "%sd2nx\\d2gl\\d2gl.ini", g_root);
        if (!file_exists(src)) snprintf(src, sizeof(src), "%sd2gl.ini", g_root);
        CopyFileA(src, ini, TRUE);
    }
    if (!file_exists(ini)) return;
    for (i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        char custom[64];
        snprintf(custom, sizeof(custom), "d2gl.%s", keys[i][1]);
        cfg_str("Video", custom, keys[i][2], v, sizeof(v));   /* o usuario pode trocar no d2nx.ini */
        set_ini_value(ini, keys[i][0], keys[i][1], v);
    }
    lg("d2gl.ini da entrada ajustado para o perfil do Switch (%s)", ini);
}

static int core_supports_d2gl(const char *core)
{
    return !lstrcmpiA(core, "1.09d") || !lstrcmpiA(core, "1.10f") || !lstrcmpiA(core, "1.11b") ||
           !lstrcmpiA(core, "1.12a") || !lstrcmpiA(core, "1.13c");
}

static int same_file(const char *a, const char *b);

/* Mapa de memoria resumido: o que esta em uso abaixo e acima de 2 GB. */
static void log_vm_layout(const char *tag)
{
    MEMORY_BASIC_INFORMATION mbi;
    DWORD a = 0x10000, used_lo = 0, used_hi = 0, resv_hi = 0;
    while (a < 0xFFFF0000 && VirtualQuery((void *)a, &mbi, sizeof(mbi))) {
        DWORD base = (DWORD)mbi.BaseAddress, size = (DWORD)mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && mbi.Type == MEM_MAPPED && size >= 0x1000000)
            lg("  mapeamento de arquivo: %08lX-%08lX (%lu MB)", base, base + size, size >> 20);
        if (mbi.State == MEM_COMMIT) {
            if (base >= 0x80000000) {
                used_hi += size;
                if (mbi.Type != MEM_IMAGE && size >= 0x100000)
                    lg("  acima de 2 GB: %08lX %lu KB %s", base, size >> 10, mbi.Type == MEM_MAPPED ? "mapeada" : "privada");
            } else used_lo += size;
        }
        else if (mbi.State == MEM_RESERVE && base >= 0x80000000) resv_hi += size;
        if (base + size <= a) break;
        a = base + size;
    }
    lg("memoria (%s): em uso abaixo de 2 GB %lu MB, acima de 2 GB %lu MB (+%lu MB reservados)",
       tag, used_lo >> 20, used_hi >> 20, resv_hi >> 20);
}

/* O Median XL grava os personagens em %APPDATA%\MedianXL\save e nao cria a pasta (no Windows quem cria e o
 * instalador). Sem ela o personagem novo nao e gravado e o jogo recusa carrega-lo: no Ryujinx o log do
 * jogo mostrou "[PLAYER LOAD] ClientAddPlayerToGame() Error Loading:<nome> Error:14", que e o
 * "Generic bad character file". Num console recem-instalado a pasta nao existe. */
static void ensure_save_dir(void)
{
    char dir[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("APPDATA", dir, sizeof(dir));
    if (!n || n >= sizeof(dir) - 20) { lg("APPDATA indisponivel: pasta de saves nao verificada"); return; }
    lstrcatA(dir, "\\MedianXL");
    CreateDirectoryA(dir, NULL);
    lstrcatA(dir, "\\save");
    if (CreateDirectoryA(dir, NULL)) lg("pasta de saves criada: %s", dir);
    else if (GetLastError() != ERROR_ALREADY_EXISTS) lg("pasta de saves %s: erro %lu", dir, GetLastError());
}

/* Versoes anteriores do D2NXPad gravavam data\global\excel\charstats.bin com "gld " no kit inicial
 * (ouro inicial). O Median XL recusa o personagem criado assim ("Generic bad character file").
 * Remove essa copia antes de o jogo carregar as tabelas, para valer a original do MPQ. */
static void remove_bad_charstats(void)
{
    char path[MAX_PATH];
    HANDLE h;
    snprintf(path, sizeof(path), "%sdata\\global\\excel\\charstats.bin", g_root);
    h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    {
        char buf[4096];
        DWORD got = 0, i;
        int bad = 0;
        ReadFile(h, buf, sizeof(buf), &got, NULL);
        CloseHandle(h);
        for (i = 0; i + 4 <= got; i++) if (!memcmp(buf + i, "gld ", 4)) { bad = 1; break; }
        if (!bad) return;
    }
    lg("charstats.bin com ouro inicial (gld) encontrado: %s", DeleteFileA(path) ? "removido" : "FALHA ao remover");
}

static void video_setup(void)
{
    char mode[32], setup[MAX_PATH], glide[MAX_PATH];
    int d2gl;
    HKEY k;
    DWORD render;
    cfg_str("Video", "Renderizador", "auto", mode, sizeof(mode));
    snprintf(setup, sizeof(setup), "%sD2SE_SETUP.ini", g_entry);
    snprintf(glide, sizeof(glide), "%sd2nx\\d2gl\\glide3x.dll", g_root);
    if (!file_exists(glide)) snprintf(glide, sizeof(glide), "%sglide3x.dll", g_root);
    if (!lstrcmpiA(mode, "nativo")) { lg("Video: configuracao original da entrada mantida"); return; }
    {   /* Median XL */
        char sigma[MAX_PATH], p113[MAX_PATH];
        snprintf(sigma, sizeof(sigma), "%sD2Sigma.dll", g_root);
        snprintf(p113, sizeof(p113), "%sP113c.dll", g_entry);
        if (file_exists(sigma) || file_exists(p113) || g_is_mxl) {
            char root_glide[MAX_PATH];
            snprintf(root_glide, sizeof(root_glide), "%sglide3x.dll", g_root);
            lstrcpynA(g_glide_path, root_glide, sizeof(g_glide_path));
            render = 3;
            set_ini_value(setup, "USERSETTINGS", "Renderer", "3");
            set_ini_value(setup, "USERSETTINGS", "WindowMode", "0");
            d2gl_profile();
            lg("Video: Median XL configurado com D2GL (perfil Switch, 60 FPS, skip_intro)");
            if (!RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Blizzard Entertainment\\Diablo II\\VideoConfig", 0, NULL, 0,
                                 KEY_SET_VALUE, NULL, &k, NULL)) {
                RegSetValueExA(k, "Render", 0, REG_DWORD, (BYTE *)&render, sizeof(render));
                RegCloseKey(k);
            }
            return;
        }
    }
    d2gl = (!lstrcmpiA(mode, "auto") || !lstrcmpiA(mode, "d2gl")) && core_supports_d2gl(g_core) && file_exists(glide);
    if (d2gl) {
        lstrcpynA(g_glide_path, glide, sizeof(g_glide_path));
        render = 3;
        set_ini_value(setup, "USERSETTINGS", "Renderer", "3");
        set_ini_value(setup, "USERSETTINGS", "WindowMode", "0");
        d2gl_profile();
    } else if (!lstrcmpiA(mode, "ddraw")) {
        render = 0;
        set_ini_value(setup, "USERSETTINGS", "Renderer", "0");
    } else {
        /* Glide simples (Sven) para cores sem D2GL ou quando sven for solicitado */
        char sven[MAX_PATH];
        snprintf(sven, sizeof(sven), "%sd2nx\\sven\\glide3x.dll", g_root);
        if (file_exists(sven)) lstrcpynA(g_glide_path, sven, sizeof(g_glide_path));
        render = 3;
        set_ini_value(setup, "USERSETTINGS", "Renderer", "3");
        set_ini_value(setup, "USERSETTINGS", "WindowMode", "0");
    }
    if (!RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Blizzard Entertainment\\Diablo II\\VideoConfig", 0, NULL, 0,
                         KEY_SET_VALUE, NULL, &k, NULL)) {
        RegSetValueExA(k, "Render", 0, REG_DWORD, (BYTE *)&render, sizeof(render));
        RegCloseKey(k);
    }
    lg("Video: core %s, renderizador %s%s", g_core, d2gl ? "D2GL" : render ? "Glide" : "DirectDraw",
       g_glide_path[0] ? "" : " (wrapper original)");
}

static HWND g_start_button;
static BOOL CALLBACK child(HWND w, LPARAM l)
{
    char c[128];
    (void)l;
    GetWindowTextA(w, c, sizeof(c));
    if ((!lstrcmpiA(c, "Start Plugin") || !lstrcmpiA(c, "StartPlugin")) && IsWindowEnabled(w)) g_start_button = w;
    return TRUE;
}
static BOOL CALLBACK top(HWND w, LPARAM l)
{
    DWORD pid = 0;
    (void)l;
    GetWindowThreadProcessId(w, &pid);
    if (pid == GetCurrentProcessId()) {
        char t[256];
        GetWindowTextA(w, t, sizeof(t));
        /* D2SE 2.2.0: botao owner-drawn sem texto, ID 1 (IDOK); Start D2 = 3000 */
        if (strstr(t, "D2SE Modmanager ->") && strstr(t, "inside Folder:")) {
            HWND b = GetDlgItem(w, 1);
            if (b && IsWindowEnabled(b) && IsWindowVisible(b)) g_start_button = b;
        }
        if (!g_start_button) EnumChildWindows(w, child, 0);
    }
    return TRUE;
}

static void ensure_pad(void)
{
    char mode[16];
    if (!g_pad_path[0]) return;
    cfg_str("Controle", "Controle", "d2nxpad", mode, sizeof(mode));
    if (!lstrcmpiA(mode, "nenhum") || !lstrcmpiA(mode, "nativo")) return;
    if (GetModuleHandleA("D2NXPad.dll")) return;
    {
        HMODULE h = LoadLibraryA(g_pad_path);
        lg("D2NXPad carregado para %s: %s (erro %lu)", g_folder[0] ? g_folder : "jogo", h ? "ok" : "FALHOU", h ? 0 : GetLastError());
    }
}

static int same_file(const char *a, const char *b)
{
    FILE *f = fopen(a, "rb"), *g = fopen(b, "rb");
    int same = f && g, x, y;
    while (same) {
        x = fgetc(f); y = fgetc(g);
        if (x != y) same = 0;
        if (x == EOF || y == EOF) break;
    }
    if (f) fclose(f);
    if (g) fclose(g);
    return same;
}

/* Mods que carregam D2Pad.dll (ModDll do D2SE ou DllToLoad do PlugY): troca o arquivo pelo
 * D2NXPad, guardando o original. O D2NXPad copiado carrega o ChainLoad do D2Pad.ini como a
 * D2Pad original fazia. SubstituirD2Pad=0 ou Controle=nativo devolvem o original. */
static void swap_pad(void)
{
    char pad[MAX_PATH], orig[MAX_PATH], m[16];
    int want;
    if (g_is_vanilla || !g_pad_path[0]) return;
    snprintf(pad, sizeof(pad), "%sD2Pad.dll", g_entry);
    snprintf(orig, sizeof(orig), "%sD2Pad.dll.original-d2pad", g_entry);
    if (!file_exists(pad) && !file_exists(orig)) return;
    g_entry_pad = 1;
    cfg_str("Controle", "Controle", "d2nxpad", m, sizeof(m));
    want = cfg_int("Controle", "SubstituirD2Pad", 1) && lstrcmpiA(m, "nativo") && lstrcmpiA(m, "nenhum");
    if (want) {
        if (same_file(pad, g_pad_path)) { lg("D2Pad.dll do mod ja e o D2NXPad"); return; }
        if (!file_exists(orig)) {
            SetFileAttributesA(pad, FILE_ATTRIBUTE_NORMAL);
            if (!CopyFileA(pad, orig, TRUE)) {
                DWORD err = GetLastError();
                lg("D2Pad: falha ao guardar o original (erro %lu)", err);
                return;
            }
        }
        SetFileAttributesA(pad, FILE_ATTRIBUTE_NORMAL);
        BOOL ok = CopyFileA(g_pad_path, pad, FALSE);
        DWORD err = ok ? 0 : GetLastError();
        lg("D2Pad.dll do mod -> D2NXPad: %s (erro %lu)", ok ? "ok" : "FALHOU", err);
    } else if (file_exists(orig) && same_file(pad, g_pad_path)) {
        SetFileAttributesA(pad, FILE_ATTRIBUTE_NORMAL);
        BOOL ok = CopyFileA(orig, pad, FALSE);
        DWORD err = ok ? 0 : GetLastError();
        lg("D2Pad.dll original restaurado: %s (erro %lu)", ok ? "ok" : "FALHOU", err);
    }
}

static DWORD WINAPI worker(void *arg)
{
    unsigned n;
    HMODULE fog;
    (void)arg;

    lg("Worker thread iniciada fora do loader lock");

    fog = GetModuleHandleA("Fog.dll");
    if (fog) patch_fog(fog);

    if (g_is_d2se) swap_pad();
    if (g_is_d2se) {
        int autostart = cfg_int("D2SE", "IniciarAutomatico", 1);
        for (n = 0; autostart && n < 3000; n++) {
            EnumWindows(top, 0);
            if (g_start_button) {
                Sleep(400);
                lg("D2SE: Start Plugin (controle %d)", GetDlgCtrlID(g_start_button));
                PostMessageA(GetParent(g_start_button), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(g_start_button), BN_CLICKED),
                             (LPARAM)g_start_button);
                break;
            }
            if (n % 150 == 0) lg("D2SE: aguardando o menu (%u s)", n / 5);
            Sleep(200);
        }
    } else if (g_is_pd2) {
        HMODULE m = GetModuleHandleA("ProjectDiablo.dll");
        if (!m) m = LoadLibraryA("ProjectDiablo.dll");
        lg("ProjectDiablo.dll: %s (erro %lu)", m ? "carregada" : "FALHOU", m ? 0 : GetLastError());
        if (!m) {
            char msg[256];
            snprintf(msg, sizeof(msg), "Nao foi possivel carregar ProjectDiablo.dll (erro %lu). Veja D2Autorun.log.", GetLastError());
            MessageBoxA(NULL, msg, "Diablo II AUTORUN", MB_OK | MB_ICONERROR);
            ExitProcess(73);
        }
    }
    /* controle: espera o D2Client e garante o D2NXPad (se nenhum D2Pad foi pedido pelo mod) */
    for (n = 0; n < 12000 && !GetModuleHandleA("D2Client.dll"); n++) Sleep(50);
    if (GetModuleHandleA("D2Client.dll")) {
        Sleep(300);
        /* mods com D2Pad.dll proprio recebem o controle pela troca de arquivo */
        if (g_entry_pad) {
            for (n = 0; n < 150 && !GetModuleHandleA("D2Pad.dll"); n++) Sleep(100);
            if (GetModuleHandleA("D2Pad.dll")) {
                lg("Controle: carregado pelo proprio mod (D2Pad.dll)");
                HMODULE pin = NULL;
                if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, "D2Pad.dll", &pin)) {
                    lg("D2Pad.dll fixada em memoria (PIN ok)");
                }
            } else { lg("O mod tem D2Pad.dll mas nao o carregou: carregando o D2NXPad"); ensure_pad(); }
        } else if (!g_is_pd2 || cfg_int("Controle", "ControleNoPD2", 0)) ensure_pad();

        /* Garante fixacao adicional de DLLs criticas */
        {
            const char *critical[] = {
                "D2Pad.dll", "D2NXPad.dll", "P113c.dll", "D2Sigma.dll", "PlugY.dll", "glide3x.dll"
            };
            for (unsigned i = 0; i < sizeof(critical) / sizeof(critical[0]); i++) {
                HMODULE pin = NULL;
                if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN, critical[i], &pin) && pin) {
                    lg("%s confirmada fixada em memoria", critical[i]);
                }
            }
        }
    }
    return 0;
}

static void install_messagebox_hooks(void)
{
    LoadLibraryA("user32.dll");
    real_MessageBoxA = hook_export("user32.dll", "MessageBoxA", my_MessageBoxA);
    real_MessageBoxW = hook_export("user32.dll", "MessageBoxW", my_MessageBoxW);
    real_MessageBoxIndirectA = hook_export("user32.dll", "MessageBoxIndirectA", my_MessageBoxIndirectA);
    real_MessageBoxIndirectW = hook_export("user32.dll", "MessageBoxIndirectW", my_MessageBoxIndirectW);
    lg("ganchos ui (auto-dispensar popup): MessageBoxA %s, MessageBoxW %s",
       real_MessageBoxA ? "ok" : "-", real_MessageBoxW ? "ok" : "-");
}

static void install_hooks(void)
{
    real_OpenProcessToken = hook_export("kernelbase.dll", "OpenProcessToken", my_OpenProcessToken);
    if (!real_OpenProcessToken) real_OpenProcessToken = hook_export("advapi32.dll", "OpenProcessToken", my_OpenProcessToken);
    real_OpenThreadToken = hook_export("kernelbase.dll", "OpenThreadToken", my_OpenThreadToken);
    if (!real_OpenThreadToken) real_OpenThreadToken = hook_export("advapi32.dll", "OpenThreadToken", my_OpenThreadToken);
    real_AdjustTokenPrivileges = hook_export("kernelbase.dll", "AdjustTokenPrivileges", my_AdjustTokenPrivileges);
    if (!real_AdjustTokenPrivileges) real_AdjustTokenPrivileges = hook_export("advapi32.dll", "AdjustTokenPrivileges", my_AdjustTokenPrivileges);
    real_GetTokenInformation = hook_export("kernelbase.dll", "GetTokenInformation", my_GetTokenInformation);
    if (!real_GetTokenInformation) real_GetTokenInformation = hook_export("advapi32.dll", "GetTokenInformation", my_GetTokenInformation);
    real_FreeLibrary = hook_export("kernelbase.dll", "FreeLibrary", my_FreeLibrary);
    if (!real_FreeLibrary) real_FreeLibrary = hook_export("kernel32.dll", "FreeLibrary", my_FreeLibrary);
    real_LoadLibraryExW = hook_export("kernelbase.dll", "LoadLibraryExW", my_LoadLibraryExW);
    if (!real_LoadLibraryExW) real_LoadLibraryExW = hook_export("kernel32.dll", "LoadLibraryExW", my_LoadLibraryExW);
    real_CreateFileW = hook_export("kernelbase.dll", "CreateFileW", my_CreateFileW);
    if (!real_CreateFileW) real_CreateFileW = hook_export("kernel32.dll", "CreateFileW", my_CreateFileW);
    install_messagebox_hooks();
    LoadLibraryA("version.dll");
    real_GetFileVersionInfoSizeA = hook_export("version.dll", "GetFileVersionInfoSizeA", my_GetFileVersionInfoSizeA);
    real_GetFileVersionInfoSizeW = hook_export("version.dll", "GetFileVersionInfoSizeW", my_GetFileVersionInfoSizeW);
    real_GetFileVersionInfoA = hook_export("version.dll", "GetFileVersionInfoA", my_GetFileVersionInfoA);
    real_GetFileVersionInfoW = hook_export("version.dll", "GetFileVersionInfoW", my_GetFileVersionInfoW);
    lg("ganchos: token %s/%s/%s/%s, FreeLibrary %s, LoadLibrary %s, CreateFile %s, MessageBox %s/%s/%s/%s, Version %s/%s",
       real_OpenProcessToken ? "ok" : "-", real_OpenThreadToken ? "ok" : "-", real_AdjustTokenPrivileges ? "ok" : "-",
       real_GetTokenInformation ? "ok" : "-", real_FreeLibrary ? "ok" : "-", real_LoadLibraryExW ? "ok" : "-", real_CreateFileW ? "ok" : "-",
       real_MessageBoxA ? "ok" : "-", real_MessageBoxW ? "ok" : "-",
       real_MessageBoxIndirectA ? "ok" : "-", real_MessageBoxIndirectW ? "ok" : "-",
       real_GetFileVersionInfoSizeW ? "ok" : "-", real_GetFileVersionInfoW ? "ok" : "-");
}

static void log_memory_status(void)
{
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (GlobalMemoryStatusEx(&ms)) {
        DWORD total_mb = (DWORD)(ms.ullTotalPhys / (1024 * 1024));
        DWORD avail_mb = (DWORD)(ms.ullAvailPhys / (1024 * 1024));
        lg("Memoria do sistema: %lu MB total, %lu MB disponivel (carga: %lu%%)",
           total_mb, avail_mb, ms.dwMemoryLoad);
        if (total_mb < 600) {
            lg("====================================================================");
            lg("[ALERTA CRITICO] MODO ALBUM / APPLET DETECTADO! (RAM total: %lu MB)", total_mb);
            lg("O Switch limita o Homebrew Menu aberto pelo Album a ~400 MB de RAM.");
            lg("O Median XL Sigma + D2GL consome ~900 MB e VAI TRAVAR com 'Halt line #904'!");
            lg("COMO CORRIGIR DEFINITIVAMENTE:");
            lg("  1. Va para a tela inicial do Switch (HOME).");
            lg("  2. SEGURE o botao [R] no Joy-Con / controle.");
            lg("  3. Com [R] mantido segurado, abra QUALQUER JOGO instalado no Switch.");
            lg("  4. Quando abrir a tela azul do Homebrew Menu, solte o botao [R].");
            lg("  5. Abra o Diablo II / D2Autorun (tera 3200 MB de RAM livre!).");
            lg("====================================================================");
        } else {
            lg("Modo de memoria: TITLE OVERRIDE / APLICACAO OK (%lu MB de RAM)", total_mb);
        }
    }
}

__declspec(dllexport) void D2Autorun_Init(void) {}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved)
{
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        char exe[MAX_PATH], logp[MAX_PATH], *s;
        HANDLE th;
        DisableThreadLibraryCalls(module);
        InitializeCriticalSection(&g_log_cs);
        GetModuleFileNameA(NULL, exe, sizeof(exe));
        lstrcpynA(g_root, exe, sizeof(g_root));
        if ((s = strrchr(g_root, '\\'))) s[1] = 0;
        g_is_d2se = !lstrcmpiA(base_name(exe), "D2SE.exe");
        g_is_pd2 = !g_is_d2se && file_exists(strcat(strcpy(logp, g_root), "ProjectDiablo.dll"));
        snprintf(logp, sizeof(logp), "%sD2Autorun.log", g_root);
        g_log = fopen(logp, "w");
        snprintf(g_cfg, sizeof(g_cfg), "%sd2nx\\d2nx.ini", g_root);
        if (!file_exists(g_cfg)) snprintf(g_cfg, sizeof(g_cfg), "%s..\\d2nx\\d2nx.ini", g_root);  /* ProjectD2 */
        snprintf(g_pad_path, sizeof(g_pad_path), "%sd2nx\\D2NXPad.dll", g_root);
        if (!file_exists(g_pad_path)) snprintf(g_pad_path, sizeof(g_pad_path), "%s..\\d2nx\\D2NXPad.dll", g_root);
        if (!file_exists(g_pad_path)) g_pad_path[0] = 0;
        lg("==== D2Autorun %s (%s, pid %lu) ====", AUTORUN_VERSION, base_name(exe), GetCurrentProcessId());
        log_memory_status();
        char sigma_path[MAX_PATH];
        snprintf(sigma_path, sizeof(sigma_path), "%sD2Sigma.dll", g_root);
        g_is_mxl = !g_is_d2se && file_exists(sigma_path);
        if (g_is_d2se) {
            read_active_entry();
            lg("Entrada ativa: %s (core %s, %s)", g_folder, g_core[0] ? g_core : "?", g_is_vanilla ? "vanilla" : "mod");
        } else if (g_is_pd2) {
            lstrcpynA(g_folder, "ProjectDiablo", sizeof(g_folder));
            lstrcpynA(g_core, "1.13c", sizeof(g_core));
            lstrcpynA(g_entry, g_root, sizeof(g_entry));
        } else if (g_is_mxl) {
            lstrcpynA(g_folder, "MedianXL", sizeof(g_folder));
            lstrcpynA(g_core, "1.13c", sizeof(g_core));
            lstrcpynA(g_entry, g_root, sizeof(g_entry));
            lg("Entrada ativa: %s (core %s, boot direto Game.exe)", g_folder, g_core);

            /* Se medianxl-data.mpq unificado estiver presente, remove os 56 MPQs delta obsoletos
               para evitar a sobrecarga de I/O de 5 minutos do FAT32 no Switch */
            char unified_mpq[MAX_PATH];
            snprintf(unified_mpq, sizeof(unified_mpq), "%smedianxl-data.mpq", g_root);
            if (file_exists(unified_mpq)) {
                WIN32_FIND_DATAA fd;
                char pattern[MAX_PATH];
                snprintf(pattern, sizeof(pattern), "%smedianxl-*.mpq", g_root);
                HANDLE hFind = FindFirstFileA(pattern, &fd);
                if (hFind != INVALID_HANDLE_VALUE) {
                    int cleaned = 0;
                    do {
                        if (lstrcmpiA(fd.cFileName, "medianxl-data.mpq") != 0) {
                            char old_path[MAX_PATH];
                            snprintf(old_path, sizeof(old_path), "%s%s", g_root, fd.cFileName);
                            if (DeleteFileA(old_path)) cleaned++;
                        }
                    } while (FindNextFileA(hFind, &fd));
                    FindClose(hFind);
                    if (cleaned > 0) {
                        lg("Limpeza automatica: %d MPQs delta obsoletos removidos (boot rapido ativo)", cleaned);
                    }
                }
            }

            /* Se Game.box64.txt obsoleto estiver presente no SD, remove para evitar lentidao extrema do dynarec */
            char obsolete_box64[MAX_PATH];
            snprintf(obsolete_box64, sizeof(obsolete_box64), "%sGame.box64.txt", g_root);
            if (file_exists(obsolete_box64)) {
                if (DeleteFileA(obsolete_box64)) {
                    lg("Removido Game.box64.txt obsoleto para restaurar velocidade nativa");
                }
            }
        }
        /* o D2Pad do mod e trocado por copia de arquivo (swap_pad); nenhum gancho de LoadLibrary para isso */
        g_redirect_pad = 0;
        g_safe = cfg_int("Geral", "ModoSeguro", 0);
        if (g_is_mxl) {
            int v = GetPrivateProfileIntA("MedianXL", "ModoSeguro", -12345, g_cfg);
            g_safe = (v != -12345) ? v : 1;
        } else if (g_is_d2se && !g_is_vanilla) {
            char p113[MAX_PATH];
            snprintf(p113, sizeof(p113), "%sP113c.dll", g_entry);
            /* Median XL (D2Sigma) encerra o jogo se detectar funcoes de token interceptadas */
            if (file_exists(p113)) {
                int v = GetPrivateProfileIntA(g_folder, "ModoSeguro", -12345, g_cfg);
                g_safe = (v != -12345) ? v : 1;
            }
        }
        if (g_safe) {
            lg("Modo seguro: ganchos de sistema desativados para compatibilidade (entrada %s)", g_folder);
            install_messagebox_hooks();
        } else {
            install_hooks();
        }

        /* O tratador vetorado (VEH) DEVE ser instalado em TODOS os casos.
         * No Switch/Horizon OS e Wine ARM64, protecao W^X e ativada por hardware.
         * Quando qualquer mod grava ganchos na memoria de codigo das DLLs, o Tegra X1 gera
         * Access Violation (c0000005). O VEH intercepta a falha e libera a secao com PAGE_EXECUTE_READWRITE. */
        AddVectoredExceptionHandler(1, veh);
        lg("Tratador de excecoes vetorado (VEH c0000005) ativo para todos os modulos");

        if (g_is_mxl && GetPrivateProfileIntA("MedianXL", "Memoria2GB", 1, g_cfg)) low_memory_hook();
        if (g_is_mxl) {
            g_io_diag = GetPrivateProfileIntA("MedianXL", "DiagnosticoLeitura", 0, g_cfg);
            if (GetPrivateProfileIntA("MedianXL", "DiagnosticoStorm", 1, g_cfg)) storm_diag_init();
        }

        void *cookie = NULL;
        LdrRegisterDllNotification_t reg = (LdrRegisterDllNotification_t)GetProcAddress(GetModuleHandleA("ntdll.dll"), "LdrRegisterDllNotification");
        if (reg) {
            reg(0, on_dll, NULL, &cookie);
            lg("LdrRegisterDllNotification ativo (notificacoes de modulo e pin de memoria)");
        }

        if (g_is_mxl) {
            log_vm_layout("boot");
            ensure_save_dir();
            remove_bad_charstats();
        }
        if (g_is_d2se || g_is_mxl) video_setup();
        th = CreateThread(NULL, 2 * 1024 * 1024, worker, NULL, 0, NULL);
        if (th) CloseHandle(th);
    }
    return TRUE;
}
