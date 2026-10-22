/* D2NXPad - controle estilo Diablo II Resurrected (Nintendo Switch) para Diablo II LoD.
 *
 * Substitui a D2Pad antiga (binaria, sem fonte). E carregada:
 *   - pelo D2SE (ModDll1=D2Pad.dll) ou pela PlugY (DllToLoad=D2Pad.dll) dos mods, porque o
 *     D2Autorun.dll redireciona qualquer "D2Pad.dll" para este arquivo;
 *   - pelo D2Autorun.dll nos cores vanilla.
 * Le o controle por XInput (o AUTORUN entrega o Joy-Con/Pro Controller como Xbox 360, com
 * botoes posicionais) e conversa com o jogo como um mouse/teclado (SendInput), lendo a memoria
 * do jogo para mirar (versoes 1.09b-1.13c). Em versoes desconhecidas funciona como cursor.
 *
 * Copyright 2026. Licenca MIT. */
#include "d2nxpad.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <wchar.h>

#define PAD_VERSION "1.1.1"

/* ======================================================================================
 * utilidades
 * ==================================================================================== */
static char g_dir[MAX_PATH];        /* pasta deste DLL, com barra final */
static char g_ini[MAX_PATH];
static FILE *g_log;
static int g_debug;
static HMODULE g_self;
static volatile LONG g_running;

static void lg(const char *fmt, ...)
{
    va_list ap; SYSTEMTIME t;
    if (!g_log) return;
    GetLocalTime(&t);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    va_start(ap, fmt); vfprintf(g_log, fmt, ap); va_end(ap);
    fputc('\n', g_log); fflush(g_log);
}
#define dbg(...) do { if (g_debug) lg(__VA_ARGS__); } while (0)

static float clampf(float v, float a, float b) { return v < a ? a : v > b ? b : v; }

/* ---- leitura segura da memoria do jogo (cache de regioes legiveis) ---- */
typedef struct { DWORD lo, hi; } Region;
static Region g_regions[96];
static int g_nregions;
static DWORD g_regions_tick;

static int readable(DWORD addr, DWORD size)
{
    MEMORY_BASIC_INFORMATION mbi;
    int i;
    if (addr < 0x10000 || addr + size < addr) return 0;
    for (i = 0; i < g_nregions; i++)
        if (addr >= g_regions[i].lo && addr + size <= g_regions[i].hi) return 1;
    if (!VirtualQuery((void *)addr, &mbi, sizeof(mbi))) return 0;
    if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
                         PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return 0;
    if (g_nregions < (int)(sizeof(g_regions) / sizeof(g_regions[0]))) {
        g_regions[g_nregions].lo = (DWORD)mbi.BaseAddress;
        g_regions[g_nregions].hi = (DWORD)mbi.BaseAddress + mbi.RegionSize;
        g_nregions++;
    }
    return addr + size <= (DWORD)mbi.BaseAddress + mbi.RegionSize;
}
static DWORD rd32(DWORD addr) { return readable(addr, 4) ? *(volatile DWORD *)addr : 0; }
static WORD rd16(DWORD addr) { return readable(addr, 2) ? *(volatile WORD *)addr : 0; }

/* ======================================================================================
 * configuracao
 * ==================================================================================== */
enum Btn { B_A, B_B, B_X, B_Y, B_L, B_R, B_ZL, B_ZR, B_PLUS, B_MINUS, B_L3, B_R3,
           B_UP, B_DOWN, B_LEFT, B_RIGHT, B_COUNT };
static const char *btn_names[B_COUNT] = { "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "Plus", "Minus",
                                          "L3", "R3", "Up", "Down", "Left", "Right" };
enum ActKind { A_NONE, A_ATTACK, A_SKILL, A_RIGHT, A_KEY, A_HOLD, A_LCLICK, A_RCLICK, A_SHIFTCLICK,
               A_CTRLCLICK, A_SHIFTRCLICK, A_LAYER, A_PLUS, A_NAV, A_POPUP_R, A_POPUP_L, A_TYPENAME };
typedef struct { BYTE kind; BYTE vk; BYTE mods; BYTE n; } Act;  /* mods: 1 shift 2 ctrl 4 alt */
enum Ctx { C_GAME, C_LAYER, C_PLUS, C_PANEL, C_MENU, C_COUNT };
static const char *ctx_names[C_COUNT] = { "Jogo", "Jogo.ZL", "Jogo.Mais", "Painel", "Menu" };
static Act g_bind[C_COUNT][B_COUNT];

static struct {
    int enabled, pad_index;
    float dz_left, dz_right, trigger;
    int walk_radius, center_dy, always_run;
    int aim, aim_range, aim_cone, melee_range, interact_range, body_offset, ranged_distance;
    int stand_attack, repeat_ms;
    float cursor_speed, cursor_curve;
    int nav_step, nav_delay, nav_rate;
    int panel_shift;
    int skill_delay_ms;
    int autojoin, autowalk;
    float xp_multiplier, rune_multiplier, unique_multiplier, rare_multiplier;
    int start_gold;
} cfg;

static const char *default_ini =
"; ============================================================================\n"
"; [Gameplay] - Modificadores de Experiencia, Queda de Itens e Ouro\n"
"; ============================================================================\n"
"[Gameplay]\n"
"MultiplicadorXP=2.0\n"
"MultiplicadorRunas=2.0\n"
"MultiplicadorUnicos=1.5\n"
"MultiplicadorRaros=1.5\n"
"OuroInicial=10000\n"
"\n"
"; ============================================================================\n"
"; D2NXPad " PAD_VERSION " - controle estilo Diablo II Resurrected (Nintendo Switch)\n"
"; Botoes com nomes do Switch (posicionais). Apague para restaurar o padrao.\n"
"; Acoes: ATTACK SKILL:n RIGHT KEY:<tecla> KEY:SHIFT+<tecla> HOLD:<tecla> LCLICK RCLICK\n"
";        SHIFTCLICK CTRLCLICK SHIFTRCLICK LAYER PLUS NAV POPUP_R POPUP_L NONE\n"
"; Teclas: A-Z 0-9 F1-F12 TAB SPACE ENTER ESC SHIFT CTRL ALT UP DOWN LEFT RIGHT\n"
"; ============================================================================\n"
"[Geral]\n"
"Ativo=1\n"
"; -1 = primeiro controle conectado\n"
"Controle=-1\n"
"Debug=0\n"
"\n"
"[Movimento]\n"
"ZonaMortaEsquerda=0.20\n"
"; distancia do ponto de andar, em pixels do jogo\n"
"RaioAndar=120\n"
"; altura do centro do personagem em relacao ao meio da tela (pixels)\n"
"AjusteCentroY=-28\n"
"; 1 = entra no jogo correndo (L3 alterna andar/correr)\n"
"SempreCorrer=1\n"
"\n"
"[Mira]\n"
"; mira automatica no monstro mais proximo na direcao do analogico (como no D2R)\n"
"MiraAssistida=1\n"
"Alcance=25\n"
"Cone=110\n"
"CorpoACorpo=4\n"
"Interagir=12\n"
"AlturaCorpo=26\n"
"DistanciaSemAlvo=160\n"
"; sem alvo: ataca parado (Shift) na direcao em que olha\n"
"AtacarParado=1\n"
"RepetirMs=350\n"
"AtrasoHabilidadeMs=40\n"
"\n"
"[Cursor]\n"
"ZonaMortaDireita=0.18\n"
"; telas por segundo\n"
"Velocidade=1.2\n"
"Curva=2.0\n"
"Gatilho=0.35\n"
"; direcional nos paineis: tamanho do passo (uma casa do inventario = 29)\n"
"PassoNav=29\n"
"AtrasoNav=260\n"
"RepeticaoNav=90\n"
"; deslocamento da camera com painel aberto (0 = automatico)\n"
"DeslocamentoPainel=0\n"
"\n"
"; ---- Jogo (igual ao D2R do Switch) ----\n"
"; B = habilidade 2 / interagir (ataque principal). Y, A, X, R, ZR = habilidades 1, 3, 4, 5, 6.\n"
"; ZL segurado = segunda barra. L = mostrar itens. Direcional = cinto. + = menu, - = mapa.\n"
"; As habilidades usam as teclas F1-F8 do jogo: na tela de habilidades (+ e ZR), passe o cursor\n"
"; sobre a habilidade e aperte o botao para gravar.\n"
"[Jogo]\n"
"B=ATTACK\n"
"Y=SKILL:1\n"
"A=SKILL:2\n"
"X=SKILL:3\n"
"R=SKILL:4\n"
"ZR=SKILL:5\n"
"ZL=LAYER\n"
"L=HOLD:ALT\n"
"Plus=PLUS\n"
"Minus=KEY:TAB\n"
"L3=KEY:R\n"
"R3=KEY:W\n"
"Left=KEY:1\n"
"Right=KEY:2\n"
"Down=KEY:3\n"
"Up=KEY:4\n"
"\n"
"; segurando ZL\n"
"[Jogo.ZL]\n"
"Y=SKILL:6\n"
"A=SKILL:7\n"
"X=SKILL:8\n"
"R=RIGHT\n"
"ZR=RIGHT\n"
"B=ATTACK\n"
"Left=KEY:SHIFT+1\n"
"Right=KEY:SHIFT+2\n"
"Down=KEY:SHIFT+3\n"
"Up=KEY:SHIFT+4\n"
"\n"
"; segurando + (toque rapido no + = ESC/menu do jogo)\n"
"[Jogo.Mais]\n"
"Y=KEY:I\n"
"X=KEY:C\n"
"A=KEY:T\n"
"B=KEY:Q\n"
"R=KEY:O\n"
"L=KEY:P\n"
"ZR=POPUP_R\n"
"ZL=POPUP_L\n"
"Up=KEY:H\n"
"Down=KEY:M\n"
"Left=KEY:K\n"
"Right=KEY:ENTER\n"
"\n"
"; inventario, bau, cubo, loja, troca, mercenario, habilidades, personagem\n"
"[Painel]\n"
"A=LCLICK\n"
"Y=RCLICK\n"
"X=SHIFTCLICK\n"
"B=KEY:ESC\n"
"L=CTRLCLICK\n"
"R=SHIFTRCLICK\n"
"ZL=LAYER\n"
"ZR=RCLICK\n"
"Plus=KEY:ESC\n"
"Minus=KEY:TAB\n"
"L3=KEY:I\n"
"R3=KEY:C\n"
"Up=NAV\n"
"Down=NAV\n"
"Left=NAV\n"
"Right=NAV\n"
"\n"
"; fora da partida (menus, selecao de personagem)\n"
"[Menu]\n"
"A=LCLICK\n"
"Y=RCLICK\n"
"B=KEY:ESC\n"
"X=KEY:ENTER\n"
"Plus=KEY:ENTER\n"
"Minus=KEY:ESC\n"
"L=KEY:SHIFT\n"
"R=KEY:TAB\n"
"L3=TYPENAME\n"
"R3=TYPENAME\n"
"Up=KEY:UP\n"
"Down=KEY:DOWN\n"
"Left=KEY:LEFT\n"
"Right=KEY:RIGHT\n"
"\n"
"; valores em hexadecimal (RVA no D2Client.dll) para versoes desconhecidas/modificadas\n"
"[Offsets]\n"
"; Versao=1.13c\n";

static int key_from_name(const char *s)
{
    static const struct { const char *n; int vk; } k[] = {
        {"TAB", VK_TAB}, {"SPACE", VK_SPACE}, {"ENTER", VK_RETURN}, {"ESC", VK_ESCAPE},
        {"SHIFT", VK_SHIFT}, {"CTRL", VK_CONTROL}, {"ALT", VK_MENU}, {"UP", VK_UP}, {"DOWN", VK_DOWN},
        {"LEFT", VK_LEFT}, {"RIGHT", VK_RIGHT}, {"BACKSPACE", VK_BACK}, {"HOME", VK_HOME},
        {"END", VK_END}, {"PGUP", VK_PRIOR}, {"PGDN", VK_NEXT}, {"INSERT", VK_INSERT}, {"DELETE", VK_DELETE},
    };
    unsigned i;
    if (!s[0]) return 0;
    if (!s[1] && ((s[0] >= 'A' && s[0] <= 'Z') || (s[0] >= '0' && s[0] <= '9'))) return s[0];
    if ((s[0] == 'F' || s[0] == 'f') && s[1] >= '1' && s[1] <= '9') {
        int n = atoi(s + 1);
        if (n >= 1 && n <= 16) return VK_F1 + n - 1;
    }
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) return (int)strtol(s, NULL, 16) & 0xff;
    for (i = 0; i < sizeof(k) / sizeof(k[0]); i++) if (!lstrcmpiA(s, k[i].n)) return k[i].vk;
    return 0;
}

static Act parse_act(const char *v)
{
    Act a = { A_NONE, 0, 0, 0 };
    char buf[64], *p;
    lstrcpynA(buf, v, sizeof(buf));
    for (p = buf; *p; p++) if (*p == ' ' || *p == '\t' || *p == ';') { *p = 0; break; }
    if (!lstrcmpiA(buf, "ATTACK")) a.kind = A_ATTACK;
    else if (!strncasecmp(buf, "SKILL:", 6)) { a.kind = A_SKILL; a.n = (BYTE)atoi(buf + 6); }
    else if (!lstrcmpiA(buf, "RIGHT")) a.kind = A_RIGHT;
    else if (!lstrcmpiA(buf, "LCLICK")) a.kind = A_LCLICK;
    else if (!lstrcmpiA(buf, "RCLICK")) a.kind = A_RCLICK;
    else if (!lstrcmpiA(buf, "SHIFTCLICK")) a.kind = A_SHIFTCLICK;
    else if (!lstrcmpiA(buf, "CTRLCLICK")) a.kind = A_CTRLCLICK;
    else if (!lstrcmpiA(buf, "SHIFTRCLICK")) a.kind = A_SHIFTRCLICK;
    else if (!lstrcmpiA(buf, "LAYER")) a.kind = A_LAYER;
    else if (!lstrcmpiA(buf, "PLUS")) a.kind = A_PLUS;
    else if (!lstrcmpiA(buf, "NAV")) a.kind = A_NAV;
    else if (!lstrcmpiA(buf, "POPUP_R")) a.kind = A_POPUP_R;
    else if (!lstrcmpiA(buf, "POPUP_L")) a.kind = A_POPUP_L;
    else if (!lstrcmpiA(buf, "TYPENAME")) a.kind = A_TYPENAME;
    else if (!strncasecmp(buf, "KEY:", 4) || !strncasecmp(buf, "HOLD:", 5)) {
        char *k = buf + (buf[0] == 'K' || buf[0] == 'k' ? 4 : 5);
        a.kind = (buf[0] == 'K' || buf[0] == 'k') ? A_KEY : A_HOLD;
        for (;;) {
            if (!strncasecmp(k, "SHIFT+", 6)) { a.mods |= 1; k += 6; }
            else if (!strncasecmp(k, "CTRL+", 5)) { a.mods |= 2; k += 5; }
            else if (!strncasecmp(k, "ALT+", 4)) { a.mods |= 4; k += 4; }
            else break;
        }
        a.vk = (BYTE)key_from_name(k);
        if (!a.vk) a.kind = A_NONE;
    }
    return a;
}

static int ini_int(const char *sec, const char *key, int def)
{ return GetPrivateProfileIntA(sec, key, def, g_ini); }
static float ini_float(const char *sec, const char *key, float def)
{
    char b[32], d[32];
    snprintf(d, sizeof(d), "%.3f", def);
    GetPrivateProfileStringA(sec, key, d, b, sizeof(b), g_ini);
    return (float)atof(b);
}
static DWORD ini_hex(const char *sec, const char *key, DWORD def)
{
    char b[32];
    GetPrivateProfileStringA(sec, key, "", b, sizeof(b), g_ini);
    return b[0] ? strtoul(b, NULL, 16) : def;
}

static void init_gameplay_menu_indices(void);

static void load_config(void)
{
    int c, b;
    if (GetFileAttributesA(g_ini) == INVALID_FILE_ATTRIBUTES) {
        FILE *f = fopen(g_ini, "wb");
        if (f) { fputs(default_ini, f); fclose(f); }
    }
    cfg.enabled = ini_int("Geral", "Ativo", 1);
    cfg.pad_index = ini_int("Geral", "Controle", -1);
    g_debug = ini_int("Geral", "Debug", 0);
    cfg.dz_left = ini_float("Movimento", "ZonaMortaEsquerda", 0.20f);
    cfg.walk_radius = ini_int("Movimento", "RaioAndar", 120);
    cfg.center_dy = ini_int("Movimento", "AjusteCentroY", -28);
    cfg.always_run = ini_int("Movimento", "SempreCorrer", 1);
    cfg.aim = ini_int("Mira", "MiraAssistida", 1);
    cfg.aim_range = ini_int("Mira", "Alcance", 25);
    cfg.aim_cone = ini_int("Mira", "Cone", 110);
    cfg.melee_range = ini_int("Mira", "CorpoACorpo", 4);
    cfg.interact_range = ini_int("Mira", "Interagir", 12);
    cfg.body_offset = ini_int("Mira", "AlturaCorpo", 26);
    cfg.ranged_distance = ini_int("Mira", "DistanciaSemAlvo", 160);
    cfg.stand_attack = ini_int("Mira", "AtacarParado", 1);
    cfg.repeat_ms = ini_int("Mira", "RepetirMs", 350);
    cfg.skill_delay_ms = ini_int("Mira", "AtrasoHabilidadeMs", 40);
    cfg.dz_right = ini_float("Cursor", "ZonaMortaDireita", 0.18f);
    cfg.cursor_speed = ini_float("Cursor", "Velocidade", 1.2f);
    cfg.cursor_curve = ini_float("Cursor", "Curva", 2.0f);
    cfg.trigger = ini_float("Cursor", "Gatilho", 0.35f);
    cfg.nav_step = ini_int("Cursor", "PassoNav", 29);
    cfg.nav_delay = ini_int("Cursor", "AtrasoNav", 260);
    cfg.nav_rate = ini_int("Cursor", "RepeticaoNav", 90);
    cfg.panel_shift = ini_int("Cursor", "DeslocamentoPainel", 0);
    cfg.autojoin = ini_int("AutoTest", "AutoJoin", 0);
    cfg.autowalk = ini_int("AutoTest", "AutoWalk", 0);
    if (cfg.autojoin || cfg.autowalk) lg("AutoTest ativo: autojoin=%d autowalk=%d", cfg.autojoin, cfg.autowalk);
    cfg.xp_multiplier = ini_float("Gameplay", "MultiplicadorXP", 2.0f);
    cfg.rune_multiplier = ini_float("Gameplay", "MultiplicadorRunas", 2.0f);
    cfg.unique_multiplier = ini_float("Gameplay", "MultiplicadorUnicos", 1.5f);
    cfg.rare_multiplier = ini_float("Gameplay", "MultiplicadorRaros", 1.5f);
    cfg.start_gold = ini_int("Gameplay", "OuroInicial", 10000);
    lg("Gameplay ativo: XP=%.1fx, Runas=%.1fx, Unicos=%.1fx, Raros=%.1fx, Ouro=%d",
       cfg.xp_multiplier, cfg.rune_multiplier, cfg.unique_multiplier, cfg.rare_multiplier, cfg.start_gold);
    for (c = 0; c < C_COUNT; c++)
        for (b = 0; b < B_COUNT; b++) {
            char v[64];
            GetPrivateProfileStringA(ctx_names[c], btn_names[b], "", v, sizeof(v), g_ini);
            g_bind[c][b] = parse_act(v);
        }
    /* offsets manuais */
    g_ver_override.name = NULL;
    {
        static char vname[16];
        GetPrivateProfileStringA("Offsets", "Versao", "", vname, sizeof(vname), g_ini);
        if (vname[0]) g_ver_override.name = vname;
    }
    g_ver_override.res_x = ini_hex("Offsets", "ResX", 0);
    g_ver_override.res_y = ini_hex("Offsets", "ResY", 0);
    g_ver_override.mouse_x = ini_hex("Offsets", "MouseX", 0);
    g_ver_override.mouse_y = ini_hex("Offsets", "MouseY", 0);
    g_ver_override.player = ini_hex("Offsets", "Jogador", 0);
    g_ver_override.ui_vars = ini_hex("Offsets", "Paineis", 0);
    g_ver_override.print_msg = ini_hex("Offsets", "PrintMsg", 0);
    init_gameplay_menu_indices();
}

/* ======================================================================================
 * entrada (XInput)
 * ==================================================================================== */
typedef struct { WORD wButtons; BYTE bLeftTrigger, bRightTrigger; SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY; } XPad;
typedef struct { DWORD dwPacketNumber; XPad Gamepad; } XState;
typedef DWORD (WINAPI *XInputGetState_t)(DWORD, XState *);
static XInputGetState_t pXInputGetState;

typedef struct { int down[B_COUNT]; float lx, ly, rx, ry; int connected; } Pad;
static Pad g_pad, g_prev;
static DWORD g_press_time[B_COUNT];

static void load_xinput(void)
{
    static const char *dlls[] = {
        ".\\xinput1_4.dll", "d2nx\\xinput1_4.dll",
        "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll", "xinput1_2.dll"
    };
    unsigned i;
    for (i = 0; i < sizeof(dlls) / sizeof(dlls[0]); i++) {
        HMODULE h = LoadLibraryA(dlls[i]);
        if (h && (pXInputGetState = (XInputGetState_t)GetProcAddress(h, "XInputGetState"))) {
            lg("XInput: %s (modulo %p)", dlls[i], h);
            return;
        }
    }
    lg("XInput indisponivel");
}

static float stick(SHORT v, int invert) { float f = v / 32767.0f; f = clampf(f, -1, 1); return invert ? -f : f; }

static void poll_pad(void)
{
    XState st;
    DWORD i, from = 0, to = 4;
    g_prev = g_pad;
    memset(&g_pad, 0, sizeof(g_pad));
    if (!pXInputGetState) return;
    if (cfg.pad_index >= 0 && cfg.pad_index < 4) { from = cfg.pad_index; to = from + 1; }
    for (i = from; i < to; i++) {
        memset(&st, 0, sizeof(st));
        if (pXInputGetState(i, &st) == ERROR_SUCCESS) {
            WORD w = st.Gamepad.wButtons;
            g_pad.connected = 1;
            /* AUTORUN: Switch B -> Xbox A, Switch A -> Xbox B, Switch Y -> Xbox X, Switch X -> Xbox Y */
            g_pad.down[B_B] = !!(w & 0x1000);
            g_pad.down[B_A] = !!(w & 0x2000);
            g_pad.down[B_Y] = !!(w & 0x4000);
            g_pad.down[B_X] = !!(w & 0x8000);
            g_pad.down[B_L] = !!(w & 0x0100);
            g_pad.down[B_R] = !!(w & 0x0200);
            g_pad.down[B_PLUS] = !!(w & 0x0010);
            g_pad.down[B_MINUS] = !!(w & 0x0020);
            g_pad.down[B_L3] = !!(w & 0x0040);
            g_pad.down[B_R3] = !!(w & 0x0080);
            g_pad.down[B_UP] = !!(w & 0x0001);
            g_pad.down[B_DOWN] = !!(w & 0x0002);
            g_pad.down[B_LEFT] = !!(w & 0x0004);
            g_pad.down[B_RIGHT] = !!(w & 0x0008);
            g_pad.down[B_ZL] = st.Gamepad.bLeftTrigger > cfg.trigger * 255;
            g_pad.down[B_ZR] = st.Gamepad.bRightTrigger > cfg.trigger * 255;
            g_pad.lx = stick(st.Gamepad.sThumbLX, 0);
            g_pad.ly = stick(st.Gamepad.sThumbLY, 1);   /* y para baixo, como na tela */
            g_pad.rx = stick(st.Gamepad.sThumbRX, 0);
            g_pad.ry = stick(st.Gamepad.sThumbRY, 1);
            break;
        }
    }
    for (i = 0; i < B_COUNT; i++) if (g_pad.down[i] && !g_prev.down[i]) g_press_time[i] = GetTickCount();
}
#define PRESSED(b) (g_pad.down[b] && !g_prev.down[b])
#define RELEASED(b) (!g_pad.down[b] && g_prev.down[b])

/* ======================================================================================
 * saida (SendInput) e coordenadas
 * ==================================================================================== */
static HWND g_hwnd;
static const D2Ver *g_ver;
static DWORD g_client;      /* base do D2Client.dll */
static int g_resx = 800, g_resy = 600;
/* tela = origem + jogo * escala */
static float g_ox, g_oy, g_sx = 1, g_sy = 1;
static DWORD g_player;
static int g_cal_ok, g_cal_step, g_cal_wait;
static DWORD g_cal_retry;
static int g_cal_fail, g_cal_real;
static RECT g_cal_rect;
static int g_cal_resx, g_cal_resy;
static float g_cal_g1x, g_cal_g1y;
static POINT g_cal_s1, g_cal_s2;
static int g_lbtn, g_rbtn;       /* botoes do mouse que NOS estamos segurando */
static int g_key_down[256];

static void send_key(int vk, int down)
{
    INPUT in;
    memset(&in, 0, sizeof(in));
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = (WORD)vk;
    in.ki.wScan = (WORD)MapVirtualKeyA(vk, 0);
    in.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    if (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT || vk == VK_INSERT ||
        vk == VK_DELETE || vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT)
        in.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
    SendInput(1, &in, sizeof(in));
    keybd_event((BYTE)vk, (BYTE)in.ki.wScan, in.ki.dwFlags, 0);
    g_key_down[vk & 0xff] = down;
}
static void key_mods(int mods, int down)
{
    if (mods & 1) send_key(VK_SHIFT, down);
    if (mods & 2) send_key(VK_CONTROL, down);
    if (mods & 4) send_key(VK_MENU, down);
}
static void tap_key(int vk, int mods)
{
    key_mods(mods, 1);
    send_key(vk, 1);
    if (g_hwnd) {
        PostMessageA(g_hwnd, WM_KEYDOWN, vk, 0);
        if (vk >= 'A' && vk <= 'Z') PostMessageA(g_hwnd, WM_CHAR, (mods & 1) ? vk : (vk + 32), 0);
        else if (vk >= '0' && vk <= '9') PostMessageA(g_hwnd, WM_CHAR, vk, 0);
        else if (vk == VK_RETURN) PostMessageA(g_hwnd, WM_CHAR, '\r', 0);
    }
    Sleep(35);
    send_key(vk, 0);
    if (g_hwnd) {
        PostMessageA(g_hwnd, WM_KEYUP, vk, 0xC0000001);
    }
    key_mods(mods, 0);
}

static void type_string(const char *s)
{
    while (*s) {
        char c = *s++;
        int vk = 0, shift = 0;
        if (c >= 'a' && c <= 'z') { vk = c - 'a' + 'A'; shift = 0; }
        else if (c >= 'A' && c <= 'Z') { vk = c; shift = 1; }
        else if (c >= '0' && c <= '9') { vk = c; shift = 0; }
        if (vk) {
            tap_key(vk, shift ? 1 : 0);
            Sleep(40);
        }
    }
}

static void send_mouse_abs(int sx, int sy)
{
    SetCursorPos(sx, sy);
    INPUT in;
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    if (vw <= 1) vw = GetSystemMetrics(SM_CXSCREEN);
    if (vh <= 1) vh = GetSystemMetrics(SM_CYSCREEN);
    memset(&in, 0, sizeof(in));
    in.type = INPUT_MOUSE;
    in.mi.dx = (LONG)(((sx - vx) * 65535.0 + vw / 2) / (vw - 1));
    in.mi.dy = (LONG)(((sy - vy) * 65535.0 + vh / 2) / (vh - 1));
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &in, sizeof(in));
}
static void send_button(int right, int down)
{
    INPUT in;
    memset(&in, 0, sizeof(in));
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = right ? (down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP)
                          : (down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP);
    SendInput(1, &in, sizeof(in));
    if (right) g_rbtn = down; else g_lbtn = down;
}

/* posicao do mouse dentro do jogo, lida da memoria (-1 se desconhecido) */
typedef DWORD (__stdcall *fn_void_t)(void);
static int game_mouse(int *x, int *y)
{
    if (!g_ver || !g_client) return 0;
    if (g_ver->mouse_x && g_ver->mouse_y) {
        *x = (int)rd32(g_client + g_ver->mouse_x);
        *y = (int)rd32(g_client + g_ver->mouse_y);
        return 1;
    }
    if (g_ver->f_mouse_x && g_ver->f_mouse_y) {
        *x = (int)((fn_void_t)(g_client + g_ver->f_mouse_x))();
        *y = (int)((fn_void_t)(g_client + g_ver->f_mouse_y))();
        return 1;
    }
    return 0;
}

static void read_resolution(void)
{
    if (!g_player) { g_resx = 800; g_resy = 600; return; }   /* menus do LoD: 800x600 */
    if (g_ver && g_ver->res_x && g_ver->res_y) {
        int x = (int)rd32(g_client + g_ver->res_x), y = (int)rd32(g_client + g_ver->res_y);
        if (x >= 640 && x <= 4096 && y >= 480 && y <= 4096) { g_resx = x; g_resy = y; }
    }
}

/* mapeamento padrao: area do jogo centralizada na janela mantendo a proporcao */
static void fallback_mapping(void)
{
    RECT rc; POINT p = { 0, 0 };
    float s, w, h;
    GetClientRect(g_hwnd, &rc);
    ClientToScreen(g_hwnd, &p);
    w = (float)(rc.right - rc.left); h = (float)(rc.bottom - rc.top);
    if (w < 16 || h < 16) { w = (float)g_resx; h = (float)g_resy; }
    if (w >= 1200 && h >= 700 && fabsf((w / h) - (16.0f / 9.0f)) < 0.2f) {
        /* 16:9 widescreen no Switch (ex: 1280x720) com D2GL esticado: preenche tela inteira sem offset fantasma */
        g_sx = w / (float)g_resx;
        g_sy = h / (float)g_resy;
        g_ox = (float)p.x;
        g_oy = (float)p.y;
    } else {
        s = w / g_resx < h / g_resy ? w / g_resx : h / g_resy;
        g_sx = g_sy = s;
        g_ox = p.x + (w - g_resx * s) / 2;
        g_oy = p.y + (h - g_resy * s) / 2;
    }
}

static void game_to_screen(float gx, float gy, int *sx, int *sy)
{
    gx = clampf(gx, 1, g_resx - 2);
    gy = clampf(gy, 1, g_resy - 2);
    *sx = (int)lroundf(g_ox + gx * g_sx);
    *sy = (int)lroundf(g_oy + gy * g_sy);
}

static float g_curx = 400, g_cury = 300;   /* cursor que controlamos, em coordenadas do jogo */
static POINT g_last_set = { -1, -1 };
/* o mouse tambem pode ser movido pela tela de toque do AUTORUN: segue a posicao real */
static void sync_cursor(void)
{
    POINT p;
    if (!GetCursorPos(&p) || g_sx <= 0 || g_sy <= 0) return;
    if (abs(p.x - g_last_set.x) > 2 || abs(p.y - g_last_set.y) > 2) {
        g_curx = clampf((p.x - g_ox) / g_sx, 1, g_resx - 2);
        g_cury = clampf((p.y - g_oy) / g_sy, 1, g_resy - 2);
        g_last_set = p;
    }
}
static void move_cursor(float gx, float gy)
{
    int sx, sy;
    g_curx = clampf(gx, 1, g_resx - 2);
    g_cury = clampf(gy, 1, g_resy - 2);
    game_to_screen(g_curx, g_cury, &sx, &sy);
    send_mouse_abs(sx, sy);
    g_last_set.x = sx; g_last_set.y = sy;
}

/* Calibracao: move o mouse do sistema para dois pontos da janela e le onde o jogo o viu.
 * Funciona com qualquer wrapper (D2GL, Glide/Sven, DirectDraw do Wine, resolucoes do
 * SGD2FreeRes) porque nao depende de como a imagem e escalada. Maquina de estados por tick. */
static void calibrate_tick(void)
{
    RECT rc; POINT p = { 0, 0 };
    int gx, gy;
    GetClientRect(g_hwnd, &rc);
    ClientToScreen(g_hwnd, &p);
    OffsetRect(&rc, p.x, p.y);
    {
        static int had_player;
        if (!!g_player != had_player) {      /* entrou/saiu da partida: as variaveis do mouse mudam */
            had_player = !!g_player;
            if (had_player && !g_cal_real) { g_cal_ok = 0; g_cal_fail = 0; g_cal_step = 0; }
        }
    }
    {   /* dentro da partida sem calibracao real: tenta de novo a cada 8 s */
        static DWORD last_try;
        if (g_cal_ok && !g_cal_real && g_player && g_ver && (g_ver->mouse_x || g_ver->f_mouse_x) &&
            GetTickCount() - last_try > 8000 && !g_lbtn && !g_rbtn &&
            fabsf(g_pad.lx) < 0.2f && fabsf(g_pad.ly) < 0.2f && fabsf(g_pad.rx) < 0.2f && fabsf(g_pad.ry) < 0.2f) {
            last_try = GetTickCount();
            g_cal_ok = 0; g_cal_fail = 2; g_cal_step = 0;
        }
    }
    if (g_cal_ok && EqualRect(&rc, &g_cal_rect) && g_cal_resx == g_resx && g_cal_resy == g_resy) return;
    if (!g_player || !g_ver || (!g_ver->mouse_x && !g_ver->f_mouse_x)) {
        fallback_mapping();
        g_cal_ok = 1; g_cal_rect = rc; g_cal_resx = g_resx; g_cal_resy = g_resy;
        return;
    }
    if (g_cal_wait > 0) { g_cal_wait--; return; }
    if (g_cal_retry && (int)(GetTickCount() - g_cal_retry) < 0) return;
    g_cal_retry = 0;
    switch (g_cal_step) {
    case 0:
        fallback_mapping();
        g_cal_s1.x = rc.left + (rc.right - rc.left) * 3 / 10;
        g_cal_s1.y = rc.top + (rc.bottom - rc.top) * 3 / 10;
        send_mouse_abs(g_cal_s1.x, g_cal_s1.y);
        g_cal_step = 1; g_cal_wait = 10;
        break;
    case 1:
        if (!game_mouse(&gx, &gy)) { g_cal_step = 0; g_cal_ok = 1; g_cal_rect = rc; return; }
        g_cal_g1x = (float)gx; g_cal_g1y = (float)gy;
        g_cal_s2.x = rc.left + (rc.right - rc.left) * 7 / 10;
        g_cal_s2.y = rc.top + (rc.bottom - rc.top) * 7 / 10;
        send_mouse_abs(g_cal_s2.x, g_cal_s2.y);
        g_cal_step = 2; g_cal_wait = 10;
        break;
    case 2: {
        float sx, sy;
        game_mouse(&gx, &gy);
        g_cal_step = 0;
        if (fabsf(gx - g_cal_g1x) < 20 || fabsf(gy - g_cal_g1y) < 20) {
            static int warned;
            if (!g_player) { g_cal_fail = 4; }   /* menus: variaveis do mouse nao existem */
            if (!warned++) lg("Calibracao sem resposta (%.0f,%.0f -> %d,%d): tentando de novo", g_cal_g1x, g_cal_g1y, gx, gy);
            fallback_mapping();
            g_cal_retry = GetTickCount() + 1500;
            if (++g_cal_fail < 4) return;
            lg("Calibracao: usando proporcao da janela");
        } else {
            sx = (g_cal_s2.x - g_cal_s1.x) / (gx - g_cal_g1x);
            sy = (g_cal_s2.y - g_cal_s1.y) / (gy - g_cal_g1y);
            if (sx < 0.2f || sx > 8 || sy < 0.2f || sy > 8) {
                lg("Calibracao invalida (%.3f x %.3f)", sx, sy);
                fallback_mapping();
            } else {
                g_sx = sx; g_sy = sy; g_cal_real = 1;
                g_ox = g_cal_s1.x - g_cal_g1x * sx;
                g_oy = g_cal_s1.y - g_cal_g1y * sy;
                lg("Calibrado: janela %ldx%ld, jogo %dx%d, escala %.3f x %.3f, origem %.1f,%.1f",
                   rc.right - rc.left, rc.bottom - rc.top, g_resx, g_resy, g_sx, g_sy, g_ox, g_oy);
            }
        }
        g_cal_ok = 1; g_cal_rect = rc; g_cal_resx = g_resx; g_cal_resy = g_resy;
        move_cursor(g_curx, g_cury);
        break; }
    }
}

/* ======================================================================================
 * leitura do jogo
 * ==================================================================================== */
static DWORD g_room_next_off;      /* 0xE4/0xE8 (1.10+) ou 0x10C (1.07-1.09), detectado */
/* layout da estrutura Unit: 1.10+ tem pMemPool em +8; 1.07-1.09 nao tem e o caminho fica em +0x38 */
static DWORD U_MODE = 0x10, U_PATH = 0x2C, U_FLAGS = 0xC4, SP_ROOM = 0x00, SP_X = 0x0C, SP_Y = 0x10;
static void unit_layout_init(void)
{
    if (g_ver && g_ver->room_fmt == 0 && g_ver->client_ts && g_ver->client_ts < 0x3f000000) { /* 1.07-1.09d */
        U_MODE = 0x0C; U_PATH = 0x38; U_FLAGS = 0xE8;
        SP_ROOM = 0x1C; SP_X = 0x08; SP_Y = 0x0C;       /* caminho estatico (objetos/itens) */
    }
}
static DWORD player_unit(void)
{
    DWORD p = 0;
    if (!g_ver || !g_client) return 0;
    if (g_ver->player) p = rd32(g_client + g_ver->player);
    else if (g_ver->f_player) p = ((fn_void_t)(g_client + g_ver->f_player))();
    if (p && (rd32(p) != 0 || !readable(p, 0xF0))) return 0;   /* tipo 0 = jogador */
    return p;
}
static int ui_open(int id)
{
    if (!g_ver || !g_ver->ui_vars) return 0;
    return rd32(g_client + g_ver->ui_vars + id * 4) != 0;
}

typedef struct { DWORD unit; int type, cls, mode; DWORD flags; float x, y; } UnitInfo;

static int unit_pos(DWORD u, int type, float *x, float *y)
{
    DWORD path = rd32(u + U_PATH);
    if (!path) return 0;
    if (type == 0 || type == 1 || type == 3) {
        WORD xo = rd16(path), xp = rd16(path + 2), yo = rd16(path + 4), yp = rd16(path + 6);
        if (!xp && !yp) return 0;
        *x = xp + xo / 65536.0f; *y = yp + yo / 65536.0f;
    } else {
        DWORD xp = rd32(path + SP_X), yp = rd32(path + SP_Y);
        if (!xp && !yp) return 0;
        *x = (float)xp; *y = (float)yp;
    }
    return 1;
}
static DWORD unit_room(DWORD u, int type)
{
    DWORD path = rd32(u + U_PATH);
    if (!path) return 0;
    return (type == 0 || type == 1 || type == 3) ? rd32(path + 0x1C) : rd32(path + SP_ROOM);
}
static DWORD room_first(DWORD room) { return rd32(room + (g_ver->room_fmt ? 0x74 : 0x2C)); }
static DWORD room_near(DWORD room, DWORD *count)
{
    *count = rd32(room + (g_ver->room_fmt ? 0x24 : 0x28));
    return rd32(room + (g_ver->room_fmt ? 0x00 : 0x24));
}
static int room_level(DWORD room)
{
    DWORD lvl;
    if (g_ver->room_fmt) {
        DWORD r2 = rd32(room + 0x10);
        lvl = r2 ? rd32(r2 + 0x58) : 0;
        return lvl ? (int)rd32(lvl + 0x1D0) : -1;
    } else {
        DWORD ex = rd32(room + 0x38);
        lvl = ex ? rd32(ex) : 0;
        return lvl ? (int)rd32(lvl + 0x04) : -1;
    }
}
static int is_town(int level) { return level == 1 || level == 40 || level == 75 || level == 103 || level == 109; }

static int detect_next_off(DWORD room)
{
    static const DWORD offs[3] = { 0xE8, 0xE4, 0x10C };
    int best = -1, bestn = 0, i;
    for (i = 0; i < 3; i++) {
        DWORD u = room_first(room);
        int n = 0, bad = 0;
        while (u && n < 200) {
            int t = (int)rd32(u);
            if (t < 0 || t > 5 || unit_room(u, t) != room) {
                if (g_debug > 1) lg("DBG next+%lX: unidade %d %08lX tipo %d sala %08lX (esperada %08lX)", offs[i], n, u, t, t >= 0 && t <= 5 ? unit_room(u, t) : 0, room);
                bad = 1; break;
            }
            n++;
            u = rd32(u + offs[i]);
        }
        if (!bad && n > bestn) { bestn = n; best = i; }
    }
    if (best >= 0 && bestn >= 2) return (int)offs[best];
    return 0;
}

static UnitInfo g_units[512];
static int g_nunits;
static float g_px, g_py;       /* posicao do jogador (subtiles) */
static int g_level = -1;

static void scan_units(void)
{
    DWORD room, nearlist, count, i;
    g_nunits = 0;
    if (!g_player || !unit_pos(g_player, 0, &g_px, &g_py)) return;
    room = unit_room(g_player, 0);
    if (!room) return;
    g_level = room_level(room);
    if (!g_room_next_off) {
        g_room_next_off = (DWORD)detect_next_off(room);
        if (g_room_next_off) lg("Lista de unidades por sala: proximo em +0x%lX", g_room_next_off);
        else return;
    }
    nearlist = room_near(room, &count);
    if (!nearlist || count > 64) { nearlist = 0; count = 0; }
    for (i = 0; i <= count; i++) {
        DWORD r = i < count ? rd32(nearlist + i * 4) : room, u;
        int guard = 0;
        if (!r || (i == count && nearlist && count)) {
            /* a lista de vizinhas ja inclui a propria sala */
            if (i == count) break;
            continue;
        }
        for (u = room_first(r); u && guard < 300 && g_nunits < (int)(sizeof(g_units) / sizeof(g_units[0]));
             u = rd32(u + g_room_next_off), guard++) {
            UnitInfo *ui = &g_units[g_nunits];
            ui->unit = u;
            ui->type = (int)rd32(u);
            if (ui->type < 0 || ui->type > 5) break;
            if (u == g_player || ui->type == 3) continue;
            ui->cls = (int)rd32(u + 4);
            ui->mode = (int)rd32(u + U_MODE);
            ui->flags = rd32(u + U_FLAGS);
            if (!unit_pos(u, ui->type, &ui->x, &ui->y)) continue;
            g_nunits++;
        }
    }
}

/* posicao do jogador na tela (coordenadas do jogo) */
static float g_anchor_x, g_anchor_y;
static void compute_anchor(void)
{
    int left = 0, right = 0;
    float shift = cfg.panel_shift ? (float)cfg.panel_shift : g_resx / 4.0f;
    g_anchor_x = g_resx / 2.0f;
    g_anchor_y = g_resy / 2.0f + cfg.center_dy;
    if (g_ver && g_ver->ui_vars) {
        right = ui_open(UI_INVENTORY) || ui_open(UI_SKILLTREE) || ui_open(UI_NPCSHOP) || ui_open(UI_STASH) ||
                ui_open(UI_CUBE) || ui_open(UI_TRADE) || ui_open(UI_ANVIL);
        left = ui_open(UI_CHARACTER) || ui_open(UI_QUEST) || ui_open(UI_PARTY) || ui_open(UI_MERC) ||
               ui_open(UI_WAYPOINT) || ui_open(UI_NPCSHOP) || ui_open(UI_STASH) || ui_open(UI_CUBE) || ui_open(UI_TRADE);
        if (right && !left) g_anchor_x -= shift;
        else if (left && !right) g_anchor_x += shift;
    }
}
static void world_to_game(float wx, float wy, float *gx, float *gy)
{
    float dx = wx - g_px, dy = wy - g_py;
    *gx = g_anchor_x + (dx - dy) * 16.0f;
    *gy = g_anchor_y + (dx + dy) * 8.0f;
}

/* ======================================================================================
 * alvo (mira assistida)
 * ==================================================================================== */
enum { T_NONE, T_HOSTILE, T_INTERACT };
typedef struct { int kind; DWORD unit; float gx, gy; float dist; } Target;

static float g_face_x = 1, g_face_y = 0.5f;   /* direcao em que o heroi olha (tela) */

static int monster_is_ally(const UnitInfo *u)
{
    static const short allies[] = {
        271, 338, 359, 560, 561,                 /* mercenarios */
        357, 358,                                /* valquiria / shadow */
        363, 364, 365, 366, 289, 290, 291, 292,  /* esqueletos, magos, golens */
        410, 411, 412, 413, 414, 415, 416, 417, 418, 419, 420, 421, 422, 423, 424, /* armadilhas/espiritos/druida */
        426, 427, 428,                           /* corvos, lobos, urso */
    };
    unsigned i;
    if (u->flags & 0x200) return 1;         /* UNITFLAG_ISMERC */
    for (i = 0; i < sizeof(allies) / sizeof(allies[0]); i++) if (u->cls == allies[i]) return 1;
    return 0;
}

/* waypoints, portais e o bau continuam usaveis depois de ativados (modo != 0) */
static int object_always_usable(int cls)
{
    static const int ids[] = { 59, 60, 100, 119, 145, 156, 157, 189, 237, 238, 266, 267, 288, 323, 324, 342, 386,
                               398, 402, 429, 451, 494, 496, 511, 539, 557, 558, 559, 563, 564, 565, 566 };
    unsigned i;
    for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) if (ids[i] == cls) return 1;
    return 0;
}

static Target find_target(int want_hostile, int want_interact)
{
    Target best = { T_NONE, 0, 0, 0, 1e9f };
    float fl = sqrtf(g_face_x * g_face_x + g_face_y * g_face_y), fx, fy;
    float cone = cosf((cfg.aim_cone / 2.0f) * 3.14159265f / 180.0f);
    int town = is_town(g_level), i;
    if (fl < 1e-3f) { g_face_x = 1; g_face_y = 0; fl = 1; }
    if (g_debug > 1) {
        int k = 0;
        for (i = 0; i < g_nunits; i++) if (g_units[i].type == 1) k++;
        lg("DBG find_target hostil %d interagir %d cidade %d face %.2f,%.2f monstros %d", want_hostile, want_interact, town, g_face_x, g_face_y, k);
    }
    fx = g_face_x / fl; fy = g_face_y / fl;
    for (i = 0; i < g_nunits; i++) {
        const UnitInfo *u = &g_units[i];
        float gx, gy, sdx, sdy, sl, dot, d, score;
        int kind = T_NONE;
        if (u->type == 1) {
            if (u->mode == 0 || u->mode == 12 || (u->flags & 0x10000)) continue;
            if (town || monster_is_ally(u)) {
                if (!want_interact || !(u->flags & 0x2)) continue;
                if (!town) continue;                     /* aliados fora da cidade: ignora */
                kind = T_INTERACT;                       /* NPC da cidade */
            } else {
                if (!want_hostile) continue;
                kind = T_HOSTILE;
            }
        } else if (u->type == 4) {                       /* item no chao */
            if (!want_interact || (u->mode != 3 && u->mode != 5)) continue;
            kind = T_INTERACT;
        } else if (u->type == 2) {                       /* objeto (bau, santuario, portal, waypoint) */
            if (!want_interact || !(u->flags & 0x2)) continue;
            if (u->mode != 0 && !object_always_usable(u->cls)) continue;   /* bau ja aberto etc. */
            kind = T_INTERACT;
        } else if (u->type == 0) {
            continue;
        } else continue;
        d = sqrtf((u->x - g_px) * (u->x - g_px) + (u->y - g_py) * (u->y - g_py));
        if (g_debug > 1 && d < 25)
            lg("DBG alvo? tipo %d cls %d modo %d flags %08lX d %.1f kind %d", u->type, u->cls, u->mode, u->flags, d, kind);
        if (kind == T_HOSTILE && d > cfg.aim_range) continue;
        if (kind == T_INTERACT && d > cfg.interact_range) continue;
        world_to_game(u->x, u->y, &gx, &gy);
        sdx = gx - g_anchor_x; sdy = (gy - g_anchor_y) * 2.0f;  /* compensa o isometrico */
        sl = sqrtf(sdx * sdx + sdy * sdy);
        dot = sl > 1 ? (sdx * fx + sdy * fy) / sl : 1;
        if (dot < cone && d > cfg.melee_range && !(kind == T_INTERACT && d < cfg.interact_range * 0.6f)) continue;
        score = d * (2.0f - dot);
        if (kind == T_INTERACT) score *= 1.3f;           /* monstro tem prioridade */
        if (score < best.dist) {
            best.kind = kind; best.unit = u->unit; best.dist = score;
            best.gx = gx; best.gy = gy - (u->type == 1 ? cfg.body_offset : (u->type == 4 ? 0 : cfg.body_offset / 2));
        }
    }
    return best;
}

static int target_still_valid(DWORD unit)
{
    int i;
    for (i = 0; i < g_nunits; i++)
        if (g_units[i].unit == unit) {
            const UnitInfo *u = &g_units[i];
            if (u->type == 1 && (u->mode == 0 || u->mode == 12 || (u->flags & 0x10000))) return 0;
            return 1;
        }
    return 0;
}
static int target_point(DWORD unit, float *gx, float *gy)
{
    int i;
    for (i = 0; i < g_nunits; i++)
        if (g_units[i].unit == unit) {
            world_to_game(g_units[i].x, g_units[i].y, gx, gy);
            *gy -= g_units[i].type == 1 ? cfg.body_offset : 0;
            return 1;
        }
    return 0;
}

/* ======================================================================================
 * logica
 * ==================================================================================== */
enum { M_IDLE, M_MENU, M_PANEL, M_GAME };
static int g_mode = M_IDLE;
static int g_walking;
static int g_running_state;     /* 1 = o jogador quer correr */
static DWORD g_walk_t0, g_run_tap;
static float g_walk_px, g_walk_py; static int g_walk_kicks;

/* vigia: analogico segurado e o heroi parado -> solta modificadores perdidos e repete o clique */
static void walk_watchdog(void)
{
    DWORD now = GetTickCount();
    float d;
    if (!g_player || now - g_walk_t0 < 700 || g_walk_kicks >= 3) return;
    d = fabsf(g_px - g_walk_px) + fabsf(g_py - g_walk_py);
    if (d > 0.3f) { g_walk_kicks = 3; return; }        /* esta andando: nada a fazer */
    if (now - g_walk_t0 < 700u * (g_walk_kicks + 1)) return;
    lg("Heroi parado com o analogico segurado: reenviando clique (%d)", g_walk_kicks + 1);
    if (!g_key_down[VK_SHIFT]) send_key(VK_SHIFT, 0);
    if (!g_key_down[VK_CONTROL]) send_key(VK_CONTROL, 0);
    if (!g_key_down[VK_MENU]) send_key(VK_MENU, 0);
    send_button(0, 0); Sleep(15); send_button(0, 1);
    g_walk_kicks++;
}

/* modos do jogador: 2 andar, 3 correr, 6 andar na cidade. O jogo guarda o estado
 * andar/correr por conta propria; aqui so corrigimos quando ele diverge do desejado. */
static void enforce_run(void)
{
    static DWORD tap_mode, tap_level; static int fails, no_town_run;
    DWORD m, now = GetTickCount();
    int walk_mode;
    if (!g_player) return;
    m = rd32(g_player + U_MODE);
    { static DWORD lm; if (m != lm) { dbg("modo do jogador %lu", m); lm = m; } }
    if (now - g_walk_t0 < 350 || now - g_run_tap < 1200) return;
    /* ate a 1.09 nao se corre na cidade: o modo 6 nao diz nada */
    walk_mode = m == 2 || (m == 6 && U_MODE != 0x0C && !no_town_run);
    if (tap_mode) {                       /* a ultima tecla R mudou alguma coisa? */
        if (m == 6 && tap_mode == 6 && (DWORD)g_level == tap_level) {
            /* 1.10 e anteriores: nao da para correr na cidade */
            if (!no_town_run) lg("Correr na cidade nao suportado nesta versao");
            no_town_run = 1; walk_mode = 0;
        } else if (m == tap_mode && (DWORD)g_level == tap_level && ++fails >= 2) {
            lg("Correr automatico desativado: o jogo nao responde a tecla R neste modo (%lu)", m);
            g_running_state = -1;
        }
        tap_mode = 0;
    }
    if (g_running_state < 0) return;
    if ((g_running_state && walk_mode) || (!g_running_state && m == 3)) {
        dbg("correr: modo %lu, desejado %d -> tecla R", m, g_running_state);
        tap_key('R', 0);
        g_run_tap = now; tap_mode = m; tap_level = (DWORD)g_level;
    }
}

/* acao em andamento de um botao */
typedef struct { int active; Act act; int stage; DWORD t0, last; DWORD unit; int shift; int right; } Hold;
static Hold g_hold[B_COUNT];
static int g_plus_used;         /* algum botao foi usado enquanto + estava segurado */
static int g_layer;

static void release_all(void)
{
    int i;
    if (g_lbtn) send_button(0, 0);
    if (g_rbtn) send_button(1, 0);
    for (i = 0; i < 256; i++) if (g_key_down[i]) send_key(i, 0);
    memset(g_hold, 0, sizeof(g_hold));
    g_walking = 0;
}

static void forward_point(float dist, float *gx, float *gy)
{
    float l = sqrtf(g_face_x * g_face_x + g_face_y * g_face_y);
    float fx = l > 0 ? g_face_x / l : 1, fy = l > 0 ? g_face_y / l : 0;
    *gx = g_anchor_x + fx * dist;
    *gy = g_anchor_y + fy * dist * 0.75f;
}

/* ponto de andar que nao fique em cima de um monstro/NPC (clique nele atacaria/falaria) */
static void walk_point(float dx, float dy, float *gx, float *gy)
{
    static const float scales[] = { 1.0f, 0.75f, 1.3f, 0.55f };
    static const float rots[] = { 0, 0.26f, -0.26f, 0.5f, -0.5f };
    unsigned si, ri;
    for (si = 0; si < 4; si++)
        for (ri = 0; ri < 5; ri++) {
            float c = cosf(rots[ri]), s = sinf(rots[ri]);
            float ux = dx * c - dy * s, uy = dx * s + dy * c;
            float px = g_anchor_x + ux * cfg.walk_radius * scales[si];
            float py = g_anchor_y + uy * cfg.walk_radius * scales[si] * 0.8f;
            int i, blocked = 0;
            for (i = 0; i < g_nunits && !blocked; i++) {
                float ux2, uy2;
                if (g_units[i].type != 1 && g_units[i].type != 4) continue;
                world_to_game(g_units[i].x, g_units[i].y, &ux2, &uy2);
                if (g_units[i].type == 1) uy2 -= cfg.body_offset;
                if (fabsf(ux2 - px) < 26 && fabsf(uy2 - py) < 34) blocked = 1;
            }
            if (!blocked) { *gx = px; *gy = py; return; }
        }
    *gx = g_anchor_x + dx * cfg.walk_radius;
    *gy = g_anchor_y + dy * cfg.walk_radius * 0.8f;
}

static void click(int right, int mods)
{
    int sx = 0, sy = 0, cx = 0, cy = 0;
    POINT p = { 0, 0 };
    /* os menus do D2 so aceitam o clique depois de um WM_MOUSEMOVE sobre o botao */
    if (g_cal_ok) { move_cursor(g_curx, g_cury); Sleep(16); }
    game_to_screen(g_curx, g_cury, &sx, &sy);
    SetCursorPos(sx, sy);
    if (g_hwnd) {
        ClientToScreen(g_hwnd, &p);
        cx = sx - p.x;
        cy = sy - p.y;
    } else {
        cx = sx; cy = sy;
    }
    key_mods(mods, 1);
    send_button(right, 1);
    if (g_hwnd) {
        DWORD md = right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN;
        WPARAM w = right ? MK_RBUTTON : MK_LBUTTON;
        PostMessageA(g_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(cx, cy));
        PostMessageA(g_hwnd, md, w, MAKELPARAM(cx, cy));
    }
    Sleep(80);
    send_button(right, 0);
    if (g_hwnd) {
        DWORD mu = right ? WM_RBUTTONUP : WM_LBUTTONUP;
        PostMessageA(g_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(cx, cy));
        PostMessageA(g_hwnd, mu, 0, MAKELPARAM(cx, cy));
    }
    key_mods(mods, 0);
}

static void do_popup(int right)
{
    /* botoes de habilidade ficam nos cantos inferiores do painel de controle */
    float gx = right ? g_resx / 2.0f + 117 : g_resx / 2.0f - 117;
    float gy = g_resy - 24.0f;
    move_cursor(gx, gy);
    Sleep(30);
    click(0, 0);
}

static Act binding(int ctx, int b)
{
    Act a = g_bind[ctx][b];
    if (ctx == C_LAYER && a.kind == A_NONE) a = g_bind[C_GAME][b];
    return a;
}

/* ataque/habilidade com mira: estagio 0 = escolher alvo e apertar, 1 = segurando */
static void begin_attack(Hold *h, int right)
{
    Target t = cfg.aim ? find_target(1, !right) : (Target){ T_NONE, 0, 0, 0, 0 };
    float gx, gy;
    h->right = right;
    h->shift = 0;
    h->unit = 0;
    if (g_walking) { send_button(0, 0); g_walking = 0; }
    if (t.kind != T_NONE) {
        gx = t.gx; gy = t.gy;
        h->unit = t.unit;
        h->stage = (t.kind == T_INTERACT) ? 2 : 1;
    } else if (is_town(g_level) && !right) {
        /* cidade sem alvo: nao anda (como no D2R); segue procurando enquanto segurar */
        h->stage = 2;
        h->last = GetTickCount();
        return;
    } else {
        forward_point((float)cfg.ranged_distance, &gx, &gy);
        h->stage = 1;
        if (cfg.stand_attack) h->shift = 1;
    }
    move_cursor(gx, gy);
    Sleep(12);
    if (h->stage == 2) {                 /* interagir: um clique, repete enquanto segurar */
        click(0, 0);
        h->last = GetTickCount();
        return;
    }
    if (h->shift) send_key(VK_SHIFT, 1);
    send_button(right, 1);
    h->last = GetTickCount();
}
static void continue_attack(Hold *h)
{
    float gx, gy;
    DWORD now = GetTickCount();
    if (h->stage == 2) {
        if (now - h->last > (DWORD)cfg.repeat_ms) {
            Target t = find_target(1, 1);
            if (t.kind != T_NONE) { move_cursor(t.gx, t.gy); Sleep(10); click(0, 0); }
            h->last = now;
        }
        return;
    }
    if (h->unit) {
        if (target_still_valid(h->unit) && target_point(h->unit, &gx, &gy)) {
            move_cursor(gx, gy);
        } else if (now - h->last > 120) {   /* alvo morreu: procura outro */
            Target t = find_target(1, 0);
            send_button(h->right, 0);
            if (h->shift) { send_key(VK_SHIFT, 0); h->shift = 0; }
            if (t.kind == T_HOSTILE) { h->unit = t.unit; move_cursor(t.gx, t.gy); }
            else { h->unit = 0; forward_point((float)cfg.ranged_distance, &gx, &gy); move_cursor(gx, gy);
                   if (cfg.stand_attack) { h->shift = 1; send_key(VK_SHIFT, 1); } }
            Sleep(10);
            send_button(h->right, 1);
            h->last = now;
        }
    } else if (now - h->last > 200) {        /* sem alvo: procura enquanto segura */
        Target t = find_target(1, 0);
        if (t.kind == T_HOSTILE) {
            send_button(h->right, 0);
            if (h->shift) { send_key(VK_SHIFT, 0); h->shift = 0; }
            h->unit = t.unit;
            move_cursor(t.gx, t.gy);
            Sleep(10);
            send_button(h->right, 1);
        } else {
            forward_point((float)cfg.ranged_distance, &gx, &gy);
            move_cursor(gx, gy);
        }
        h->last = now;
    }
}
static void end_attack(Hold *h)
{
    if (h->stage == 1) send_button(h->right, 0);
    if (h->shift) send_key(VK_SHIFT, 0);
    h->shift = 0;
}

static void start_action(int b, Act a)
{
    Hold *h = &g_hold[b];
    memset(h, 0, sizeof(*h));
    h->active = 1;
    h->act = a;
    h->t0 = GetTickCount();
    switch (a.kind) {
    case A_ATTACK: begin_attack(h, 0); break;
    case A_SKILL:
        if (a.n >= 1 && a.n <= 16) tap_key(VK_F1 + a.n - 1, 0);
        h->stage = 0;                      /* espera o jogo trocar a habilidade */
        break;
    case A_RIGHT: begin_attack(h, 1); break;
    case A_KEY: tap_key(a.vk, a.mods); h->active = 0; break;
    case A_HOLD: key_mods(a.mods, 1); send_key(a.vk, 1); break;
    case A_LCLICK: click(0, 0); h->active = 0; break;
    case A_RCLICK: click(1, 0); h->active = 0; break;
    case A_SHIFTCLICK: click(0, 1); h->active = 0; break;
    case A_CTRLCLICK: click(0, 2); h->active = 0; break;
    case A_SHIFTRCLICK: click(1, 1); h->active = 0; break;
    case A_POPUP_R: do_popup(1); h->active = 0; break;
    case A_POPUP_L: do_popup(0); h->active = 0; break;
    case A_NAV: h->last = GetTickCount(); h->stage = 0; break;
    case A_TYPENAME: type_string("Switch"); h->active = 0; break;
    default: h->active = 0; break;
    }
}
static void nav_step(int b)
{
    float dx = b == B_LEFT ? -1.f : b == B_RIGHT ? 1.f : 0, dy = b == B_UP ? -1.f : b == B_DOWN ? 1.f : 0;
    float x = g_curx + dx * cfg.nav_step, y = g_cury + dy * cfg.nav_step;
    /* painel direito (inventario): encaixa no centro das celulas de 29px, como no D2R.
     * Origem medida no 1.13c: (res/2 + 17, res_y/2 + 14); mods usam a mesma grade. */
    if (x > g_resx / 2 && cfg.nav_step == 29) {
        float ox = g_resx / 2 + 17 + 14.5f, oy = g_resy / 2 + 14 + 14.5f;
        x = ox + floorf((x - ox) / 29.f + 0.5f) * 29.f;
        y = oy + floorf((y - oy) / 29.f + 0.5f) * 29.f;
    } else if (x < g_resx / 2 && cfg.nav_step == 29 && (ui_open(UI_STASH) || ui_open(UI_CUBE) || ui_open(UI_TRADE))) {
        /* painel esquerdo (bau/cubo/troca): grade medida com PlugY 1.13c em (res/2-304, res_y/2-159) */
        float ox = g_resx / 2 - 304 + 14.5f, oy = g_resy / 2 - 159 + 14.5f;
        x = ox + floorf((x - ox) / 29.f + 0.5f) * 29.f;
        y = oy + floorf((y - oy) / 29.f + 0.5f) * 29.f;
    }
    move_cursor(clampf(x, 1, g_resx - 2), clampf(y, 1, g_resy - 2));
}
static void update_action(int b)
{
    Hold *h = &g_hold[b];
    DWORD now = GetTickCount();
    if (!h->active) return;
    switch (h->act.kind) {
    case A_ATTACK: case A_RIGHT: continue_attack(h); break;
    case A_SKILL:
        if (h->stage == 0 && now - h->t0 >= (DWORD)cfg.skill_delay_ms) begin_attack(h, 1);
        else if (h->stage) continue_attack(h);
        break;
    case A_NAV:
        if (h->stage == 0) { nav_step(b); h->stage = 1; h->last = now; }
        else if (now - h->t0 > (DWORD)cfg.nav_delay && now - h->last > (DWORD)cfg.nav_rate) { nav_step(b); h->last = now; }
        break;
    default: break;
    }
}
static void stop_action(int b)
{
    Hold *h = &g_hold[b];
    if (!h->active) return;
    switch (h->act.kind) {
    case A_ATTACK: case A_RIGHT: end_attack(h); break;
    case A_SKILL: if (h->stage) end_attack(h); break;
    case A_HOLD: send_key(h->act.vk, 0); key_mods(h->act.mods, 0); break;
    default: break;
    }
    h->active = 0;
}

static void cursor_from_stick(float x, float y, float dz, float dt)
{
    float m = sqrtf(x * x + y * y), speed;
    if (m < dz) return;
    speed = powf(clampf((m - dz) / (1 - dz), 0, 1), cfg.cursor_curve) * cfg.cursor_speed * g_resx;
    move_cursor(g_curx + x / m * speed * dt, g_cury + y / m * speed * dt);
}

/* Debug=3: compara as secoes graváveis do D2Client a cada 1,5 s e registra DWORDs que
 * alternaram 0<->1. Serve para achar o array de UI em versoes/mods sem offset conhecido. */
static void ui_scan_tick(void)
{
    static BYTE *prev; static DWORD base, size; static DWORD last;
    IMAGE_NT_HEADERS32 *nt; IMAGE_SECTION_HEADER *sec; DWORD i, lo = 0, hi = 0, now = GetTickCount();
    char b[900]; int o = 0, n = 0;
    if (now - last < 1500) return;
    last = now;
    if (!prev) {
        nt = (IMAGE_NT_HEADERS32 *)(g_client + ((IMAGE_DOS_HEADER *)g_client)->e_lfanew);
        sec = IMAGE_FIRST_SECTION(nt);
        for (i = 0; i < nt->FileHeader.NumberOfSections; i++)
            if (sec[i].Characteristics & IMAGE_SCN_MEM_WRITE) {
                if (!lo || sec[i].VirtualAddress < lo) lo = sec[i].VirtualAddress;
                if (sec[i].VirtualAddress + sec[i].Misc.VirtualSize > hi) hi = sec[i].VirtualAddress + sec[i].Misc.VirtualSize;
            }
        if (!lo) return;
        base = lo; size = (hi - lo) & ~3u;
        prev = HeapAlloc(GetProcessHeap(), 0, size);
        memcpy(prev, (BYTE *)g_client + base, size);
        lg("UISCAN secoes graváveis RVA %lX..%lX", base, base + size);
        return;
    }
    for (i = 0; i < size; i += 4) {
        DWORD a = *(DWORD *)(prev + i), c = *(volatile DWORD *)(g_client + base + i);
        if (a != c && ((a == 0 && c == 1) || (a == 1 && c == 0))) {
            if (n < 60) o += snprintf(b + o, sizeof(b) - o, "%lX:%lu ", base + i, c);
            n++;
        }
    }
    memcpy(prev, (BYTE *)g_client + base, size);
    if (n) lg("UISCAN %d: %s", n, b);
    {   /* procura o par de variaveis do mouse (x, y adjacentes) igual ao cursor do Windows */
        static DWORD cand[64]; static int ncand = -1;
        POINT cp; int k, m = 0; o = 0;
        GetCursorPos(&cp); ScreenToClient(g_hwnd, &cp);
        if (ncand < 0) {
            ncand = 0;
            for (i = 4; i + 8 < size && ncand < 64; i += 4) {
                DWORD v = *(volatile DWORD *)(g_client + base + i);
                if (v == (DWORD)cp.x && (*(volatile DWORD *)(g_client + base + i - 4) == (DWORD)cp.y ||
                                         *(volatile DWORD *)(g_client + base + i + 4) == (DWORD)cp.y))
                    cand[ncand++] = base + i;
            }
        } else {
            for (k = 0; k < ncand; k++) {
                DWORD a = cand[k];
                if (rd32(g_client + a) == (DWORD)cp.x &&
                    (rd32(g_client + a - 4) == (DWORD)cp.y || rd32(g_client + a + 4) == (DWORD)cp.y)) cand[m++] = a;
            }
            ncand = m;
        }
        for (k = 0; k < ncand && k < 20; k++) o += snprintf(b + o, sizeof(b) - o, "%lX ", cand[k]);
        lg("MOUSESCAN cursor %ld,%ld candidatos %d: %s", cp.x, cp.y, ncand, b);
        if (!ncand) ncand = -1;
    }
}

static int any_panel_open(void)
{
    static const int ids[] = { UI_INVENTORY, UI_CHARACTER, UI_SKILLTREE, UI_NPCMENU, UI_GAMEMENU, UI_CONFIG,
                               UI_NPCSHOP, UI_ANVIL, UI_QUEST, UI_WAYPOINT, UI_PARTY, UI_TRADE, UI_STASH,
                               UI_CUBE, UI_HELP, UI_MERC, UI_MSGLOG, UI_SCROLL };
    unsigned i;
    if (!g_ver || !g_ver->ui_vars) return 0;
    for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) if (ui_open(ids[i])) return 1;
    return 0;
}

/* ======================================================================================
 * Menu de Gameplay (Switch) - In-Game OSD e Subclass da Janela
 * ==================================================================================== */
#define WM_D2NX_PRINT_MSG (WM_APP + 777)

static WNDPROC g_old_wndproc = NULL;
typedef void (__stdcall *PrintGameString_t)(const wchar_t *msg, int color);
static PrintGameString_t pPrintGameString = NULL;

static LRESULT CALLBACK d2nx_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_D2NX_PRINT_MSG) {
        const wchar_t *wmsg = (const wchar_t *)lp;
        int color = (int)wp;
        if (pPrintGameString && wmsg) {
            pPrintGameString(wmsg, color);
        }
        return 0;
    }
    return CallWindowProcA(g_old_wndproc, hwnd, msg, wp, lp);
}

static void d2_print_string(const wchar_t *wmsg, int color)
{
    if (!pPrintGameString || !wmsg) return;
    if (g_hwnd && IsWindow(g_hwnd) && g_old_wndproc) {
        SendMessageW(g_hwnd, WM_D2NX_PRINT_MSG, (WPARAM)color, (LPARAM)wmsg);
    } else {
        pPrintGameString(wmsg, color);
    }
}

static const float g_xp_opts[] = { 1.0f, 1.5f, 2.0f, 3.0f, 5.0f, 10.0f };
#define XP_COUNT (sizeof(g_xp_opts)/sizeof(g_xp_opts[0]))

static const float g_rune_opts[] = { 1.0f, 1.5f, 2.0f, 3.0f, 5.0f };
#define RUNE_COUNT (sizeof(g_rune_opts)/sizeof(g_rune_opts[0]))

static const float g_unique_opts[] = { 1.0f, 1.25f, 1.5f, 2.0f, 3.0f };
#define UNIQUE_COUNT (sizeof(g_unique_opts)/sizeof(g_unique_opts[0]))

static const float g_rare_opts[] = { 1.0f, 1.25f, 1.5f, 2.0f, 3.0f };
#define RARE_COUNT (sizeof(g_rare_opts)/sizeof(g_rare_opts[0]))

static const int g_gold_opts[] = { 0, 1000, 5000, 10000, 50000, 100000 };
#define GOLD_COUNT (sizeof(g_gold_opts)/sizeof(g_gold_opts[0]))

#define MENU_ITEM_COUNT 6 /* 0: XP, 1: Runas, 2: Unicos, 3: Raros, 4: Ouro, 5: Salvar */

typedef struct {
    int open;
    int cur_item;
    int xp_idx;
    int rune_idx;
    int unique_idx;
    int rare_idx;
    int gold_idx;
    DWORD last_refresh;
} GameplayMenu;

static GameplayMenu g_gp_menu;
static int g_welcome_shown = 0;

static void init_gameplay_menu_indices(void)
{
    unsigned i;
    g_gp_menu.xp_idx = 2; /* 2.0x padrao */
    for (i = 0; i < XP_COUNT; i++) {
        if (fabsf(g_xp_opts[i] - cfg.xp_multiplier) < 0.05f) { g_gp_menu.xp_idx = i; break; }
    }
    g_gp_menu.rune_idx = 2; /* 2.0x padrao */
    for (i = 0; i < RUNE_COUNT; i++) {
        if (fabsf(g_rune_opts[i] - cfg.rune_multiplier) < 0.05f) { g_gp_menu.rune_idx = i; break; }
    }
    g_gp_menu.unique_idx = 2; /* 1.5x padrao */
    for (i = 0; i < UNIQUE_COUNT; i++) {
        if (fabsf(g_unique_opts[i] - cfg.unique_multiplier) < 0.05f) { g_gp_menu.unique_idx = i; break; }
    }
    g_gp_menu.rare_idx = 2; /* 1.5x padrao */
    for (i = 0; i < RARE_COUNT; i++) {
        if (fabsf(g_rare_opts[i] - cfg.rare_multiplier) < 0.05f) { g_gp_menu.rare_idx = i; break; }
    }
    g_gp_menu.gold_idx = 3; /* 10000 padrao */
    for (i = 0; i < GOLD_COUNT; i++) {
        if (g_gold_opts[i] == cfg.start_gold) { g_gp_menu.gold_idx = i; break; }
    }
}

static void patch_experience_file(const char *in_path, const char *out_path, float mult)
{
    FILE *fin = fopen(in_path, "rb");
    if (!fin) {
        fin = fopen(out_path, "rb");
        if (!fin) return;
    }
    fseek(fin, 0, SEEK_END);
    long sz = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    BYTE *buf = (BYTE *)malloc(sz);
    if (!buf) { fclose(fin); return; }
    fread(buf, 1, sz, fin);
    fclose(fin);

    if (sz > 0x20 && mult >= 0.5f) {
        int cnt = (int)(sz - 0x20) / 32;
        int lvl, c;
        for (lvl = 0; lvl < cnt; lvl++) {
            DWORD offset = 0x20 + lvl * 32;
            for (c = 0; c < 7; c++) {
                DWORD pos = offset + 4 + c * 4;
                if (pos + 4 <= (DWORD)sz) {
                    DWORD orig = *(DWORD *)(buf + pos);
                    DWORD nv = orig > 0 ? (DWORD)((float)orig / mult) : 0;
                    if (orig > 0 && nv < 1) nv = 1;
                    *(DWORD *)(buf + pos) = nv;
                }
            }
        }
    }
    FILE *fout = fopen(out_path, "wb");
    if (fout) { fwrite(buf, 1, sz, fout); fclose(fout); }
    free(buf);
}

static void patch_itemratio_file(const char *in_path, const char *out_path, float u_mult, float r_mult)
{
    FILE *fin = fopen(in_path, "rb");
    if (!fin) {
        fin = fopen(out_path, "rb");
        if (!fin) return;
    }
    fseek(fin, 0, SEEK_END);
    long sz = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    BYTE *buf = (BYTE *)malloc(sz);
    if (!buf) { fclose(fin); return; }
    fread(buf, 1, sz, fin);
    fclose(fin);

    if (sz >= 4) {
        DWORD rows = *(DWORD *)buf;
        DWORD r;
        for (r = 0; r < rows; r++) {
            DWORD off = 4 + r * 68;
            if (off + 36 <= (DWORD)sz) {
                DWORD u = *(DWORD *)(buf + off + 8);
                DWORD rv = *(DWORD *)(buf + off + 20);
                DWORD s = *(DWORD *)(buf + off + 32);
                DWORD nu = (DWORD)((float)u / u_mult); if (nu < 5) nu = 5;
                DWORD nr = (DWORD)((float)rv / r_mult); if (nr < 5) nr = 5;
                DWORD ns = (DWORD)((float)s / u_mult); if (ns < 6) ns = 6;
                *(DWORD *)(buf + off + 8) = nu;
                *(DWORD *)(buf + off + 20) = nr;
                *(DWORD *)(buf + off + 32) = ns;
            }
        }
    }
    FILE *fout = fopen(out_path, "wb");
    if (fout) { fwrite(buf, 1, sz, fout); fclose(fout); }
    free(buf);
}

static void patch_treasureclass_file(const char *in_path, const char *out_path, float r_mult)
{
    FILE *fin = fopen(in_path, "rb");
    if (!fin) {
        fin = fopen(out_path, "rb");
        if (!fin) return;
    }
    fseek(fin, 0, SEEK_END);
    long sz = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    BYTE *buf = (BYTE *)malloc(sz);
    if (!buf) { fclose(fin); return; }
    fread(buf, 1, sz, fin);
    fclose(fin);

    if (sz >= 4) {
        DWORD rows = *(DWORD *)buf;
        DWORD row_size = 736;
        DWORD r, idx;
        for (r = 0; r < rows; r++) {
            DWORD off = 4 + r * row_size;
            if (off + row_size > (DWORD)sz) break;
            char tc_name[64];
            memcpy(tc_name, buf + off, 56);
            tc_name[56] = 0;
            if (!strncmp(tc_name, "Runes ", 6)) continue;
            for (idx = 0; idx < 10; idx++) {
                DWORD item_off = off + 0x38 + idx * 64;
                char item_name[64];
                memcpy(item_name, buf + item_off, 64);
                item_name[63] = 0;
                char lower[64];
                int k;
                for (k = 0; item_name[k] && k < 63; k++) lower[k] = (char)tolower((unsigned char)item_name[k]);
                lower[k] = 0;
                if (strstr(lower, "rune")) {
                    DWORD prob_pos = off + 0x2b8 + idx * 4;
                    DWORD prob = *(DWORD *)(buf + prob_pos);
                    DWORD np = (DWORD)((float)prob * r_mult);
                    if (np > 65535) np = 65535;
                    *(DWORD *)(buf + prob_pos) = np;
                }
            }
        }
    }
    FILE *fout = fopen(out_path, "wb");
    if (fout) { fwrite(buf, 1, sz, fout); fclose(fout); }
    free(buf);
}

static void patch_charstats_file(const char *in_path, const char *out_path, int gold)
{
    FILE *fin = fopen(in_path, "rb");
    if (!fin) {
        fin = fopen(out_path, "rb");
        if (!fin) return;
    }
    fseek(fin, 0, SEEK_END);
    long sz = ftell(fin);
    fseek(fin, 0, SEEK_SET);
    BYTE *buf = (BYTE *)malloc(sz);
    if (!buf) { fclose(fin); return; }
    fread(buf, 1, sz, fin);
    fclose(fin);

    if (sz >= 4) {
        DWORD classes = *(DWORD *)buf;
        DWORD row_len = 196;
        DWORD c, pos;
        for (c = 0; c < classes; c++) {
            DWORD off = 4 + c * row_len;
            if (off + row_len > (DWORD)sz) break;
            for (pos = off; pos + 4 <= off + row_len; pos++) {
                if (memcmp(buf + pos, "box ", 4) == 0) {
                    DWORD item_start = pos - 8;
                    DWORD slot9_pos = item_start + 8 * 8;
                    if (slot9_pos + 8 <= (DWORD)sz) {
                        memcpy(buf + slot9_pos, "gld ", 4);
                        WORD gld_val = gold > 10000 ? 10000 : (WORD)gold;
                        *(WORD *)(buf + slot9_pos + 4) = 0;
                        *(WORD *)(buf + slot9_pos + 6) = gld_val;
                    }
                    break;
                }
            }
        }
    }
    FILE *fout = fopen(out_path, "wb");
    if (fout) { fwrite(buf, 1, sz, fout); fclose(fout); }
    free(buf);
}

static void apply_and_save_gameplay_menu(void)
{
    char tmp[64];
    cfg.xp_multiplier = g_xp_opts[g_gp_menu.xp_idx];
    cfg.rune_multiplier = g_rune_opts[g_gp_menu.rune_idx];
    cfg.unique_multiplier = g_unique_opts[g_gp_menu.unique_idx];
    cfg.rare_multiplier = g_rare_opts[g_gp_menu.rare_idx];
    cfg.start_gold = g_gold_opts[g_gp_menu.gold_idx];

    snprintf(tmp, sizeof(tmp), "%.1f", cfg.xp_multiplier);
    WritePrivateProfileStringA("Gameplay", "MultiplicadorXP", tmp, g_ini);
    snprintf(tmp, sizeof(tmp), "%.1f", cfg.rune_multiplier);
    WritePrivateProfileStringA("Gameplay", "MultiplicadorRunas", tmp, g_ini);
    snprintf(tmp, sizeof(tmp), "%.2f", cfg.unique_multiplier);
    WritePrivateProfileStringA("Gameplay", "MultiplicadorUnicos", tmp, g_ini);
    snprintf(tmp, sizeof(tmp), "%.2f", cfg.rare_multiplier);
    WritePrivateProfileStringA("Gameplay", "MultiplicadorRaros", tmp, g_ini);
    snprintf(tmp, sizeof(tmp), "%d", cfg.start_gold);
    WritePrivateProfileStringA("Gameplay", "OuroInicial", tmp, g_ini);

    char d2_root[MAX_PATH];
    lstrcpynA(d2_root, g_dir, sizeof(d2_root));
    char *p = strrchr(d2_root, '\\');
    if (p && p > d2_root) {
        *p = 0;
        p = strrchr(d2_root, '\\');
        if (p) *(p + 1) = 0;
    }

    char excel_dir[MAX_PATH];
    snprintf(excel_dir, sizeof(excel_dir), "%sdata\\global\\excel\\", d2_root);
    CreateDirectoryA(excel_dir, NULL);

    char t_exp[MAX_PATH], o_exp[MAX_PATH];
    snprintf(t_exp, sizeof(t_exp), "%stemplates\\experience_sp.bin", g_dir);
    snprintf(o_exp, sizeof(o_exp), "%sexperience_sp.bin", excel_dir);
    patch_experience_file(t_exp, o_exp, cfg.xp_multiplier);

    char t_ratio[MAX_PATH], o_ratio[MAX_PATH];
    snprintf(t_ratio, sizeof(t_ratio), "%stemplates\\itemratio_sp.bin", g_dir);
    snprintf(o_ratio, sizeof(o_ratio), "%sitemratio_sp.bin", excel_dir);
    patch_itemratio_file(t_ratio, o_ratio, cfg.unique_multiplier, cfg.rare_multiplier);

    char t_tc[MAX_PATH], o_tc[MAX_PATH];
    snprintf(t_tc, sizeof(t_tc), "%stemplates\\treasureclassex.bin", g_dir);
    snprintf(o_tc, sizeof(o_tc), "%streasureclassex.bin", excel_dir);
    patch_treasureclass_file(t_tc, o_tc, cfg.rune_multiplier);

    char t_cs[MAX_PATH], o_cs[MAX_PATH];
    snprintf(t_cs, sizeof(t_cs), "%stemplates\\charstats.bin", g_dir);
    snprintf(o_cs, sizeof(o_cs), "%scharstats.bin", excel_dir);
    patch_charstats_file(t_cs, o_cs, cfg.start_gold);

    lg("[GameplayMenu] Salvo e aplicado com sucesso! XP=%.1fx, Runas=%.1fx, Unicos=%.2fx, Raros=%.2fx, Ouro=%d",
       cfg.xp_multiplier, cfg.rune_multiplier, cfg.unique_multiplier, cfg.rare_multiplier, cfg.start_gold);
}

static void d2_render_gameplay_menu(void)
{
    wchar_t line[160];
    
    d2_print_string(L"\xFFc4=== MENU DE GAMEPLAY (SWITCH) ===", 4);

    _snwprintf(line, sizeof(line)/sizeof(line[0]),
               g_gp_menu.cur_item == 0 ? L"\xFFc2> Multiplicador XP:  [\xFFc8 %.1fx \xFFc2]  \xFFc5(1.0x-10x)"
                                       : L"\xFFc0  Multiplicador XP:  [\xFFc4 %.1fx \xFFc0]",
               g_xp_opts[g_gp_menu.xp_idx]);
    d2_print_string(line, 0);

    _snwprintf(line, sizeof(line)/sizeof(line[0]),
               g_gp_menu.cur_item == 1 ? L"\xFFc2> Queda de Runas:    [\xFFc8 %.1fx \xFFc2]  \xFFc5(1.0x-5.0x)"
                                       : L"\xFFc0  Queda de Runas:    [\xFFc4 %.1fx \xFFc0]",
               g_rune_opts[g_gp_menu.rune_idx]);
    d2_print_string(line, 0);

    _snwprintf(line, sizeof(line)/sizeof(line[0]),
               g_gp_menu.cur_item == 2 ? L"\xFFc2> Itens Unicos:      [\xFFc8 %.2fx \xFFc2]  \xFFc5(1.0x-3.0x)"
                                       : L"\xFFc0  Itens Unicos:      [\xFFc4 %.2fx \xFFc0]",
               g_unique_opts[g_gp_menu.unique_idx]);
    d2_print_string(line, 0);

    _snwprintf(line, sizeof(line)/sizeof(line[0]),
               g_gp_menu.cur_item == 3 ? L"\xFFc2> Itens Raros:       [\xFFc8 %.2fx \xFFc2]  \xFFc5(1.0x-3.0x)"
                                       : L"\xFFc0  Itens Raros:       [\xFFc4 %.2fx \xFFc0]",
               g_rare_opts[g_gp_menu.rare_idx]);
    d2_print_string(line, 0);

    _snwprintf(line, sizeof(line)/sizeof(line[0]),
               g_gp_menu.cur_item == 4 ? L"\xFFc2> Ouro Inicial Lv1:  [\xFFc8 %d \xFFc2]  \xFFc5(0-100k)"
                                       : L"\xFFc0  Ouro Inicial Lv1:  [\xFFc4 %d \xFFc0]",
               g_gold_opts[g_gp_menu.gold_idx]);
    d2_print_string(line, 0);

    if (g_gp_menu.cur_item == 5)
        d2_print_string(L"\xFFc2>>> [ SALVAR E APLICAR ] <<<", 2);
    else
        d2_print_string(L"\xFFc3    [ SALVAR E APLICAR ]", 3);

    d2_print_string(L"\xFFc5(D-Pad: Navega/Altera | A: Salva | B / ZL+Minus: Sair)", 5);

    g_gp_menu.last_refresh = GetTickCount();
}

static BOOL CALLBACK enum_win_proc(HWND w, LPARAM lp)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(w, &pid);
    if (pid == GetCurrentProcessId()) {
        char title[128];
        GetWindowTextA(w, title, sizeof(title));
        if (strstr(title, "Diablo II") || !lstrcmpiA(title, "Diablo II")) {
            *(HWND *)lp = w;
            return FALSE;
        }
    }
    return TRUE;
}

static HWND find_game_window(void)
{
    HWND w = NULL;
    EnumWindows(enum_win_proc, (LPARAM)&w);
    if (!w) {
        DWORD pid;
        while ((w = FindWindowExA(NULL, w, NULL, "Diablo II"))) {
            GetWindowThreadProcessId(w, &pid);
            if (pid == GetCurrentProcessId()) break;
        }
    }
    if (w) lg("Janela do jogo encontrada: hwnd=%p", w);
    return w;
}

static void tick(float dt)
{
    int b, ctx, mode;
    HWND fg;
    if (!g_hwnd || !IsWindow(g_hwnd)) {
        g_hwnd = find_game_window();
        g_cal_ok = 0;
        if (!g_hwnd) return;
        if (!g_old_wndproc) {
            g_old_wndproc = (WNDPROC)(LONG_PTR)SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC, (LONG_PTR)d2nx_wndproc);
            lg("WndProc subclass instalado: g_hwnd=%p, old=%p", g_hwnd, g_old_wndproc);
        }
    }
    poll_pad();
    fg = GetForegroundWindow();
    if (fg && !cfg.autojoin && !cfg.autowalk) {
        DWORD pid = 0;
        GetWindowThreadProcessId(fg, &pid);
        if (pid && pid != GetCurrentProcessId()) {
            HWND cur = g_hwnd;
            int is_ancestor = 0;
            while (cur) {
                if (cur == fg) { is_ancestor = 1; break; }
                cur = GetParent(cur);
            }
            if (!is_ancestor) {
                if (g_mode != M_IDLE) { release_all(); g_mode = M_IDLE; }
                return;
            }
        }
    }
    if (!g_pad.connected && !cfg.autojoin && !cfg.autowalk) { if (g_mode != M_IDLE) { release_all(); g_mode = M_IDLE; } return; }
    if (g_nregions && GetTickCount() - g_regions_tick > 2000) { g_nregions = 0; g_regions_tick = GetTickCount(); }
    g_player = player_unit();
    read_resolution();
    calibrate_tick();
    if (!g_cal_ok) return;

    if (g_debug) {
        static DWORD last;
        if (GetTickCount() - last > 2000) {
            int mx = -1, my = -1;
            last = GetTickCount();
            game_mouse(&mx, &my);
            lg("DBG mouse jogo %d,%d cursor %.0f,%.0f res %dx%d player %08lX pos %.1f,%.1f lvl %d units %d modo %d",
               mx, my, g_curx, g_cury, g_resx, g_resy, g_player, g_px, g_py, g_level, g_nunits, g_mode);
            if (g_debug > 1 && g_ver && g_ver->res_x) {
                int k; char b[400]; int o = 0;
                for (k = -8; k < 8; k++) o += snprintf(b + o, sizeof(b) - o, "%lX ", rd32(g_client + g_ver->res_x + k * 4));
                lg("DBG res area: %s", b);
                for (k = 0; k < g_nunits; k++)
                    if (g_units[k].type == 2 && (g_units[k].cls == 119 || g_units[k].cls == 156 || g_units[k].cls == 157 ||
                        g_units[k].cls == 237 || g_units[k].cls == 238 || g_units[k].cls == 288 || g_units[k].cls == 323 ||
                        g_units[k].cls == 398 || g_units[k].cls == 402 || g_units[k].cls == 429 || g_units[k].cls == 494 ||
                        g_units[k].cls == 496 || g_units[k].cls == 511 || g_units[k].cls == 539))
                        lg("DBG waypoint cls %d em %.0f,%.0f (delta %.0f,%.0f)", g_units[k].cls, g_units[k].x, g_units[k].y, g_units[k].x - g_px, g_units[k].y - g_py);
                {
                    float bd = 1e9f; int bi = -1;
                    for (k = 0; k < g_nunits; k++) if (g_units[k].type == 1 && g_units[k].mode != 0 && g_units[k].mode != 12) {
                        float dd = fabsf(g_units[k].x - g_px) + fabsf(g_units[k].y - g_py);
                        if (dd < bd) { bd = dd; bi = k; }
                    }
                    if (bi >= 0) lg("DBG monstro mais perto cls %d modo %d flags %08lX delta %.0f,%.0f", g_units[bi].cls, g_units[bi].mode,
                                    g_units[bi].flags, g_units[bi].x - g_px, g_units[bi].y - g_py);
                }
                if (g_ver->mouse_x) lg("DBG mouse vars x-4 %ld x %ld x+4 %ld", rd32(g_client + g_ver->mouse_x - 4), rd32(g_client + g_ver->mouse_x), rd32(g_client + g_ver->mouse_x + 4));
            }
        }
    }
    if (g_player) { scan_units(); compute_anchor(); if (g_debug > 2) ui_scan_tick(); }
    if (g_player && g_debug > 2) {   /* despejo da estrutura do jogador para versoes novas */
        static DWORD last;
        if (GetTickCount() - last > 3000) {
            DWORD k, base[2]; char b[1200]; int o, j;
            last = GetTickCount();
            base[0] = g_player; base[1] = rd32(g_player + 0x2C);
            base[1] = rd32(rd32(g_player + U_PATH) + 0x1C);
            for (j = 0; j < 2; j++) {
                o = 0;
                for (k = 0; k < 0x48 && base[j]; k++) o += snprintf(b + o, sizeof(b) - o, "%lX ", rd32(base[j] + k * 4));
                lg("DUMP %s %08lX: %s", j ? "room" : "unit", base[j], b);
            }
            {
                DWORD u = room_first(base[1]), q;
                for (j = 0; j < 4 && u; j++, u = rd32(u + (g_room_next_off ? g_room_next_off : 0x10C))) {
                    q = rd32(u + U_PATH); o = 0;
                    for (k = 0; k < 0x10 && q; k++) o += snprintf(b + o, sizeof(b) - o, "%lX ", rd32(q + k * 4));
                    lg("DUMP unidade %08lX tipo %lu cls %lu path %08lX: %s", u, rd32(u), rd32(u + 4), q, b);
                }
            }
            for (j = 0; j < 0x48; j++) {
                DWORD q = rd32(base[1] + j * 4);
                if (q < 0x10000 || !readable(q, 0x40)) continue;
                o = 0;
                for (k = 0; k < 0x20; k++) o += snprintf(b + o, sizeof(b) - o, "%lX ", rd32(q + k * 4));
                lg("DUMP +%02X -> %08lX: %s", j * 4, q, b);
            }
        }
    }
    mode = !g_player ? M_MENU : any_panel_open() ? M_PANEL : M_GAME;
    if (mode != g_mode) {
        release_all();
        /* cada partida comeca com a preferencia do INI; L3 (tecla R) alterna depois */
        if (mode == M_GAME && g_mode == M_MENU) g_running_state = cfg.always_run;
        /* como no D2R: ao abrir o inventario o cursor vai para a primeira celula */
        if (mode == M_PANEL && g_mode == M_GAME && !ui_open(UI_NPCSHOP) &&
            (ui_open(UI_INVENTORY) || ui_open(UI_STASH) || ui_open(UI_CUBE) || ui_open(UI_TRADE)))
            move_cursor(g_resx / 2 + 17 + 14.5f, g_resy / 2 + 14 + 14.5f);
        dbg("modo %d -> %d", g_mode, mode);
        g_mode = mode;
    }

    if (mode == M_MENU && cfg.autojoin) {
        static DWORD s_menu_t0;
        static int s_stage;
        DWORD now = GetTickCount();
        if (!s_menu_t0) s_menu_t0 = now;
        if (s_stage == 0 && now - s_menu_t0 >= 6000) {
            lg("[AutoTest] 6s no menu principal: clicando em Single Player (400, 320 / 400, 446)...");
            move_cursor(400.0f, 320.0f); Sleep(60); click(0, 0);
            move_cursor(400.0f, 446.0f); Sleep(80); click(0, 0);
            tap_key('S', 0);
            s_stage = 1;
            s_menu_t0 = now;
        } else if (s_stage == 1 && now - s_menu_t0 >= 4000) {
            lg("[AutoTest] 4s na selecao: clicando no personagem (400, 230) e OK (400, 575 / 620, 575)...");
            move_cursor(400.0f, 230.0f); Sleep(80); click(0, 0);
            move_cursor(400.0f, 230.0f); Sleep(80); click(0, 0);
            move_cursor(400.0f, 575.0f); Sleep(80); click(0, 0);
            move_cursor(620.0f, 575.0f); Sleep(80); click(0, 0);
            tap_key(VK_RETURN, 0);
            s_stage = 2;
            s_menu_t0 = now;
        } else if (s_stage == 2 && now - s_menu_t0 >= 2500) {
            lg("[AutoTest] confirmando entrada (Enter / 400, 575 / 620, 575)...");
            move_cursor(400.0f, 575.0f); Sleep(80); click(0, 0);
            move_cursor(620.0f, 575.0f); Sleep(80); click(0, 0);
            tap_key(VK_RETURN, 0);
            s_stage = 3;
            s_menu_t0 = now;
        } else if (s_stage == 3 && now - s_menu_t0 >= 3000) {
            move_cursor(400.0f, 446.0f); click(0, 0);
            move_cursor(400.0f, 230.0f); click(0, 0);
            move_cursor(400.0f, 575.0f); click(0, 0);
            move_cursor(620.0f, 575.0f); click(0, 0);
            tap_key(VK_RETURN, 0);
            s_menu_t0 = now;
        }
    }
    if (mode == M_GAME && cfg.autojoin) {
        lg("[AutoTest] SUCESSO: Personagem entrou no jogo! player %08lX pos %.1f,%.1f lvl %d",
           g_player, g_px, g_py, g_level);
        cfg.autojoin = 0;
    }
    if (mode == M_GAME && cfg.autowalk) {
        static DWORD s_walk_t0;
        DWORD now = GetTickCount();
        if (!s_walk_t0) s_walk_t0 = now;
        if (now - s_walk_t0 > 1500 && now - s_walk_t0 < 10000) {
            g_pad.connected = 1;
            g_pad.lx = 0.85f;
            g_pad.ly = 0.30f;
        }
    }

    if (mode == M_GAME && !g_welcome_shown) {
        static DWORD s_welcome_t0;
        if (!s_welcome_t0) s_welcome_t0 = GetTickCount();
        if (GetTickCount() - s_welcome_t0 > 500) {
            wchar_t wbuf[160];
            d2_print_string(L"\xFFc4[D2NX] Controles Switch ativos! Segure ZL e aperte Minus para abrir o Menu.", 4);
            _snwprintf(wbuf, sizeof(wbuf)/sizeof(wbuf[0]),
                       L"\xFFc2[Gameplay] XP: %.1fx | Runas: %.1fx | Unicos: %.2fx | Raros: %.2fx | Ouro: %d",
                       cfg.xp_multiplier, cfg.rune_multiplier, cfg.unique_multiplier, cfg.rare_multiplier, cfg.start_gold);
            d2_print_string(wbuf, 2);
            g_welcome_shown = 1;
            lg("[D2NX] Mensagem de boas-vindas exibida na tela!");
        }
    }

    /* Atalho do Menu de Gameplay: ZL + Minus, Minus + Plus, ZL + Plus, ou F10 */
    int menu_toggle = 0;
    if ((g_pad.down[B_ZL] && PRESSED(B_MINUS)) ||
        (PRESSED(B_MINUS) && g_pad.down[B_PLUS]) ||
        (PRESSED(B_PLUS) && g_pad.down[B_MINUS]) ||
        (g_pad.down[B_ZL] && PRESSED(B_PLUS))) {
        menu_toggle = 1;
    }
    if (GetAsyncKeyState(VK_F10) & 1) menu_toggle = 1;

    if (menu_toggle) {
        g_gp_menu.open = !g_gp_menu.open;
        if (g_gp_menu.open) {
            lg("[GameplayMenu] Menu aberto via controle!");
            release_all();
            d2_render_gameplay_menu();
        } else {
            lg("[GameplayMenu] Menu fechado.");
            d2_print_string(L"\xFFc5[D2NX] Menu de gameplay fechado.", 5);
        }
    }

    if (g_gp_menu.open) {
        if (PRESSED(B_UP)) {
            g_gp_menu.cur_item = (g_gp_menu.cur_item - 1 + MENU_ITEM_COUNT) % MENU_ITEM_COUNT;
            d2_render_gameplay_menu();
        } else if (PRESSED(B_DOWN)) {
            g_gp_menu.cur_item = (g_gp_menu.cur_item + 1) % MENU_ITEM_COUNT;
            d2_render_gameplay_menu();
        } else if (PRESSED(B_LEFT)) {
            if (g_gp_menu.cur_item == 0 && g_gp_menu.xp_idx > 0) g_gp_menu.xp_idx--;
            else if (g_gp_menu.cur_item == 1 && g_gp_menu.rune_idx > 0) g_gp_menu.rune_idx--;
            else if (g_gp_menu.cur_item == 2 && g_gp_menu.unique_idx > 0) g_gp_menu.unique_idx--;
            else if (g_gp_menu.cur_item == 3 && g_gp_menu.rare_idx > 0) g_gp_menu.rare_idx--;
            else if (g_gp_menu.cur_item == 4 && g_gp_menu.gold_idx > 0) g_gp_menu.gold_idx--;
            d2_render_gameplay_menu();
        } else if (PRESSED(B_RIGHT)) {
            if (g_gp_menu.cur_item == 0 && g_gp_menu.xp_idx < (int)XP_COUNT - 1) g_gp_menu.xp_idx++;
            else if (g_gp_menu.cur_item == 1 && g_gp_menu.rune_idx < (int)RUNE_COUNT - 1) g_gp_menu.rune_idx++;
            else if (g_gp_menu.cur_item == 2 && g_gp_menu.unique_idx < (int)UNIQUE_COUNT - 1) g_gp_menu.unique_idx++;
            else if (g_gp_menu.cur_item == 3 && g_gp_menu.rare_idx < (int)RARE_COUNT - 1) g_gp_menu.rare_idx++;
            else if (g_gp_menu.cur_item == 4 && g_gp_menu.gold_idx < (int)GOLD_COUNT - 1) g_gp_menu.gold_idx++;
            d2_render_gameplay_menu();
        } else if (PRESSED(B_A)) {
            apply_and_save_gameplay_menu();
            d2_print_string(L"\xFFc2[D2NX] CONFIGURACOES SALVAS E APLICADAS!", 2);
            wchar_t wbuf[160];
            _snwprintf(wbuf, sizeof(wbuf)/sizeof(wbuf[0]),
                       L"\xFFc4XP: %.1fx | Runas: %.1fx | Unicos: %.2fx | Raros: %.2fx | Ouro: %d",
                       cfg.xp_multiplier, cfg.rune_multiplier, cfg.unique_multiplier, cfg.rare_multiplier, cfg.start_gold);
            d2_print_string(wbuf, 4);
        } else if (PRESSED(B_B)) {
            g_gp_menu.open = 0;
            d2_print_string(L"\xFFc5[D2NX] Menu de gameplay fechado.", 5);
        } else if (GetTickCount() - g_gp_menu.last_refresh > 4000) {
            d2_render_gameplay_menu();
        }
        return;
    }

    /* direcao em que o heroi olha */
    if (sqrtf(g_pad.rx * g_pad.rx + g_pad.ry * g_pad.ry) > 0.5f) { g_face_x = g_pad.rx; g_face_y = g_pad.ry; }
    else if (sqrtf(g_pad.lx * g_pad.lx + g_pad.ly * g_pad.ly) > cfg.dz_left) { g_face_x = g_pad.lx; g_face_y = g_pad.ly; }

    /* contexto de cada botao */
    g_layer = g_pad.down[B_ZL];
    if (mode == M_GAME) ctx = g_pad.down[B_PLUS] ? C_PLUS : g_layer ? C_LAYER : C_GAME;
    else if (mode == M_PANEL) ctx = C_PANEL;
    else ctx = C_MENU;

    for (b = 0; b < B_COUNT; b++) {
        if (PRESSED(b)) {
            Act a;
            if (mode == M_GAME && b == B_PLUS) { g_plus_used = 0; continue; }
            if (mode == M_GAME && b == B_ZL && binding(C_GAME, B_ZL).kind == A_LAYER) continue;
            a = binding(ctx, b);
            if (ctx == C_PLUS) g_plus_used = 1;
            if (a.kind == A_LAYER || a.kind == A_PLUS) continue;
            if (g_hold[b].active) stop_action(b);
            if (mode == M_GAME && a.kind == A_KEY && a.vk == 'R' && !a.mods) g_running_state = g_running_state <= 0;
            dbg("botao %s ctx %s acao %d", btn_names[b], ctx_names[ctx], a.kind);
            start_action(b, a);
        } else if (RELEASED(b)) {
            if (mode == M_GAME && b == B_PLUS && !g_plus_used) {
                Act a = binding(C_GAME, B_PLUS);
                if (a.kind == A_PLUS) tap_key(VK_ESCAPE, 0); else if (a.kind != A_NONE) start_action(b, a);
                continue;
            }
            stop_action(b);
        } else if (g_pad.down[b]) {
            update_action(b);
        }
    }

    /* movimento */
    if (mode == M_GAME) {
        float m = sqrtf(g_pad.lx * g_pad.lx + g_pad.ly * g_pad.ly);
        int busy = 0;
        for (b = 0; b < B_COUNT; b++)
            if (g_hold[b].active && (g_hold[b].act.kind == A_ATTACK || g_hold[b].act.kind == A_SKILL ||
                                     g_hold[b].act.kind == A_RIGHT)) busy = 1;
        if (m > cfg.dz_left && !busy) {
            float gx, gy;
            walk_point(g_pad.lx / m, g_pad.ly / m, &gx, &gy);
            move_cursor(gx, gy);
            if (!g_walking) {
                Sleep(8); send_button(0, 1); g_walking = 1; g_walk_t0 = GetTickCount();
                g_walk_px = g_px; g_walk_py = g_py; g_walk_kicks = 0;
            } else { enforce_run(); walk_watchdog(); }
        } else if (g_walking) {
            send_button(0, 0);
            g_walking = 0;
        }
        /* analogico direito: cursor livre (mirar manualmente / clicar no chao) */
        if (!g_walking && !busy) cursor_from_stick(g_pad.rx, g_pad.ry, cfg.dz_right, dt);
    } else {
        sync_cursor();
        cursor_from_stick(g_pad.lx, g_pad.ly, cfg.dz_left, dt);
        cursor_from_stick(g_pad.rx, g_pad.ry, cfg.dz_right, dt);
    }
}

/* ======================================================================================
 * inicializacao
 * ==================================================================================== */
static DWORD WINAPI worker(void *arg)
{
    LARGE_INTEGER f, t0, t1;
    unsigned n;
    (void)arg;
    for (n = 0; n < 6000 && !(g_client = (DWORD)GetModuleHandleA("D2Client.dll")); n++) Sleep(100);
    if (!g_client) { lg("D2Client.dll nao carregou; controle inativo"); return 0; }
    g_ver = d2ver_detect((HMODULE)g_client);
    if (!g_ver && g_ver_override.name) {
        lg("Usando offsets do INI (%s)", g_ver_override.name);
        g_ver = &g_ver_override;
    }
    if (g_ver) {
        static D2Ver merged;
        merged = *g_ver;
        if (g_ver_override.res_x) merged.res_x = g_ver_override.res_x;
        if (g_ver_override.res_y) merged.res_y = g_ver_override.res_y;
        if (g_ver_override.mouse_x) merged.mouse_x = g_ver_override.mouse_x;
        if (g_ver_override.mouse_y) merged.mouse_y = g_ver_override.mouse_y;
        if (g_ver_override.player) merged.player = g_ver_override.player;
        if (g_ver_override.ui_vars) merged.ui_vars = g_ver_override.ui_vars;
        if (g_ver_override.print_msg) merged.print_msg = g_ver_override.print_msg;
        g_ver = &merged;
        lg("Diablo II %s detectado (D2Client %08lX)", g_ver->name, g_client);
    } else {
        IMAGE_NT_HEADERS32 *nt = (IMAGE_NT_HEADERS32 *)(g_client + ((IMAGE_DOS_HEADER *)g_client)->e_lfanew);
        lg("D2Client desconhecido (timestamp %08lX, tamanho %lX): modo cursor", nt->FileHeader.TimeDateStamp,
           nt->OptionalHeader.SizeOfImage);
    }
    if (g_ver && g_ver->print_msg) {
        pPrintGameString = (PrintGameString_t)(g_client + g_ver->print_msg);
        lg("PrintGameString resolvido em %p (RVA %08lX)", pPrintGameString, g_ver->print_msg);
    } else if (g_client) {
        pPrintGameString = (PrintGameString_t)(g_client + 0x7D850);
        lg("PrintGameString fallback para 0x7D850: %p", pPrintGameString);
    }
    unit_layout_init();
    load_xinput();
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);
    while (g_running) {
        float dt;
        Sleep(8);
        QueryPerformanceCounter(&t1);
        dt = (float)(t1.QuadPart - t0.QuadPart) / f.QuadPart;
        t0 = t1;
        if (dt > 0.1f) dt = 0.1f;
        tick(dt);
    }
    release_all();
    return 0;
}

/* Copiado como D2Pad.dll na pasta de um mod (troca de arquivo feita pelo D2Autorun):
 * carrega o ChainLoad do D2Pad.ini da mesma pasta, como a D2Pad original fazia. */
static void chain_load_self(void)
{
    char dir[MAX_PATH], ini[MAX_PATH], list[512], *tok, *ctx = NULL, *s;
    GetModuleFileNameA(g_self, dir, sizeof(dir));
    if (!(s = strrchr(dir, '\\'))) return;
    if (lstrcmpiA(s + 1, "D2Pad.dll")) return;
    s[1] = 0;
    snprintf(ini, sizeof(ini), "%sD2Pad.ini", dir);
    GetPrivateProfileStringA("General", "ChainLoad", "", list, sizeof(list), ini);
    for (tok = strtok_r(list, ",|;", &ctx); tok; tok = strtok_r(NULL, ",|;", &ctx)) {
        char p[MAX_PATH];
        while (*tok == ' ') tok++;
        if (!*tok) continue;
        if (strchr(tok, ':') || tok[0] == '\\') lstrcpynA(p, tok, sizeof(p));
        else snprintf(p, sizeof(p), "%s%s", dir, tok);
        LoadLibraryA(p);
    }
}

static void start(void)
{
    char *s;
    HANDLE th;
    HMODULE pin;
    char name[64], d2nx[MAX_PATH];
    if (InterlockedExchange(&g_running, 1)) return;
    /* fixa o DLL: carregadores podem descarregar a ModDll depois da inicializacao */
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCSTR)start, &pin);
    /* configuracao e log sempre em <raiz>\d2nx\ (mesmo quando copiado como D2Pad.dll) */
    GetModuleFileNameA(NULL, d2nx, sizeof(d2nx));
    if ((s = strrchr(d2nx, '\\'))) s[1] = 0;
    lstrcatA(d2nx, "d2nx\\");
    if (GetFileAttributesA(d2nx) != INVALID_FILE_ATTRIBUTES) lstrcpynA(g_dir, d2nx, sizeof(g_dir));
    else {
        GetModuleFileNameA(g_self, g_dir, sizeof(g_dir));
        if ((s = strrchr(g_dir, '\\'))) s[1] = 0;
    }
    snprintf(g_ini, sizeof(g_ini), "%sd2nxpad.ini", g_dir);
    /* uma instancia por processo (d2nx\D2NXPad.dll e a copia D2Pad.dll do mod) */
    snprintf(name, sizeof(name), "D2NXPad-%lu", GetCurrentProcessId());
    CreateMutexA(NULL, FALSE, name);
    if (GetLastError() == ERROR_ALREADY_EXISTS) return;
    {
        char logp[MAX_PATH];
        snprintf(logp, sizeof(logp), "%sd2nxpad.log", g_dir);
        g_log = fopen(logp, "w");
    }
    load_config();
    lg("==== D2NXPad %s (pid %lu) ====", PAD_VERSION, GetCurrentProcessId());
    if (!cfg.enabled) { lg("Desativado no INI"); return; }
    th = CreateThread(NULL, 0, worker, NULL, 0, NULL);
    if (th) CloseHandle(th);
}

/* exportacoes compativeis com a D2Pad antiga e com carregadores (PlugY/D2SE) */
__declspec(dllexport) void __stdcall D2Pad_Init(void) { start(); }
__declspec(dllexport) void __stdcall Init(void) { start(); }

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID r)
{
    (void)r;
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        chain_load_self();
        start();
    } else if (reason == DLL_PROCESS_DETACH) {
        g_running = 0;
    }
    return TRUE;
}
