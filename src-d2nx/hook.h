/* Gancho inline minimo para funcoes x86 do Wine/Windows (PE32).
 * Aceita os prologos de 5 bytes usados pelo Wine ("66 90 55 89 E5") e pela Microsoft
 * ("8B FF 55 8B EC"), e alguns inicios comuns decodificados por tamanho. */
#ifndef D2NX_HOOK_H
#define D2NX_HOOK_H
#include <windows.h>

static BYTE *hook_alloc(void)
{
    static BYTE *pool; static size_t used;
    if (!pool || used + 32 > 4096) { pool = VirtualAlloc(NULL, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE); used = 0; }
    if (!pool) return NULL;
    used += 32;
    return pool + used - 32;
}

/* tamanho de algumas instrucoes de prologo comuns; 0 = desconhecida */
static int hook_insn_len(const BYTE *p)
{
    if (p[0] >= 0x50 && p[0] <= 0x57) return 1;              /* push reg */
    if (p[0] == 0x55) return 1;
    if (p[0] == 0x8B && p[1] == 0xFF) return 2;              /* mov edi,edi */
    if (p[0] == 0x66 && p[1] == 0x90) return 2;              /* xchg ax,ax */
    if ((p[0] == 0x8B || p[0] == 0x89) && (p[1] & 0xC0) == 0xC0) return 2; /* mov r,r */
    if (p[0] == 0x83 && p[1] == 0xEC) return 3;              /* sub esp,imm8 */
    if (p[0] == 0x81 && p[1] == 0xEC) return 6;              /* sub esp,imm32 */
    if (p[0] == 0x6A) return 2;                              /* push imm8 */
    if (p[0] == 0x68) return 5;                              /* push imm32 */
    if (p[0] == 0x8B && (p[1] & 0xC7) == 0x44 && p[2] == 0x24) return 4; /* mov r,[esp+d8] */
    if (p[0] == 0x8B && (p[1] & 0xC7) == 0x45) return 3;     /* mov r,[ebp+d8] */
    return 0;
}

/* instala; devolve a funcao original (trampolim) ou NULL */
static void *hook_install(void *target, void *replacement)
{
    BYTE *t = (BYTE *)target, *tramp;
    int n = 0, l;
    DWORD old;
    if (!t) return NULL;
    /* segue "jmp [iat]" (stubs de kernel32 -> kernelbase) */
    if (t[0] == 0xFF && t[1] == 0x25) t = **(BYTE ***)(t + 2);
    if (t[0] == 0x8B && t[1] == 0xFF && t[2] == 0x55 && t[3] == 0x8B && t[4] == 0xEC && t[5] == 0x5D &&
        t[6] == 0xFF && t[7] == 0x25)
        t = **(BYTE ***)(t + 8);                            /* stub "mov edi,edi; push ebp; mov ebp,esp; pop ebp; jmp [x]" */
    while (n < 5) {
        l = hook_insn_len(t + n);
        if (!l) return NULL;
        n += l;
    }
    if (!(tramp = hook_alloc())) return NULL;
    memcpy(tramp, t, n);
    tramp[n] = 0xE9;
    *(DWORD *)(tramp + n + 1) = (DWORD)(t + n) - (DWORD)(tramp + n + 5);
    if (!VirtualProtect(t, 8, PAGE_EXECUTE_READWRITE, &old)) return NULL;
    t[0] = 0xE9;
    *(DWORD *)(t + 1) = (DWORD)replacement - (DWORD)(t + 5);
    for (l = 5; l < n; l++) t[l] = 0x90;
    VirtualProtect(t, 8, old, &old);
    FlushInstructionCache(GetCurrentProcess(), t, 8);
    return tramp;
}

static void *hook_export(const char *dll, const char *name, void *replacement)
{
    HMODULE m = GetModuleHandleA(dll);
    void *f = m ? (void *)GetProcAddress(m, name) : NULL;
    return f ? hook_install(f, replacement) : NULL;
}
#endif
