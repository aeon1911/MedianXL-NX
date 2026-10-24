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
static BYTE rd8(DWORD addr) { return readable(addr, 1) ? *(volatile BYTE *)addr : 0; }

/* ======================================================================================
 * configuracao
 * ==================================================================================== */
enum Btn { B_A, B_B, B_X, B_Y, B_L, B_R, B_ZL, B_ZR, B_PLUS, B_MINUS, B_L3, B_R3,
           B_UP, B_DOWN, B_LEFT, B_RIGHT, B_COUNT };
static const char *btn_names[B_COUNT] = { "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "Plus", "Minus",
                                          "L3", "R3", "Up", "Down", "Left", "Right" };
enum ActKind { A_NONE, A_ATTACK, A_SKILL, A_RIGHT, A_KEY, A_HOLD, A_LCLICK, A_RCLICK, A_SHIFTCLICK,
               A_CTRLCLICK, A_SHIFTRCLICK, A_LAYER, A_PLUS, A_NAV, A_POPUP_R, A_POPUP_L, A_TYPENAME, A_KEYBOARD, A_OSK, A_INTERACT };
typedef struct { BYTE kind; BYTE vk; BYTE mods; BYTE n; } Act;  /* mods: 1 shift 2 ctrl 4 alt */
enum Ctx { C_GAME, C_LAYER, C_PLUS, C_PANEL, C_MENU, C_COUNT };
static const char *ctx_names[C_COUNT] = { "Jogo", "Jogo.ZL", "Jogo.Mais", "Painel", "Menu" };
static Act g_bind[C_COUNT][B_COUNT];

static struct {
    int enabled, pad_index;
    float dz_left, dz_right, trigger;
    int walk_radius, center_dy, always_run;
    float run_at;
    int aim, aim_range, aim_cone, melee_range, interact_range, body_offset, ranged_distance;
    int stand_attack, repeat_ms;
    float cursor_speed, cursor_curve;
    int nav_step, nav_delay, nav_rate;
    int panel_shift;
    int skill_delay_ms;
    int autojoin, autowalk, autocreate, cursortest, menutest, itemtest, item_x, item_y, readtest, item_delay, walktest;
    int move_mode;
    float walk_noise;
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
"; analogico esquerdo = movimento nativo (sem mouse); A/X/B usam o alvo detectado na tela\n"
"A=ATTACK\n"
"X=RIGHT\n"
"B=INTERACT\n"
"Y=KEY:I\n"
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
"; - nao manda ESC: o ESC saia da tela de nome/fechava o jogo\n"
"Minus=KEYBOARD\n"
"L=KEY:SHIFT\n"
"R=KEY:TAB\n"
"; KEYBOARD = editor de nome: escreve um nome na caixa do jogo e o direcional edita\n"
"; (cima/baixo troca a letra, direita poe letra, esquerda apaga, Y sorteia, + ou X cria, B ou R3 sai).\n"
"; OSK = teclado do Autorun (com o D2GL ele abre mas nao aparece na tela)\n"
"L3=KEYBOARD\n"
"R3=KEYBOARD\n"
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
    else if (!lstrcmpiA(buf, "KEYBOARD")) a.kind = A_KEYBOARD;
    else if (!lstrcmpiA(buf, "OSK")) a.kind = A_OSK;
    else if (!lstrcmpiA(buf, "INTERACT")) a.kind = A_INTERACT;
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

static void script_load_ini(void);
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
    {   /* inclinacao do analogico (0-1, depois da zona morta) a partir da qual o heroi corre */
        char v[32];
        GetPrivateProfileStringA("Movimento", "IntensidadeCorrer", "0.65", v, sizeof(v), g_ini);
        cfg.run_at = (float)atof(v);
        if (cfg.run_at < 0.0f || cfg.run_at > 1.0f) cfg.run_at = 0.65f;
        if (!cfg.always_run) cfg.run_at = 2.0f;   /* SempreCorrer=0: so anda */
    }
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
    cfg.autocreate = ini_int("AutoTest", "AutoCreate", 0);
    cfg.cursortest = ini_int("AutoTest", "CursorTest", 0);
    cfg.menutest = ini_int("AutoTest", "MenuTest", 0);
    cfg.itemtest = ini_int("AutoTest", "ItemTest", 0);
    cfg.readtest = ini_int("AutoTest", "ReadTest", 0);
    cfg.item_x = ini_int("AutoTest", "ItemX", 0);
    cfg.item_y = ini_int("AutoTest", "ItemY", 0);
    cfg.item_delay = ini_int("AutoTest", "ItemDelay", 10000);
    cfg.walktest = ini_int("AutoTest", "WalkTest", 0);
    script_load_ini();
    cfg.walk_noise = ini_float("AutoTest", "RuidoAnalogico", 0.0f);
    cfg.move_mode = ini_int("Movimento", "ModoOrdem", 1);
    if (cfg.autocreate) cfg.autojoin = 0;   /* AutoCreate faz o proprio caminho pelos menus */
    if (cfg.autojoin || cfg.autowalk || cfg.autocreate)
        lg("AutoTest ativo: autojoin=%d autowalk=%d autocreate=%d", cfg.autojoin, cfg.autowalk, cfg.autocreate);
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

/* Teclado flutuante do Autorun: enquanto aberto, o XInput chega zerado; quando fecha (o - fecha),
 * os botoes ainda apertados aparecem como apertos novos. Esses ficam travados ate o jogador
 * solta-los, para o - que fechou o teclado nao virar acao no jogo. O mesmo vale para - + R3,
 * o atalho do proprio Autorun que abre o teclado. */
static int g_osk_wait;
static int g_latched[B_COUNT];

static void latch_all_down(void)
{
    int i;
    for (i = 0; i < B_COUNT; i++) if (g_pad.down[i]) g_latched[i] = 1;
}

static void pad_latch(void)
{
    int i, any = 0;
    for (i = 0; i < B_COUNT; i++) any |= g_pad.down[i];
    if (g_osk_wait && any) { latch_all_down(); g_osk_wait = 0; }
    if (g_pad.down[B_MINUS] && g_pad.down[B_R3]) latch_all_down();
    for (i = 0; i < B_COUNT; i++) {
        if (!g_latched[i]) continue;
        if (g_pad.down[i]) g_pad.down[i] = 0; else g_latched[i] = 0;
    }
}

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
    pad_latch();
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
static int g_cal_ok;
static int g_lbtn, g_rbtn;       /* botoes do mouse que NOS estamos segurando */
static int g_key_down[256];
static float g_curx = 400, g_cury = 300;   /* cursor que controlamos, em coordenadas do jogo */
static int g_cursor_parked = 0;

/* Teclas vao por um caminho so: SendInput. No Ryujinx, SendInput, keybd_event e PostMessage(WM_KEYDOWN)
 * abrem/fecham sozinhos o menu do ESC dentro do jogo; mandar pelos tres fazia cada toque alternar o
 * painel duas ou tres vezes. */
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
    g_key_down[vk & 0xff] = down;
}
static void key_mods(int mods, int down)
{
    if (mods & 1) send_key(VK_SHIFT, down);
    if (mods & 2) send_key(VK_CONTROL, down);
    if (mods & 4) send_key(VK_MENU, down);
}
/* Texto: WM_CHAR postado com contagem de repeticao 1 (lParam 1). No Wine-NX a tecla do SendInput nao
 * vira caractere na caixa de nome e o WM_CHAR com contagem 0 era ignorado; com 1 o texto entra. */
/* lParam de tecla como o Windows gera: contagem de repeticao 1 e scan code */
static LPARAM key_lparam(int vk, int up)
{
    DWORD lp = 1 | ((MapVirtualKeyA(vk, 0) & 0xFF) << 16);
    return (LPARAM)(up ? (lp | 0xC0000000UL) : lp);
}
static void post_char(int c)
{
    if (g_hwnd) PostMessageA(g_hwnd, WM_CHAR, (WPARAM)(unsigned char)c, 1);
}
/* Apagar na caixa de nome: WM_KEYDOWN VK_BACK (o WM_CHAR 8 e ignorado). Postado, e nao por SendInput,
 * para ficar na ordem das letras: o Windows entrega mensagens postadas antes da entrada do teclado. */
static void post_back(void)
{
    if (!g_hwnd) return;
    PostMessageA(g_hwnd, WM_KEYDOWN, VK_BACK, key_lparam(VK_BACK, 0));
    PostMessageA(g_hwnd, WM_KEYUP, VK_BACK, key_lparam(VK_BACK, 1));
}
static DWORD g_enter_t;          /* ultimo Enter pedido (para saber se o chat foi aberto de proposito) */
static void tap_key(int vk, int mods)
{
    if (vk == VK_RETURN) g_enter_t = GetTickCount();
    key_mods(mods, 1);
    send_key(vk, 1);
    if (vk >= 'A' && vk <= 'Z') post_char((mods & 1) ? vk : (vk + 32));
    else if (vk >= '0' && vk <= '9') post_char(vk);
    Sleep(35);
    send_key(vk, 0);
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

static void game_to_client(float gx, float gy, int *cx, int *cy)
{
    gx = clampf(gx, 0, (float)g_resx - 1);
    gy = clampf(gy, 0, (float)g_resy - 1);
    *cx = (int)lroundf(g_ox + gx * g_sx);
    *cy = (int)lroundf(g_oy + gy * g_sy);
}

static void send_button(int right, int down)
{
    if (right) g_rbtn = down; else g_lbtn = down;
    if (g_hwnd) {
        int cx, cy;
        game_to_client(g_curx, g_cury, &cx, &cy);
        DWORD msg = right ? (down ? WM_RBUTTONDOWN : WM_RBUTTONUP)
                          : (down ? WM_LBUTTONDOWN : WM_LBUTTONUP);
        WPARAM w = 0;
        if (down) w |= right ? MK_RBUTTON : MK_LBUTTON;
        if (g_key_down[VK_SHIFT]) w |= MK_SHIFT;
        if (g_key_down[VK_CONTROL]) w |= MK_CONTROL;
        PostMessageA(g_hwnd, WM_MOUSEMOVE, w, MAKELPARAM(cx, cy));
        PostMessageA(g_hwnd, msg, w, MAKELPARAM(cx, cy));
    }
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
    if (!g_player) { g_resx = 800; g_resy = 600; return; }   /* menus do LoD sao sempre 800x600 */
    if (g_ver && g_ver->res_x && g_ver->res_y) {
        int x = (int)rd32(g_client + g_ver->res_x), y = (int)rd32(g_client + g_ver->res_y);
        if (x >= 640 && x <= 4096 && y >= 480 && y <= 4096) { g_resx = x; g_resy = y; }
    }
}

/* mapeamento: coordenadas do jogo para coordenadas de cliente da janela */
static void fallback_mapping(void)
{
    RECT rc;
    float w, h;
    if (!g_hwnd) return;
    GetClientRect(g_hwnd, &rc);
    w = (float)(rc.right - rc.left); h = (float)(rc.bottom - rc.top);
    if (w < 16 || h < 16) { w = (float)g_resx; h = (float)g_resy; }
    if (w >= 1200 && h >= 700 && fabsf((w / h) - (16.0f / 9.0f)) < 0.2f) {
        /* 16:9 widescreen no Switch (1280x720) com D2GL: preenche 100% da tela */
        g_sx = w / (float)g_resx;
        g_sy = h / (float)g_resy;
        g_ox = 0.0f;
        g_oy = 0.0f;
    } else {
        float s = w / g_resx < h / g_resy ? w / g_resx : h / g_resy;
        g_sx = g_sy = s;
        g_ox = (w - g_resx * s) / 2.0f;
        g_oy = (h - g_resy * s) / 2.0f;
    }
}

enum { M_IDLE, M_MENU, M_PANEL, M_GAME };
static int g_mode = M_IDLE;

static void sync_cursor(void)
{
    /* Desativado no Switch para evitar que GetCursorPos do Wine-NX cause salto de cursor */
}

static void move_cursor(float gx, float gy)
{
    if (!g_hwnd) return;
    int cx, cy;
    g_cursor_parked = 0;
    g_curx = clampf(gx, 0, (float)g_resx - 1);
    g_cury = clampf(gy, 0, (float)g_resy - 1);
    game_to_client(g_curx, g_cury, &cx, &cy);

    WPARAM w = 0;
    if (g_lbtn) w |= MK_LBUTTON;
    if (g_rbtn) w |= MK_RBUTTON;
    if (g_key_down[VK_SHIFT]) w |= MK_SHIFT;
    if (g_key_down[VK_CONTROL]) w |= MK_CONTROL;

    PostMessageA(g_hwnd, WM_MOUSEMOVE, w, MAKELPARAM(cx, cy));
}

static void park_cursor(void)
{
    if (!g_hwnd || g_cursor_parked) return;
    int park_x = 2000;
    int park_y = 2000;
    PostMessageA(g_hwnd, WM_MOUSEMOVE, 0, MAKELPARAM(park_x, park_y));
    g_cursor_parked = 1;
}

/* Cursor oculto na partida. O D2Client 1.13c desenha o cursor em duas rotinas sem argumentos, ambas pela
 * posicao do mouse do jogo (+0x11B828/+0x11B824) e pelo D2Gfx: +0x16870 (a principal: imagem pelo tipo de
 * cursor em +0x11B838[tipo], quadro animado) e +0x15FC0 (imagem fixa de +0x11B850). As duas rodam a cada
 * quadro na thread do jogo. A +0x16870 vira um salto para frame_hook (camada nativa + menu de cheats), que
 * so chama o desenho original quando ha painel/menu aberto; a +0x15FC0 vira ret nesse mesmo estado. */
static BYTE *g_cursor_fn2, g_cursor_orig2;
static BYTE *g_cursor_tramp;
static volatile LONG g_cursor_hide_now = 0;
static int g_cursor_hidden = -1;
static void __cdecl frame_hook(void);
static void draw_init(void);
static DWORD g_cmd_fn;                 /* D2Client+0x2A7C0, conferido em native_init */
static void native_init(void)
{
    BYTE *a, *b, *c, *t;
    DWORD old;
    if (!g_ver || strcmp(g_ver->name, "1.13c") || !g_ver->mouse_x || !g_ver->mouse_y) {
        lg("camada nativa: so existe para o D2Client 1.13c original");
        return;
    }
    a = (BYTE *)(g_client + 0x16870);   /* mov eax,[mouse_x] */
    b = (BYTE *)(g_client + 0x15FC0);   /* sub esp,48h / mov edx,[mouse_y] */
    c = (BYTE *)(g_client + 0x2A7C0);   /* sub esp,20h / push esi / mov esi,eax / call */
    if (!(a[0] == 0xA1 && *(DWORD *)(a + 1) == g_client + g_ver->mouse_x &&
          b[0] == 0x83 && b[1] == 0xEC && b[2] == 0x48 && b[3] == 0x8B && b[4] == 0x15 &&
          *(DWORD *)(b + 5) == g_client + g_ver->mouse_y &&
          c[0] == 0x83 && c[1] == 0xEC && c[2] == 0x20 && c[3] == 0x56 && c[4] == 0x8B && c[5] == 0xF0 && c[6] == 0xE8)) {
        lg("camada nativa: bytes do D2Client nao conferem (%02X %02X %02X), nada alterado", a[0], b[0], c[0]);
        return;
    }
    t = VirtualAlloc(NULL, 16, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!t) return;
    memcpy(t, a, 5);                                      /* instrucao original */
    t[5] = 0xE9; *(DWORD *)(t + 6) = (DWORD)((a + 5) - (t + 10));   /* volta para +5 */
    VirtualProtect(t, 16, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), t, 16);
    g_cursor_tramp = t;
    g_cursor_fn2 = b; g_cursor_orig2 = b[0];
    g_cmd_fn = (DWORD)c;
    if (!VirtualProtect(a, 5, PAGE_EXECUTE_READWRITE, &old)) return;
    a[0] = 0xE9; *(DWORD *)(a + 1) = (DWORD)((BYTE *)frame_hook - (a + 5));
    VirtualProtect(a, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), a, 5);
    lg("camada nativa: gancho por quadro em %p, comando do jogador em %p, cursor oculto sem painel", a, c);
}
static void set_cursor_hidden(int hide)
{
    DWORD old;
    if (!g_cursor_tramp || hide == g_cursor_hidden) return;
    InterlockedExchange(&g_cursor_hide_now, hide);
    if (VirtualProtect(g_cursor_fn2, 1, PAGE_EXECUTE_READWRITE, &old)) {
        g_cursor_fn2[0] = hide ? 0xC3 : g_cursor_orig2;
        VirtualProtect(g_cursor_fn2, 1, old, &old);
        FlushInstructionCache(GetCurrentProcess(), g_cursor_fn2, 1);
    }
    dbg("cursor %s", hide ? "oculto" : "visivel");
    g_cursor_hidden = hide;
}

/* ======================================================================================
 * camada nativa (RULES.md: NATIVE ANALOG MOVEMENT)
 * Movimento e acoes do heroi passam pela rotina de comando do jogador do D2Client 1.13c (+0x2A7C0), a
 * mesma para onde um clique termina: eax = unidade do jogador, ebx/edi = destino (x,y do mundo) ou alvo
 * (tipo,id), pilha = comando e 0. Ela faz a acao localmente (caminho, colisao, animacao: +0x2A750) e manda
 * o comando ao servidor (+0x29FA0 -> +0x143E0). Sem cursor, mouse ou clique. Comandos: 1 andar ate (x,y),
 * 3 correr ate (x,y), 4 correr ate a unidade, 5/0x0C habilidade esq./dir. em (x,y), 6/0x0D na unidade,
 * 0x13 interagir. So pode ser chamada na thread do jogo: o D2NXPad so registra a intencao e o gancho por
 * quadro (rotina de desenho do cursor, +0x16870) executa.
 * ==================================================================================== */
enum { NC_WALK = 1, NC_RUN_TO_UNIT = 4, NC_RUN = 3, NC_LEFT_AT = 5, NC_LEFT_UNIT = 6, NC_RIGHT_AT = 0x0C,
       NC_RIGHT_UNIT = 0x0D, NC_INTERACT = 0x13 };
static int do_command(DWORD unit, DWORD a, DWORD b, DWORD cmd)
{
    DWORD fn = g_cmd_fn, u = unit;
    if (!fn || !unit) return 0;
    __asm__ __volatile__ (
        "pushl $0\n\t"
        "pushl %%ecx\n\t"
        "call *%%edx\n\t"
        : "+a"(u), "+b"(a), "+D"(b), "+c"(cmd), "+d"(fn)
        :
        : "esi", "memory", "cc");
    return (int)u;
}

/* fila de comandos avulsos (D2NXPad -> thread do jogo) */
typedef struct { int cmd; DWORD a, b; } NatCmd;
#define NATQ 32
static NatCmd g_natq[NATQ];
static volatile LONG g_natq_head, g_natq_tail;
static void native_queue(int cmd, DWORD a, DWORD b)
{
    LONG h = g_natq_head;
    if (h - g_natq_tail >= NATQ) return;
    g_natq[h % NATQ].cmd = cmd; g_natq[h % NATQ].a = a; g_natq[h % NATQ].b = b;
    InterlockedExchange(&g_natq_head, h + 1);
}

/* intencao de movimento: direcao no mundo (vetor unitario), andar/correr; 0 = parado */
static volatile LONG g_mv_active, g_mv_run;
static volatile float g_mv_wx, g_mv_wy;
static volatile LONG g_mv_busy;        /* um ataque esta segurado: o movimento espera */
static volatile LONG g_frames;         /* quadros vistos pelo gancho (diagnostico) */

static void calibrate_tick(void)
{
    fallback_mapping();
    g_cal_ok = 1;
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

static float g_face_x = 0.0f, g_face_y = 1.0f;   /* direcao em que o heroi olha (tela) - padrao baixo, nunca direita */
static int g_face_valid = 0;

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
    if (fl < 1e-3f) { g_face_x = 0; g_face_y = 1; fl = 1; }
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
/* ======================================================================================
 * logica
 * ==================================================================================== */
static int g_running_state;     /* 1 = correr (a tecla R do jogo nao e mais usada pelo D2NXPad) */

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
    InterlockedExchange(&g_mv_active, 0);
    InterlockedExchange(&g_mv_busy, 0);
    if (g_mode == M_GAME) park_cursor();
}

static void click(int right, int mods)
{
    if (!g_hwnd) return;
    int cx, cy;
    game_to_client(g_curx, g_cury, &cx, &cy);
    DWORD md = right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN;
    DWORD mu = right ? WM_RBUTTONUP : WM_LBUTTONUP;
    WPARAM w = right ? MK_RBUTTON : MK_LBUTTON;
    if (mods & 1) w |= MK_SHIFT;
    if (mods & 2) w |= MK_CONTROL;

    key_mods(mods, 1);
    PostMessageA(g_hwnd, WM_MOUSEMOVE, w, MAKELPARAM(cx, cy));
    PostMessageA(g_hwnd, md, w, MAKELPARAM(cx, cy));
    Sleep(35);
    PostMessageA(g_hwnd, mu, 0, MAKELPARAM(cx, cy));
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

/* ======================================================================================
 * acoes nativas (sem mouse): alvo por deteccao na tela, comando direto na unidade
 * ==================================================================================== */
static DWORD unit_id(DWORD u) { return rd32(u + 0x0C); }
/* modos do jogador em que ele pode receber outro comando (parado, andando, correndo, levando golpe) */
static int player_ready(void)
{
    DWORD m = g_player ? rd32(g_player + U_MODE) : 0;
    return m == 1 || m == 2 || m == 3 || m == 4 || m == 5 || m == 6;
}
/* direcao em que o heroi olha, no mundo (para atacar/lancar sem alvo) */
static void face_world(float *wx, float *wy)
{
    float sx = g_face_x, sy = g_face_y, x, y, l;
    if (!g_face_valid || (fabsf(sx) < 1e-3f && fabsf(sy) < 1e-3f)) { sx = 0; sy = 1; }
    x = sx / 16.0f + sy / 8.0f; y = sy / 8.0f - sx / 16.0f;
    l = sqrtf(x * x + y * y);
    *wx = l > 1e-6f ? x / l : 0.7071f; *wy = l > 1e-6f ? y / l : 0.7071f;
}
/* um comando de ataque/habilidade: no alvo (tipo,id) ou, sem alvo, no chao a frente do heroi */
static void attack_command(Hold *h)
{
    int right = h->right;
    if (h->unit) {
        native_queue(right ? NC_RIGHT_UNIT : NC_LEFT_UNIT, rd32(h->unit), unit_id(h->unit));
    } else {
        float wx, wy;
        face_world(&wx, &wy);
        native_queue(right ? NC_RIGHT_AT : NC_LEFT_AT, (DWORD)(g_px + wx * 6.0f + 0.5f), (DWORD)(g_py + wy * 6.0f + 0.5f));
    }
    h->last = GetTickCount();
}
/* estagio 1 = ataque/habilidade segurado; 2 = interagir */
static void begin_attack(Hold *h, int right)
{
    Target t = cfg.aim ? find_target(1, 0) : (Target){ T_NONE, 0, 0, 0, 0 };
    h->right = right;
    h->stage = 1;
    h->unit = t.kind == T_HOSTILE ? t.unit : 0;
    InterlockedExchange(&g_mv_busy, 1);
    if (!h->unit && is_town(g_level) && !right) { h->last = GetTickCount(); return; }  /* cidade sem alvo: nada */
    attack_command(h);
    dbg("%s: %s", right ? "habilidade direita" : "ataque", h->unit ? "no alvo" : "a frente");
}
static void continue_attack(Hold *h)
{
    DWORD now = GetTickCount();
    if (h->stage == 2) {                                   /* interagir: anda ate o alvo e interage */
        int i;
        for (i = 0; i < g_nunits; i++)
            if (g_units[i].unit == h->unit) {
                float d = sqrtf((g_units[i].x - g_px) * (g_units[i].x - g_px) + (g_units[i].y - g_py) * (g_units[i].y - g_py));
                if (d <= 3.0f && !h->shift) {
                    native_queue(NC_INTERACT, rd32(h->unit), unit_id(h->unit));
                    h->shift = 1;                          /* uma interacao por toque */
                    dbg("interagir: alvo a %.1f, comando enviado", d);
                }
                break;
            }
        return;
    }
    if (!h->unit || !target_still_valid(h->unit)) {        /* sem alvo ou alvo morreu: procura de novo */
        Target t = find_target(1, 0);
        DWORD was = h->unit;
        h->unit = t.kind == T_HOSTILE ? t.unit : 0;
        if (h->unit && h->unit != was) dbg("%s: novo alvo", h->right ? "habilidade direita" : "ataque");
    }
    if (!h->unit && is_town(g_level) && !h->right) return;
    /* repete so quando o heroi terminou o golpe anterior: sem alternancia nem rajada */
    if (now - h->last >= 120 && player_ready()) attack_command(h);
}
static void end_attack(Hold *h)
{
    (void)h;
    InterlockedExchange(&g_mv_busy, 0);
}
/* interacao pendente: o B foi solto antes de o heroi chegar ao alvo */
static DWORD g_pend_unit, g_pend_t;
static void pending_interact_tick(void)
{
    int i;
    if (!g_pend_unit) return;
    if (GetTickCount() - g_pend_t > 4000 || g_mode != M_GAME) { g_pend_unit = 0; return; }
    for (i = 0; i < g_nunits; i++)
        if (g_units[i].unit == g_pend_unit) {
            float d = sqrtf((g_units[i].x - g_px) * (g_units[i].x - g_px) + (g_units[i].y - g_py) * (g_units[i].y - g_py));
            if (d <= 3.0f) {
                native_queue(NC_INTERACT, rd32(g_pend_unit), unit_id(g_pend_unit));
                dbg("interagir: alvo a %.1f, comando enviado (depois de soltar)", d);
                g_pend_unit = 0;
            }
            return;
        }
    g_pend_unit = 0;
}
static void begin_interact(Hold *h)
{
    g_pend_unit = 0;
    Target t = find_target(0, 1);
    h->stage = 2;
    h->shift = 0;
    h->unit = t.kind == T_INTERACT ? t.unit : 0;
    if (!h->unit) { h->active = 0; return; }
    native_queue(NC_RUN_TO_UNIT, rd32(h->unit), unit_id(h->unit));
    dbg("interagir: indo ate o alvo tipo %lu", rd32(h->unit));
}

/* Abre o teclado flutuante do Autorun (Wine-NX), o mesmo do atalho - + R3. O texto digitado
 * chega ao jogo como WM_CHAR, que e o que a caixa de nome do D2 le. O codigo 30 de
 * NtUserCallHwndParam (ShowSoftwareKeyboard) so existe no win32u do Autorun, por isso a chamada
 * so e feita quando o ntdll exporta os simbolos do Wine-NX; fora dele cai no nome fixo. */
#define NX_CALLHWNDPARAM_SHOWSOFTWAREKEYBOARD 30
static int is_wine_nx(void)
{
    static int known = -1;
    if (known < 0) {
        HMODULE nt = GetModuleHandleA("ntdll.dll");
        known = nt && (GetProcAddress(nt, "wine_nx_pe_teb") || GetProcAddress(nt, "wine_nx_user_shared_data"));
        lg("Wine-NX %s", known ? "detectado (teclado do console disponivel)" : "nao detectado");
    }
    return known;
}

static void open_keyboard(void)
{
    typedef ULONG_PTR (WINAPI *CallHwndParam)(HWND, DWORD_PTR, DWORD);
    static CallHwndParam call;
    if (!g_hwnd) return;
    if (!is_wine_nx()) { type_string("Switch"); return; }
    if (!call) {
        HMODULE w = GetModuleHandleA("win32u.dll");
        if (!w) w = LoadLibraryA("win32u.dll");
        if (w) call = (CallHwndParam)GetProcAddress(w, "NtUserCallHwndParam");
    }
    if (!call) { lg("teclado: NtUserCallHwndParam indisponivel"); type_string("Switch"); return; }
    release_all();
    ULONG_PTR ok = call(g_hwnd, 0, NX_CALLHWNDPARAM_SHOWSOFTWAREKEYBOARD);
    lg("teclado do console: %s", ok ? "aberto" : "falhou");
    if (ok) g_osk_wait = 1;
}

/* Editor de nome. O teclado do Autorun abre mas nao aparece com o D2GL (o runtime nao desenha o teclado
 * nem num programa OpenGL minimo, testado no Ryujinx), entao o nome e editado pelo controle e o proprio
 * campo do jogo mostra o texto. Texto por WM_CHAR com contagem 1 e apagar por WM_KEYDOWN VK_BACK, os dois
 * postados (testado no Ryujinx, uma forma de cada vez, conferindo a caixa de nome nas capturas). */
static const char *g_name_ideas[] = {
    "Aldric", "Kaela", "Toran", "Mirela", "Brann", "Selis", "Varek", "Ilyra", "Doran", "Neris",
    "Garon", "Lysara", "Theron", "Zaria", "Corvin", "Elara", "Rhagar", "Sylas", "Tamsin", "Valen"
};
static struct { int on, len, rep_b; char name[16]; DWORD rep_t; } g_ned;

static void ned_type(const char *n)
{
    int i;
    for (i = 0; i < 16; i++) post_back();
    g_ned.len = 0;
    for (; *n && g_ned.len < 15; n++) { post_char(*n); g_ned.name[g_ned.len++] = *n; }
    g_ned.name[g_ned.len] = 0;
}
static void ned_begin(const char *n)
{
    release_all();
    g_ned.on = 1;
    g_ned.rep_b = -1;
    ned_type(n ? n : g_name_ideas[GetTickCount() % (sizeof(g_name_ideas) / sizeof(g_name_ideas[0]))]);
    lg("editor de nome: aberto com '%s'", g_ned.name);
}
static void ned_end(const char *why)
{
    if (!g_ned.on) return;
    g_ned.on = 0;
    lg("editor de nome: %s ('%s')", why, g_ned.name);
}
/* cima (+1) / baixo (-1): troca a ultima letra (a 1a e maiuscula) */
static void ned_cycle(int dir)
{
    char base, c;
    if (!g_ned.len) { post_char('A'); g_ned.name[g_ned.len++] = 'A'; g_ned.name[g_ned.len] = 0; return; }
    base = g_ned.len == 1 ? 'A' : 'a';
    c = g_ned.name[g_ned.len - 1];
    if (c < base || c > base + 25) c = base;
    c = (char)(base + (c - base + dir + 26) % 26);
    post_back();
    post_char(c);
    g_ned.name[g_ned.len - 1] = c;
}
static void ned_add(void)
{
    char c = g_ned.len ? 'a' : 'A';
    if (g_ned.len >= 15) return;
    post_char(c);
    g_ned.name[g_ned.len++] = c;
    g_ned.name[g_ned.len] = 0;
}
static void ned_del(void)
{
    if (!g_ned.len) return;
    post_back();
    g_ned.name[--g_ned.len] = 0;
}
/* Botoes do controle com o editor aberto (so fora da partida). Devolve 1 se o editor tratou. */
static int name_editor_tick(void)
{
    static const int dirs[] = { B_UP, B_DOWN, B_LEFT, B_RIGHT };
    DWORD now = GetTickCount();
    unsigned i;
    if (!g_ned.on) return 0;
    if (PRESSED(B_PLUS) || PRESSED(B_X)) { ned_end("Enter"); tap_key(VK_RETURN, 0); return 1; }
    if (PRESSED(B_A)) { ned_end("ok"); return 1; }
    if (PRESSED(B_B) || PRESSED(B_R3) || PRESSED(B_L3) || PRESSED(B_MINUS)) { ned_end("fechado"); return 1; }
    if (PRESSED(B_Y)) {
        ned_type(g_name_ideas[now % (sizeof(g_name_ideas) / sizeof(g_name_ideas[0]))]);
        return 1;
    }
    for (i = 0; i < 4; i++) {
        int b = dirs[i], fire = 0;
        if (PRESSED(b)) { fire = 1; g_ned.rep_b = b; g_ned.rep_t = now + 350; }
        else if (g_pad.down[b] && g_ned.rep_b == b && now >= g_ned.rep_t) { fire = 1; g_ned.rep_t = now + 90; }
        if (!fire) continue;
        if (b == B_UP) ned_cycle(1);
        else if (b == B_DOWN) ned_cycle(-1);
        else if (b == B_RIGHT) ned_add();
        else ned_del();
    }
    return 1;
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
    case A_LCLICK: click(0, 0); h->active = 0; if (g_mode == M_GAME) park_cursor(); break;
    case A_RCLICK: click(1, 0); h->active = 0; if (g_mode == M_GAME) park_cursor(); break;
    case A_SHIFTCLICK: click(0, 1); h->active = 0; if (g_mode == M_GAME) park_cursor(); break;
    case A_CTRLCLICK: click(0, 2); h->active = 0; if (g_mode == M_GAME) park_cursor(); break;
    case A_SHIFTRCLICK: click(1, 1); h->active = 0; if (g_mode == M_GAME) park_cursor(); break;
    case A_POPUP_R: do_popup(1); h->active = 0; if (g_mode == M_GAME) park_cursor(); break;
    case A_POPUP_L: do_popup(0); h->active = 0; if (g_mode == M_GAME) park_cursor(); break;
    case A_NAV: h->last = GetTickCount(); h->stage = 0; break;
    case A_TYPENAME: type_string("Switch"); h->active = 0; break;
    case A_KEYBOARD: if (g_mode == M_GAME) open_keyboard(); else ned_begin(NULL); h->active = 0; break;
    case A_INTERACT: begin_interact(h); break;
    case A_OSK: open_keyboard(); h->active = 0; break;
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
    case A_ATTACK: case A_RIGHT: case A_INTERACT: continue_attack(h); break;
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
    case A_INTERACT:   /* soltou antes de chegar: a interacao continua sozinha (um toque no B basta) */
        if (h->stage == 2 && h->unit && !h->shift) { g_pend_unit = h->unit; g_pend_t = GetTickCount(); }
        break;
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
 * Menu de Gameplay (Switch) - Fila Segura SPSC sem lock e sem deadlock
 * ==================================================================================== */
#define D2NX_MSGQ_LEN 16
#define D2NX_MSG_MAX  160

typedef struct { wchar_t text[D2NX_MSG_MAX]; int color; } D2NXMsg;
static D2NXMsg g_msgq[D2NX_MSGQ_LEN];
static volatile LONG g_msgq_head = 0;
static volatile LONG g_msgq_tail = 0;
static HHOOK g_getmsg_hook = NULL;
static DWORD g_game_tid = 0;

typedef void (__stdcall *PrintGameString_t)(const wchar_t *msg, int color);
static PrintGameString_t pPrintGameString = NULL;

static void drain_msgq(void)
{
    if (!pPrintGameString) return;
    while (g_msgq_tail != g_msgq_head) {
        D2NXMsg *m = &g_msgq[g_msgq_tail % D2NX_MSGQ_LEN];
        if (m->text[0]) {
            pPrintGameString(m->text, m->color);
        }
        InterlockedExchange(&g_msgq_tail, g_msgq_tail + 1);
    }
}

static LRESULT CALLBACK getmsg_hook(int code, WPARAM wp, LPARAM lp)
{
    if (code == HC_ACTION && wp == PM_REMOVE) {
        drain_msgq();
    }
    return CallNextHookEx(g_getmsg_hook, code, wp, lp);
}

static void d2nx_install_hook(void)
{
    if (g_getmsg_hook || !g_hwnd) return;
    g_game_tid = GetWindowThreadProcessId(g_hwnd, NULL);
    if (g_game_tid) {
        g_getmsg_hook = SetWindowsHookExW(WH_GETMESSAGE, getmsg_hook, NULL, g_game_tid);
        lg("Hook WH_GETMESSAGE na game thread tid=%lu: %p (erro %lu)", g_game_tid, g_getmsg_hook,
           g_getmsg_hook ? 0UL : GetLastError());
    }
}

static void d2nx_remove_hook(void)
{
    if (g_getmsg_hook) {
        UnhookWindowsHookEx(g_getmsg_hook);
        g_getmsg_hook = NULL;
    }
}

static void d2_print_string(const wchar_t *wmsg, int color)
{
    if (!pPrintGameString || !wmsg || !wmsg[0]) return;
    LONG h = g_msgq_head;
    if (h - g_msgq_tail >= D2NX_MSGQ_LEN) return;
    D2NXMsg *m = &g_msgq[h % D2NX_MSGQ_LEN];
    lstrcpynW(m->text, wmsg, D2NX_MSG_MAX);
    m->color = color;
    InterlockedExchange(&g_msgq_head, h + 1);
    if (g_hwnd && IsWindow(g_hwnd)) {
        PostMessageW(g_hwnd, WM_NULL, 0, 0);
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

    /* charstats.bin NAO e gerado: colocar "gld " no kit inicial cria um item invalido e o Median XL
     * recusa o personagem novo ("Generic bad character file"). Apaga copias de versoes anteriores
     * para o jogo voltar a usar a tabela original do MPQ. */
    char o_cs[MAX_PATH];
    snprintf(o_cs, sizeof(o_cs), "%scharstats.bin", excel_dir);
    if (DeleteFileA(o_cs)) lg("[GameplayMenu] charstats.bin antigo removido (ouro inicial desativado)");

    lg("[GameplayMenu] Salvo e aplicado com sucesso! XP=%.1fx, Runas=%.1fx, Unicos=%.2fx, Raros=%.2fx, Ouro=%d",
       cfg.xp_multiplier, cfg.rune_multiplier, cfg.unique_multiplier, cfg.rare_multiplier, cfg.start_gold);
}

/* O menu e desenhado a cada quadro pelo gancho (draw_cheat_menu); aqui so marca a atualizacao. */
static void d2_render_gameplay_menu(void)
{
    g_gp_menu.last_refresh = GetTickCount();
}

/* Desenho nativo do jogo, conferido nas chamadas do proprio D2Client 1.13c:
 * D2Win 10150 DrawText (fastcall: texto, x, y, cor, 0), D2Win 10184 SetFont (fastcall: fonte),
 * D2Gfx 10014 DrawRectangle (stdcall: x1, y1, x2, y2, cor, transparencia). */
typedef void (__fastcall *D2DrawText_t)(const wchar_t *, int, int, int, int);
typedef DWORD (__fastcall *D2SetFont_t)(DWORD);
typedef void (__stdcall *D2DrawRect_t)(int, int, int, int, DWORD, DWORD);
static D2DrawText_t pDrawText;
static D2SetFont_t pSetFont;
static D2DrawRect_t pDrawRect;
static wchar_t g_menu_status[96];
static DWORD g_menu_status_t;

static void draw_init(void)
{
    HMODULE w = GetModuleHandleA("D2Win.dll"), gx = GetModuleHandleA("D2gfx.dll");
    if (w) {
        pDrawText = (D2DrawText_t)(void *)GetProcAddress(w, (LPCSTR)10150);
        pSetFont = (D2SetFont_t)(void *)GetProcAddress(w, (LPCSTR)10184);
    }
    if (gx) pDrawRect = (D2DrawRect_t)(void *)GetProcAddress(gx, (LPCSTR)10014);
    lg("desenho nativo: DrawText %p, SetFont %p, DrawRectangle %p", pDrawText, pSetFont, pDrawRect);
}

static void menu_status(const wchar_t *msg)
{
    lstrcpynW(g_menu_status, msg, sizeof(g_menu_status) / sizeof(g_menu_status[0]));
    g_menu_status_t = GetTickCount();
}

static void draw_cheat_menu(void)
{
    static const wchar_t *names[MENU_ITEM_COUNT] = {
        L"Multiplicador de XP", L"Queda de runas", L"Itens unicos", L"Itens raros", L"Ouro inicial", L"Salvar e aplicar"
    };
    wchar_t line[128];
    int i, x = 48, y = 92, lh = 24;
    if (!pDrawText || !pSetFont) return;
    if (pDrawRect) pDrawRect(x - 16, y - 34, x + 470, y + lh * (MENU_ITEM_COUNT + 2) + 8, 0, 1);
    pSetFont(1);
    pDrawText(L"MENU DE CHEATS", x, y, 4, 0);
    pSetFont(0);
    for (i = 0; i < MENU_ITEM_COUNT; i++) {
        int sel = g_gp_menu.cur_item == i;
        switch (i) {
        case 0: _snwprintf(line, 128, L"%ls %ls: < %.1fx >", sel ? L">" : L" ", names[i], g_xp_opts[g_gp_menu.xp_idx]); break;
        case 1: _snwprintf(line, 128, L"%ls %ls: < %.1fx >", sel ? L">" : L" ", names[i], g_rune_opts[g_gp_menu.rune_idx]); break;
        case 2: _snwprintf(line, 128, L"%ls %ls: < %.2fx >", sel ? L">" : L" ", names[i], g_unique_opts[g_gp_menu.unique_idx]); break;
        case 3: _snwprintf(line, 128, L"%ls %ls: < %.2fx >", sel ? L">" : L" ", names[i], g_rare_opts[g_gp_menu.rare_idx]); break;
        case 4: _snwprintf(line, 128, L"%ls %ls: indisponivel no Median XL", sel ? L">" : L" ", names[i]); break;
        default: _snwprintf(line, 128, L"%ls [ %ls ]", sel ? L">" : L" ", names[i]); break;
        }
        line[127] = 0;
        pDrawText(line, x, y + lh * (i + 1) + 4, i == 4 ? 5 : sel ? 2 : 0, 0);
    }
    pDrawText(L"Direcional: escolhe/altera   A: aplica   B: fecha", x, y + lh * (MENU_ITEM_COUNT + 1) + 4, 5, 0);
    if (g_menu_status[0] && GetTickCount() - g_menu_status_t < 5000)
        pDrawText(g_menu_status, x, y + lh * (MENU_ITEM_COUNT + 2) + 4, 4, 0);
}

static volatile LONG g_walk_log;     /* WalkTest: registra cada quadro */
#define MOVE_REACH 7.0f      /* subtiles a frente do heroi a cada ordem de movimento */
#define MOVE_REPEAT_MS 130   /* intervalo entre ordens enquanto o analogico estiver inclinado */

/* Ordem de movimento estavel (ModoOrdem=1). O jogo guarda a direcao do heroi em 64 passos de 5,625 graus,
 * arredondando para baixo a partir de 45 graus no mundo: dir = floor((atan2(dy,dx) - 45) / 5,625), medido no
 * Ryujinx (WalkTest). As 8 direcoes principais do analogico caem exatamente na fronteira entre duas direcoes:
 * com o alvo a 5 subtiles, recalculado a cada 130 ms, o desvio de meio passo do proprio caminho fazia a
 * direcao alternar (31/32...) a cada passo do jogo e o sprite trocava de lado: a "tremida/passinhos".
 * Agora a direcao do analogico e encaixada no CENTRO de uma das 64 direcoes (com histerese), o alvo fica a
 * MV_REACH subtiles exatamente nessa direcao (ponto inteiro com menor erro de angulo) e a ordem so e
 * renovada quando a direcao muda, o alvo fica perto, o heroi para ou anda/corre muda. */
#define MV_REACH 18.0f
#define MV_RENEW 9.0f
#define DEG(r) ((r) * 57.29577951f)
#define RAD(d) ((d) * 0.01745329252f)
static int g_mv_bin = -1;
static float g_sm_x, g_sm_y;             /* direcao do analogico suavizada */
static int g_sm_on;
static int g_mv_tx, g_mv_ty;          /* ultimo alvo mandado (diagnostico) */
static void move_frame(DWORD player, float px, float py, DWORD now, DWORD *last_mv)
{
    static float tgx, tgy, lpx, lpy;
    static int last_run = -1;
    static DWORD still_t;
    float f = (DEG(atan2f(g_mv_wy, g_mv_wx)) - 45.0f) / 5.625f, d;
    DWORD m = rd32(player + U_MODE);
    int need = 0, bin;
    while (f < 0) f += 64.0f;
    while (f >= 64.0f) f -= 64.0f;
    bin = g_mv_bin;
    if (bin >= 0) {                                      /* histerese */
        d = f - (bin + 0.5f);
        while (d > 32) d -= 64;
        while (d < -32) d += 64;
        if (fabsf(d) > 1.0f) bin = -1;               /* troca so com meia direcao (2,8 graus) alem da borda */
    }
    if (bin < 0) { bin = (int)f & 63; need = 1; }
    if ((int)g_mv_run != last_run) need = 1;
    if ((tgx - px) * (tgx - px) + (tgy - py) * (tgy - py) < MV_RENEW * MV_RENEW) need = 1;
    if (m != 2 && m != 3 && m != 6 && now - *last_mv > 150) need = 1;        /* parou (fim do caminho/bloqueio) */
    if (px != lpx || py != lpy) { lpx = px; lpy = py; still_t = now; }
    else if (now - still_t > 300 && now - *last_mv > 300) need = 1;          /* preso */
    if (!need) return;
    {
        float a = RAD(45.0f + (bin + 0.5f) * 5.625f), ca = cosf(a), sa = sinf(a), best = 1e9f;
        int ix, iy, bx = 0, by = 0;
        float cx = px + ca * MV_REACH, cy = py + sa * MV_REACH;
        for (ix = -2; ix <= 2; ix++)
            for (iy = -2; iy <= 2; iy++) {
                float qx = floorf(cx) + ix, qy = floorf(cy) + iy, vx = qx - px, vy = qy - py;
                float l = sqrtf(vx * vx + vy * vy), e = fabsf(vx * sa - vy * ca) / (l > 1 ? l : 1);
                if (e < best) { best = e; bx = (int)qx; by = (int)qy; }
            }
        tgx = (float)bx; tgy = (float)by;
        do_command(player, (DWORD)bx, (DWORD)by, g_mv_run ? NC_RUN : NC_WALK);
        g_mv_tx = bx; g_mv_ty = by;
        g_mv_bin = bin;
        last_run = (int)g_mv_run;
        *last_mv = now;
        still_t = now;
    }
}

/* Um quadro do jogo, na thread do jogo: comandos nativos, movimento, mensagens e menu. */
static void game_frame(void)
{
    static int was_moving;
    static DWORD last_mv;
    DWORD now = GetTickCount(), player;
    float px, py;
    InterlockedIncrement(&g_frames);
    player = (g_ver && g_ver->player) ? rd32(g_client + g_ver->player) : 0;
    while (g_natq_tail != g_natq_head) {
        NatCmd c = g_natq[g_natq_tail % NATQ];
        InterlockedExchange(&g_natq_tail, g_natq_tail + 1);
        if (player) do_command(player, c.a, c.b, (DWORD)c.cmd);
    }
    if (player && g_mv_active && !g_mv_busy && cfg.move_mode == 0) {      /* ordem antiga (ModoOrdem=0) */
        if (now - last_mv >= MOVE_REPEAT_MS && unit_pos(player, 0, &px, &py)) {
            float tx = px + g_mv_wx * MOVE_REACH, ty = py + g_mv_wy * MOVE_REACH;
            do_command(player, (DWORD)(tx + 0.5f), (DWORD)(ty + 0.5f), g_mv_run ? NC_RUN : NC_WALK);
            g_mv_tx = (int)(tx + 0.5f); g_mv_ty = (int)(ty + 0.5f);
            last_mv = now;
            was_moving = 1;
        }
    } else if (player && g_mv_active && !g_mv_busy) {
        if (unit_pos(player, 0, &px, &py)) move_frame(player, px, py, now, &last_mv);
        was_moving = 1;
    } else if (was_moving) {
        /* analogico solto: ordem de andar ate onde o heroi esta = para ali (o ataque tem prioridade) */
        if (player && !g_mv_busy && unit_pos(player, 0, &px, &py))
            do_command(player, (DWORD)(px + 0.5f), (DWORD)(py + 0.5f), NC_WALK);
        was_moving = 0;
        last_mv = 0;
        g_mv_bin = -1;
    }
    if (cfg.walktest && player && g_walk_log) {
        DWORD path = rd32(player + U_PATH);
        lg("[WF] %lu pos %u.%05u,%u.%05u modo %lu gfx %lu dir %u alvo %u,%u mv %ld ordem %d,%d",
           now, rd16(path + 2), rd16(path), rd16(path + 6), rd16(path + 4), rd32(player + U_MODE),
           rd32(player + 0x44), rd8(path + 0x64), rd16(path + 0x10), rd16(path + 0x12), (long)g_mv_active, g_mv_tx, g_mv_ty);
        if (cfg.walktest > 1) {
            char b[700]; int o = 0, k;
            for (k = 0x08; k < 0xC0; k += 4) o += snprintf(b + o, sizeof(b) - o, "%02X:%lX ", k, rd32(path + k));
            lg("[WP] %s", b);
        }
    }
    drain_msgq();
    if (g_gp_menu.open) draw_cheat_menu();
}

static void __cdecl frame_hook(void)
{
    static int logged;
    game_frame();
    if (!g_cursor_hide_now) {
        if (!logged) { logged = 1; lg("gancho: chamando o desenho original do cursor (tipo %ld)", (long)rd32(g_client + 0x11B854)); }
        ((void (*)(void))g_cursor_tramp)();
        if (logged == 1) { logged = 2; lg("gancho: desenho original do cursor voltou"); }
    }
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

/* AutoTest: cria um personagem novo, entra, salva e sai, e carrega de novo.
 * sx,sy = ponto como aparece na tela 1280x720. Nos menus o D2GL desenha os 800x600 esticados
 * para 16:9, mas converte o mouse como se o jogo ocupasse 960x720 centralizado (4:3): o ponto
 * de janela que acerta o alvo e x = 160 + 0.75*sx. So para o teste; desligado por padrao. */
static void autocreate_click(float sx, float sy)
{
    int i;
    float cx = g_player ? sx : 160.0f + 0.75f * sx;
    move_cursor(cx * (float)g_resx / 1280.0f, sy * (float)g_resy / 720.0f);
    /* como o botao A: aperta, segura e solta. No emulador um clique isolado as vezes nao e
     * aceito pelo menu; dois cliques no mesmo ponto passaram de forma consistente. */
    for (i = 0; i < 2; i++) {
        Sleep(250);
        send_button(0, 1);
        Sleep(150);
        send_button(0, 0);
    }
}

/* Print Screen do proprio jogo: deixa um screenshot no SD para conferir cada passo */
static void autocreate_shot(const char *what)
{
    lg("[AutoCreate] screenshot: %s", what);
    tap_key(VK_SNAPSHOT, 0);
    Sleep(300);
}

static void autocreate_tick(int mode)
{
    static DWORD t0;
    static int stage;
    DWORD now = GetTickCount();
    if (!t0) t0 = now;
    if (cfg.autocreate == 9) {   /* so abre o teclado do console no menu, para conferir o desenho */
        if (!stage && mode == M_MENU && now - t0 >= 12000) {
            lg("[AutoCreate] modo 9: abrindo o teclado do console");
            open_keyboard();
            stage = 99;
        }
        return;
    }
    switch (stage) {
    case 0:   /* menu principal */
        if (mode == M_MENU && now - t0 >= 8000) {
            lg("[AutoCreate] menu principal: Single Player");
            autocreate_click(640, 523);
            stage = 1; t0 = now;
        }
        break;
    case 1:   /* selecao de personagem (no emulador a lista demora a carregar) */
        if (now - t0 >= 20000) {
            /* sem personagens o jogo abre direto na selecao de classe (AutoCreate=1);
             * AutoCreate=2 = ja existem personagens, clicar em Create New */
            autocreate_shot("apos Single Player");
            if (cfg.autocreate == 2) {
                lg("[AutoCreate] selecao: Create New Character");
                autocreate_click(148, 478);
            }
            stage = 2; t0 = now;
        }
        break;
    case 2:   /* selecao de classe */
        if (now - t0 >= 10000) {
            autocreate_shot("selecao de classe");
            lg("[AutoCreate] classe: Amazon");
            autocreate_click(195, 440);
            stage = 3; t0 = now;
        }
        break;
    case 3:   /* nome + Enter */
        if (now - t0 >= 6000) {
            /* as mesmas funcoes do editor de nome do R3: Teste, cima (Testf), direita (Testfa),
             * esquerda (Testf), baixo, baixo (Testd). O save tem de sair como Testd. */
            lg("[AutoCreate] editor de nome: Teste, cima, direita, esquerda, baixo, baixo (esperado Testd)");
            autocreate_shot("classe escolhida");
            ned_begin("Teste"); Sleep(300);
            ned_cycle(1); Sleep(200);
            ned_add(); Sleep(200);
            ned_del(); Sleep(200);
            ned_cycle(-1); Sleep(200);
            ned_cycle(-1); Sleep(300);
            ned_end("Enter (teste)");
            autocreate_shot("nome digitado");
            tap_key(VK_RETURN, 0);   /* o mesmo Enter do X/+ no menu */
            stage = 4; t0 = now;
        }
        break;
    case 4:   /* esperando entrar no jogo */
        if (mode == M_GAME) {
            lg("[AutoCreate] SUCESSO 1/2: personagem novo entrou no jogo (player %08lX pos %.1f,%.1f)",
               g_player, g_px, g_py);
            stage = 5; t0 = now;
        } else if (now - t0 >= 45000) {
            autocreate_shot("falha ao entrar");
            lg("[AutoCreate] FALHA: personagem novo nao entrou no jogo em 45 s");
            stage = 99;
        }
        break;
    case 5:   /* salvar e sair */
        if (now - t0 >= 8000) {
            lg("[AutoCreate] ESC -> Save and Exit (res %dx%d)", g_resx, g_resy);
            /* o mesmo toque do + no jogo; "Save and Exit" e a 2a linha do menu (capturas do emulador) */
            tap_key(VK_ESCAPE, 0);
            Sleep(2500);
            autocreate_click(640, 317);
            now = GetTickCount();
            stage = 6; t0 = now;
        }
        break;
    case 6:   /* de volta ao menu: carregar de novo */
        if (mode == M_MENU && now - t0 >= 8000) {
            lg("[AutoCreate] menu principal: Single Player para recarregar");
            autocreate_click(640, 523);
            stage = 7; t0 = now;
        } else if (mode == M_GAME && now - t0 >= 20000) {
            lg("[AutoCreate] FALHA: Save and Exit nao voltou ao menu");
            stage = 99;
        }
        break;
    case 7:
        if (now - t0 >= 20000) {
            lg("[AutoCreate] selecao: OK no personagem mais recente");
            autocreate_click(668, 545);
            tap_key(VK_RETURN, 0);
            stage = 8; t0 = now;
        }
        break;
    case 8:
        if (mode == M_GAME) {
            lg("[AutoCreate] SUCESSO 2/2: personagem recarregado do save (player %08lX)", g_player);
            stage = 99;
        } else if (now - t0 >= 45000) {
            lg("[AutoCreate] FALHA: personagem salvo nao carregou em 45 s");
            stage = 99;
        }
        break;
    default:
        break;
    }
}

/* AutoTest AutoWalk=2: cenario de aceitacao da camada nativa (injeta o analogico/botao como o controle):
 * zona morta, inclinacao leve, diagonal maxima, soltar, sair da cidade para o leste e segurar A no campo.
 * Registra a cada 0,5 s posicao, modo do jogador, mouse do jogo (tem de ficar parado: nada de mouse) e alvo. */
/* analogico que aponta do heroi para um ponto do mundo (como uma pessoa dirigindo ate la) */
static int stick_toward(float tx, float ty, float *lx, float *ly)
{
    float dx = tx - g_px, dy = ty - g_py, sx, sy, l;
    if (dx * dx + dy * dy < 9.0f) return 0;            /* chegou (3 subtiles) */
    sx = (dx - dy) * 16.0f; sy = (dx + dy) * 8.0f;    /* mundo -> tela */
    l = sqrtf(sx * sx + sy * sy);
    *lx = sx / l * 0.95f; *ly = sy / l * 0.95f;
    return 1;
}
static void autowalk_acceptance(DWORD t)
{
    static DWORD last_log, ph_t0;
    static int phase = 0;
    float lx = 0, ly = 0;
    int press_a = 0, press_b = 0, press_x = 0, next = phase;
    DWORD in = t - ph_t0;
    switch (phase) {
    case 0: if (t >= 10000) next = 1; break;                                   /* lagada de entrada */
    case 1: lx = 0.12f; ly = 0.05f; if (in >= 3000) next = 2; break;           /* zona morta: nao anda */
    case 2: lx = 0.40f; if (in >= 3000) next = 3; break;                       /* leve: anda */
    case 3: if (in >= 3000) next = 4; break;                                   /* solto andando: para */
    case 4: lx = 1.0f; ly = 1.0f; if (in >= 2000) next = 40; break;            /* diagonal do quadrado */
    case 40: {                                                                 /* vai ate o NPC mais perto */
        int i, best = -1; float bd = 1e9f;
        for (i = 0; i < g_nunits; i++)
            if (g_units[i].type == 1 && (g_units[i].flags & 0x2) && !monster_is_ally(&g_units[i])) {
                float d = (g_units[i].x - g_px) * (g_units[i].x - g_px) + (g_units[i].y - g_py) * (g_units[i].y - g_py);
                if (d < bd) { bd = d; best = i; }
            }
        if (best < 0 || bd < 36.0f || in >= 15000) next = 42;
        else stick_toward(g_units[best].x, g_units[best].y, &lx, &ly);
        break;
    }
    case 42: press_b = in < 300; if (in >= 6000) next = 41; break;             /* B: interagir */
    case 41: press_b = in < 300; if (in >= 3000) next = 5; break;              /* B de novo: fecha o painel */
    case 5: if (!stick_toward(4340.0f, 5444.0f, &lx, &ly) || in >= 15000) next = 6; break;
    case 6: if (!stick_toward(4480.0f, 5446.0f, &lx, &ly) || (g_level > 0 && !is_town(g_level)) || in >= 30000) next = 7; break;
    case 7: if (!stick_toward(4560.0f, 5446.0f, &lx, &ly) || in >= 8000) next = 8; break;   /* entra no campo */
    case 8: press_a = 1; if (in >= 15000) next = 81; break;                     /* segura A */
    case 81: press_x = 1; if (in >= 6000) next = 9; break;                      /* segura X */
    default: break;
    }
    if (next != phase) {
        phase = next; ph_t0 = t;
        lg("[Aceite] fase %d (t=%lus)", phase, t / 1000);
    }
    g_pad.connected = 1;
    g_pad.lx = lx; g_pad.ly = ly;
    g_pad.down[B_A] = press_a;
    g_pad.down[B_B] = press_b;
    g_pad.down[B_X] = press_x;
    if (t - last_log >= 500 && phase > 0) {
        Target tg = find_target(1, 0);
        last_log = t;
        lg("[Aceite] t=%.1fs fase %d analogico %.2f,%.2f A=%d B=%d X=%d pos %.1f,%.1f modo %lu nivel %d painel %d mouse %ld,%ld alvo %s quadros %ld",
           t / 1000.0f, phase, lx, ly, press_a, press_b, press_x, g_px, g_py, g_player ? rd32(g_player + U_MODE) : 0, g_level,
           any_panel_open(), g_ver ? (long)rd32(g_client + g_ver->mouse_x) : -1, g_ver ? (long)rd32(g_client + g_ver->mouse_y) : -1,
           tg.kind == T_HOSTILE ? "monstro" : "nenhum", (long)g_frames);
    }
}

/* AutoTest Script=<arquivo>: roteiro de teste que aperta os botoes do controle pelo mesmo caminho do XInput
 * (para testar menus/loja/inventario no emulador). Um comando por linha, depois que o personagem entra:
 *   wait <ms> | pad <Botao> [ms] | stick <lx> <ly> <ms> | rstick <rx> <ry> <ms> | key <TECLA> | move <x> <y>
 *   click | rclick | npc [cls] (anda ate o NPC mais perto e aperta B) | waitpanel <id hex> <ms> | mark <texto> | log */
static struct {
    char lines[200][80]; int n, i, on;
    DWORD until, hold_until; int hold_b; float lx, ly, rx, ry; int npc, npc_cls; DWORD npc_t0;
} g_scr;
static void script_load(const char *rel)
{
    char path[MAX_PATH]; FILE *f; char l[120];
    snprintf(path, sizeof(path), "%s%s", g_dir, rel);
    f = fopen(path, "rb");
    if (!f) { lg("[Script] nao abriu %s", path); return; }
    while (g_scr.n < 200 && fgets(l, sizeof(l), f)) {
        char *e = l + strlen(l);
        while (e > l && (e[-1] == '\n' || e[-1] == '\r' || e[-1] == ' ')) *--e = 0;
        if (l[0] && l[0] != '#') lstrcpynA(g_scr.lines[g_scr.n++], l, 80);
    }
    fclose(f);
    g_scr.hold_b = -1;
    lg("[Script] %d comandos de %s", g_scr.n, path);
}
static void script_load_ini(void)
{
    char sc[MAX_PATH];
    GetPrivateProfileStringA("AutoTest", "Script", "", sc, sizeof(sc), g_ini);
    if (sc[0]) { script_load(sc); g_scr.on = g_scr.n > 0; }
}
static int btn_from_name(const char *n)
{
    int b;
    for (b = 0; b < B_COUNT; b++) if (!lstrcmpiA(n, btn_names[b])) return b;
    return -1;
}
static void script_log_state(const char *tag)
{
    int mx = -1, my = -1, k; char b[200] = "";
    game_mouse(&mx, &my);
    for (k = 1; k < UI_MAX; k++) if (ui_open(k)) snprintf(b + strlen(b), sizeof(b) - strlen(b), " %02X", k);
    lg("[Script] %s: mouse jogo %d,%d cursor %.0f,%.0f paineis%s", tag, mx, my, g_curx, g_cury, b[0] ? b : " nenhum");
}
static void script_tick(void)
{
    DWORD now = GetTickCount();
    if (!g_scr.on) return;
    if (g_scr.hold_b >= 0) {
        if (now < g_scr.hold_until) g_pad.down[g_scr.hold_b] = 1;
        else g_scr.hold_b = -1;
    }
    if (now < g_scr.hold_until && (g_scr.lx || g_scr.ly || g_scr.rx || g_scr.ry)) {
        g_pad.lx = g_scr.lx; g_pad.ly = g_scr.ly; g_pad.rx = g_scr.rx; g_pad.ry = g_scr.ry;
    } else { g_scr.lx = g_scr.ly = g_scr.rx = g_scr.ry = 0; }
    g_pad.connected = 1;
    if (g_scr.npc) {                                       /* anda ate o NPC mais perto e interage */
        int i, best = -1; float bd = 1e9f, lx = 0, ly = 0;
        for (i = 0; i < g_nunits; i++)
            if (g_units[i].type == 1 && (g_units[i].flags & 0x2) && !monster_is_ally(&g_units[i]) &&
                (!g_scr.npc_cls || g_units[i].cls == g_scr.npc_cls)) {
                float d = (g_units[i].x - g_px) * (g_units[i].x - g_px) + (g_units[i].y - g_py) * (g_units[i].y - g_py);
                if (d < bd) { bd = d; best = i; }
            }
        if (best >= 0 && bd > 25.0f && now - g_scr.npc_t0 < 20000) {
            stick_toward(g_units[best].x, g_units[best].y, &lx, &ly);
            g_pad.lx = lx; g_pad.ly = ly;
            return;
        }
        lg("[Script] npc: cls %d a %.1f", best >= 0 ? g_units[best].cls : -1, sqrtf(bd));
        g_scr.npc = 0;
        g_scr.hold_b = B_B; g_scr.hold_until = now + 200; g_scr.until = now + 1500;
        return;
    }
    if (now < g_scr.until) return;
    if (g_scr.i >= g_scr.n) { lg("[Script] fim"); g_scr.on = 0; return; }
    {
        char c[80], a1[40] = "", a2[40] = "", a3[40] = "";
        const char *l = g_scr.lines[g_scr.i++];
        sscanf(l, "%79s %39s %39s %39s", c, a1, a2, a3);
        lg("[Script] > %s", l);
        g_scr.until = now + 50;
        if (!lstrcmpiA(c, "wait")) g_scr.until = now + atoi(a1);
        else if (!lstrcmpiA(c, "pad")) {
            int b = btn_from_name(a1), ms = a2[0] ? atoi(a2) : 150;
            if (b >= 0) { g_scr.hold_b = b; g_scr.hold_until = now + ms; g_scr.until = now + ms + 250; }
        } else if (!lstrcmpiA(c, "stick") || !lstrcmpiA(c, "rstick")) {
            int ms = atoi(a3);
            if (c[0] == 's' || c[0] == 'S') { g_scr.lx = (float)atof(a1); g_scr.ly = (float)atof(a2); }
            else { g_scr.rx = (float)atof(a1); g_scr.ry = (float)atof(a2); }
            g_scr.hold_until = now + ms; g_scr.until = now + ms + 100;
        } else if (!lstrcmpiA(c, "key")) { int vk = key_from_name(a1); if (vk) tap_key(vk, 0); g_scr.until = now + 300; }
        else if (!lstrcmpiA(c, "move")) { move_cursor((float)atof(a1), (float)atof(a2)); g_scr.until = now + 200; }
        else if (!lstrcmpiA(c, "click")) { click(0, 0); g_scr.until = now + 300; }
        else if (!lstrcmpiA(c, "rclick")) { click(1, 0); g_scr.until = now + 300; }
        else if (!lstrcmpiA(c, "npc")) { g_scr.npc = 1; g_scr.npc_cls = atoi(a1); g_scr.npc_t0 = now; }
        else if (!lstrcmpiA(c, "waitpanel")) {
            int id = (int)strtol(a1, NULL, 16); DWORD t = atoi(a2);
            if (!ui_open(id) && t > 100) { snprintf(g_scr.lines[--g_scr.i], 80, "waitpanel %s %lu", a1, t - 100); g_scr.until = now + 100; }
        } else if (!lstrcmpiA(c, "mark")) { lg("[Script] MARCA %s", l + 5); script_log_state("estado"); g_scr.until = now + 1200; }
        else if (!lstrcmpiA(c, "log")) script_log_state("estado");
    }
}

static void tick(float dt)
{
    int b, ctx, mode, editing;
    HWND fg;
    if (!g_hwnd || !IsWindow(g_hwnd)) {
        g_hwnd = find_game_window();
        g_cal_ok = 0;
        if (!g_hwnd) return;
        d2nx_install_hook();
    }
    poll_pad();
    if (g_scr.on && g_player) script_tick();
    {   /* Caixa de chat aberta sem pedido. Um Enter que ficou na fila do jogo durante o carregamento
         * (o + / X que entrou no jogo) abria o chat segundos depois de entrar, e com o chat aberto o heroi
         * nao anda: era o "anda dois passos e congela" (reproduzido no Ryujinx: chat ABERTO com o
         * analogico segurado e o heroi parado; volta quando um ESC fecha o chat). */
        static int chat = -1;
        int c = g_player ? ui_open(UI_CHAT) : 0;
        if (c != chat) {
            if (c && chat == 0 && GetTickCount() - g_enter_t > 1500) {
                lg("chat do jogo aberto sem pedido (Enter atrasado): fechando com ESC");
                tap_key(VK_ESCAPE, 0);
            } else if (chat >= 0 || c) lg("chat do jogo %s", c ? "aberto" : "fechado");
            chat = c;
        }
    }
    fg = GetForegroundWindow();
    if (fg && !cfg.autojoin && !cfg.autowalk && !cfg.autocreate && !cfg.itemtest && !cfg.readtest && !cfg.walktest && !g_scr.on) {
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
    if (!g_pad.connected && !cfg.autojoin && !cfg.autowalk && !cfg.autocreate && !cfg.itemtest && !cfg.readtest && !cfg.walktest && !g_scr.on) { if (g_mode != M_IDLE) { release_all(); g_mode = M_IDLE; } return; }
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
    if (mode != g_mode && g_player) {
        static const int ids[] = { UI_INVENTORY, UI_CHARACTER, UI_SKILLTREE, UI_NPCMENU, UI_GAMEMENU, UI_CONFIG,
                                   UI_NPCSHOP, UI_ANVIL, UI_QUEST, UI_WAYPOINT, UI_PARTY, UI_TRADE, UI_STASH,
                                   UI_CUBE, UI_HELP, UI_MERC, UI_MSGLOG, UI_SCROLL };
        char b[96] = "";
        unsigned i;
        for (i = 0; i < sizeof(ids) / sizeof(ids[0]); i++)
            if (ui_open(ids[i])) snprintf(b + strlen(b), sizeof(b) - strlen(b), " 0x%02X", ids[i]);
        lg("modo %d -> %d (paineis abertos:%s)", g_mode, mode, b[0] ? b : " nenhum");
    }
    set_cursor_hidden(mode == M_GAME);
    if (cfg.readtest && g_player) {
        /* AutoTest ReadTest=1: le DATA\GLOBAL\SFX\taunt\holy_ding.wav pela Storm do jogo (como o som do item de
         * ouro) e registra o CRC32 (correto: 20ec3821, extraido com a StormLib no Linux) */
        static DWORD t_in; static int done;
        if (!t_in) t_in = GetTickCount();
        if (!done && GetTickCount() - t_in > 8000) {
            typedef BOOL (WINAPI *SOpen_t)(const char *, HANDLE *);
            typedef DWORD (WINAPI *SSize_t)(HANDLE, DWORD *);
            typedef BOOL (WINAPI *SRead_t)(HANDLE, void *, DWORD, DWORD *, void *);
            typedef BOOL (WINAPI *SClose_t)(HANDLE);
            HMODULE st = GetModuleHandleA("Storm.dll");
            SOpen_t so = (SOpen_t)(void *)GetProcAddress(st, (LPCSTR)267);
            SSize_t ss = (SSize_t)(void *)GetProcAddress(st, (LPCSTR)265);
            SRead_t sr = (SRead_t)(void *)GetProcAddress(st, (LPCSTR)269);
            SClose_t sc = (SClose_t)(void *)GetProcAddress(st, (LPCSTR)253);
            HANDLE fh = 0;
            done = 1;
            lg("[ReadTest] lendo holy_ding.wav pela Storm do jogo");
            if (so && so("DATA\\GLOBAL\\SFX\\taunt\\holy_ding.wav", &fh)) {
                DWORD size = ss(fh, NULL), got = 0, i, c = 0xFFFFFFFFu;
                BYTE *buf = malloc(size + 16);
                BOOL r = sr(fh, buf, size, &got, NULL);
                int k;
                for (i = 0; i < got; i++) { c ^= buf[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & -(c & 1)); }
                lg("[ReadTest] tamanho %lu lidos %lu ok %d crc %08lx (correto 20ec3821)", size, got, r, (unsigned long)~c);
                sc(fh);
                free(buf);
            } else lg("[ReadTest] SFileOpenFile falhou (%lu)", GetLastError());
        }
    }
    if (cfg.itemtest && g_player) {
        /* AutoTest ItemTest=1: abre o inventario aos 10 s. ItemTest=2: tambem aponta (ItemX,ItemY) e usa o
         * item com o botao direito aos 14 s, como o Y no inventario (painel: o cursor e a interface). */
        static DWORD t_in; static int step;
        DWORD now = GetTickCount();
        if (!t_in) t_in = now;
        if (step == 0 && now - t_in > (DWORD)cfg.item_delay) { lg("[ItemTest] abrindo inventario"); tap_key('I', 0); step = 1; }
        else if (step == 1 && now - t_in > (DWORD)cfg.item_delay + 4000 && cfg.itemtest == 2) {
            lg("[ItemTest] botao direito no item em %d,%d", cfg.item_x, cfg.item_y);
            move_cursor((float)cfg.item_x, (float)cfg.item_y);
            Sleep(300);
            click(1, 0);
            step = 2;
        }
    }
    if (cfg.cursortest && g_player) {   /* AutoTest CursorTest=1: abre o inventario aos 6 s e fecha aos 14 s */
        static DWORD t_in; static int step;
        DWORD now = GetTickCount();
        if (!t_in) t_in = now;
        if (step == 0 && now - t_in > 6000) { lg("[CursorTest] abrindo inventario (I)"); tap_key('I', 0); step = 1; }
        else if (step == 1 && now - t_in > 14000) { lg("[CursorTest] fechando inventario (I)"); tap_key('I', 0); step = 2; }
    }
    if (mode != g_mode) {
        release_all();
        /* cada partida comeca com a preferencia do INI; L3 (tecla R) alterna depois */
        if (mode == M_GAME && g_mode == M_MENU) g_running_state = cfg.always_run;
        if (mode == M_GAME) park_cursor();
        /* como no D2R: ao abrir o inventario o cursor vai para a primeira celula */
        if (mode == M_PANEL && g_mode == M_GAME) {
            if (!ui_open(UI_NPCSHOP) && (ui_open(UI_INVENTORY) || ui_open(UI_STASH) || ui_open(UI_CUBE) || ui_open(UI_TRADE)))
                move_cursor(g_resx / 2 + 17 + 14.5f, g_resy / 2 + 14 + 14.5f);
            else
                move_cursor(g_resx / 2.0f, g_resy / 2.0f);
        }
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
    if (cfg.autocreate) autocreate_tick(mode);
    if (mode == M_GAME && cfg.autojoin) {
        lg("[AutoTest] SUCESSO: Personagem entrou no jogo! player %08lX pos %.1f,%.1f lvl %d",
           g_player, g_px, g_py, g_level);
        cfg.autojoin = 0;
    }

    if ((mode == M_GAME || (mode == M_PANEL && cfg.autowalk == 2)) && cfg.autowalk) {
        static DWORD s_walk_t0;
        DWORD now = GetTickCount();
        if (!s_walk_t0) s_walk_t0 = now;
        if (cfg.autowalk == 1) {
            if (now - s_walk_t0 > 1500 && now - s_walk_t0 < 10000) {
                g_pad.connected = 1;
                g_pad.lx = 0.85f;
                g_pad.ly = 0.30f;
            }
        } else if (cfg.autowalk == 2) {
            autowalk_acceptance(now - s_walk_t0);
        } else if (now - s_walk_t0 > 1500 && now - s_walk_t0 < 1500 + (DWORD)cfg.autowalk * 1000) {
            /* AutoWalk=N: anda N segundos sem parar, girando a direcao a cada 3 s (teste de congelamento) */
            static const float dirs[4][2] = { { 0.85f, 0.30f }, { 0.30f, 0.85f }, { -0.85f, -0.30f }, { -0.30f, -0.85f } };
            static DWORD s_last_log;
            int k = (int)(((now - s_walk_t0) / 3000) % 4);
            g_pad.connected = 1;
            g_pad.lx = dirs[k][0];
            g_pad.ly = dirs[k][1];
            if (now - s_last_log >= 1000) {
                s_last_log = now;
                lg("[AutoWalk] t=%lus pos %.1f,%.1f mouse %ld,%ld modo %lu", (now - s_walk_t0) / 1000, g_px, g_py,
                   g_ver ? (long)rd32(g_client + g_ver->mouse_x) : -1, g_ver ? (long)rd32(g_client + g_ver->mouse_y) : -1,
                   g_player ? rd32(g_player + U_MODE) : 0);
            }
        }
    }


    if (cfg.walktest && mode == M_GAME) {
        /* AutoTest WalkTest=1: 16 direcoes (22,5 graus), 2 s indo e 2 s voltando em cada; registra cada quadro
         * (posicao, modo, quadro da animacao, direcao e alvo do caminho) para medir passos/tremidas. */
        static DWORD t0; static int seg = -1;
        DWORD now = GetTickCount(), t;
        if (!t0) t0 = now;
        t = now - t0;
        if (t >= 8000) {
            int k = (int)((t - 8000) / 4500), part = (int)((t - 8000) % 4500);
            if (k < 16) {
                float a = k * 3.14159265f / 8.0f, sgn = part < 2000 ? 1.0f : part < 4000 ? -1.0f : 0.0f;
                float nx = 0, ny = 0;
                if (cfg.walk_noise > 0) { nx = ((rand() % 2001) - 1000) / 1000.0f * cfg.walk_noise; ny = ((rand() % 2001) - 1000) / 1000.0f * cfg.walk_noise; }
                if (k * 3 + (part < 2000 ? 0 : part < 4000 ? 1 : 2) != seg) {
                    seg = k * 3 + (part < 2000 ? 0 : part < 4000 ? 1 : 2);
                    lg("[WalkTest] segmento %d angulo %d %s", k, k * 225 / 10, part < 2000 ? "ida" : part < 4000 ? "volta" : "parado");
                }
                InterlockedExchange(&g_walk_log, 1);
                g_pad.connected = 1;
                g_pad.lx = sgn ? sgn * cosf(a) + nx : 0; g_pad.ly = sgn ? sgn * sinf(a) + ny : 0;
            } else if (g_walk_log) { InterlockedExchange(&g_walk_log, 0); lg("[WalkTest] fim"); }
        }
    }
    if (cfg.menutest && mode == M_GAME) {
        /* AutoTest MenuTest=1: pelos botoes, como o jogador: ZL+- abre, baixo, direita, A aplica, B fecha */
        static DWORD t_in;
        static const struct { DWORD t0, t1; int b; const char *what; } seq[] = {
            { 10000, 10900, B_ZL, "ZL" }, { 10300, 10600, B_MINUS, "- (com ZL): abre" },
            { 13000, 13200, B_DOWN, "baixo" }, { 14500, 14700, B_RIGHT, "direita" },
            { 16500, 16700, B_A, "A: aplica" }, { 19500, 19700, B_B, "B: fecha" },
        };
        DWORD now = GetTickCount(), t;
        unsigned i;
        if (!t_in) t_in = now;
        t = now - t_in;
        static int said[8];
        for (i = 0; i < sizeof(seq) / sizeof(seq[0]); i++) {
            int on = t >= seq[i].t0 && t < seq[i].t1;
            if (on && !said[i]) { said[i] = 1; lg("[MenuTest] botao %s", seq[i].what); }
            if (on) g_pad.down[seq[i].b] = 1;
        }
        g_pad.connected = 1;
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
            d2_render_gameplay_menu();
        } else if (PRESSED(B_RIGHT)) {
            if (g_gp_menu.cur_item == 0 && g_gp_menu.xp_idx < (int)XP_COUNT - 1) g_gp_menu.xp_idx++;
            else if (g_gp_menu.cur_item == 1 && g_gp_menu.rune_idx < (int)RUNE_COUNT - 1) g_gp_menu.rune_idx++;
            else if (g_gp_menu.cur_item == 2 && g_gp_menu.unique_idx < (int)UNIQUE_COUNT - 1) g_gp_menu.unique_idx++;
            else if (g_gp_menu.cur_item == 3 && g_gp_menu.rare_idx < (int)RARE_COUNT - 1) g_gp_menu.rare_idx++;
            d2_render_gameplay_menu();
        } else if (PRESSED(B_A)) {
            apply_and_save_gameplay_menu();
            menu_status(L"Salvo. Vale a partir do proximo boot do jogo.");
        } else if (PRESSED(B_B)) {
            g_gp_menu.open = 0;
            lg("[GameplayMenu] Menu fechado (B).");
        } else if (GetTickCount() - g_gp_menu.last_refresh > 4000) {
            d2_render_gameplay_menu();
        }
        return;
    }

    /* direcao em que o heroi olha */
    if (sqrtf(g_pad.rx * g_pad.rx + g_pad.ry * g_pad.ry) > 0.5f) {
        g_face_x = g_pad.rx; g_face_y = g_pad.ry; g_face_valid = 1;
    } else if (sqrtf(g_pad.lx * g_pad.lx + g_pad.ly * g_pad.ly) > cfg.dz_left) {
        g_face_x = g_pad.lx; g_face_y = g_pad.ly; g_face_valid = 1;
    }

    /* contexto de cada botao */
    g_layer = g_pad.down[B_ZL];
    if (mode == M_GAME) ctx = g_pad.down[B_PLUS] ? C_PLUS : g_layer ? C_LAYER : C_GAME;
    else if (mode == M_PANEL) ctx = C_PANEL;
    else ctx = C_MENU;

    if (ctx != C_MENU) ned_end("fora do menu");
    /* com o editor de nome aberto os botoes sao dele (os analogicos seguem movendo o cursor) */
    editing = ctx == C_MENU && name_editor_tick();
    for (b = 0; b < B_COUNT && !editing; b++) {
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

    if (mode == M_GAME) {
        if (g_pend_unit && sqrtf(g_pad.lx * g_pad.lx + g_pad.ly * g_pad.ly) > cfg.dz_left) g_pend_unit = 0;  /* mexeu: cancela */
        pending_interact_tick();
    }
    /* movimento: analogico esquerdo -> direcao no mundo -> rotina de comando do jogo (camada nativa).
     * Nenhum mouse/cursor/clique. Zona morta configuravel; a inclinacao e limitada a 1 (diagonal do
     * quadrado nao passa da velocidade das direcoes retas); inclinacao leve anda, forte corre. */
    if (mode == M_GAME) {
        float lx = g_pad.lx, ly = g_pad.ly, m = sqrtf(lx * lx + ly * ly);
        int busy = 0;
        for (b = 0; b < B_COUNT; b++)
            if (g_hold[b].active && (g_hold[b].act.kind == A_ATTACK || g_hold[b].act.kind == A_SKILL ||
                                     g_hold[b].act.kind == A_RIGHT)) busy = 1;
        InterlockedExchange(&g_mv_busy, busy);
        if (m > cfg.dz_left) {
            float k = m > 1.0f ? 1.0f : m, inten = (k - cfg.dz_left) / (1.0f - cfg.dz_left);
            float sx = lx / m, sy = ly / m;                        /* direcao na tela (y para baixo) */
            float wx, wy, wl, sl;
            /* suaviza a direcao (o analogico de verdade nunca fica parado; sem isso a direcao pulava entre
             * duas das 64 direcoes do jogo); uma virada grande passa direto */
            if (g_sm_on && g_sm_x * sx + g_sm_y * sy > 0.7f) { g_sm_x = g_sm_x * 0.75f + sx * 0.25f; g_sm_y = g_sm_y * 0.75f + sy * 0.25f; }
            else { g_sm_x = sx; g_sm_y = sy; g_sm_on = 1; }
            sl = sqrtf(g_sm_x * g_sm_x + g_sm_y * g_sm_y);
            if (sl > 1e-4f) { sx = g_sm_x / sl; sy = g_sm_y / sl; }
            wx = sx / 16.0f + sy / 8.0f; wy = sy / 8.0f - sx / 16.0f; wl = sqrtf(wx * wx + wy * wy);
            g_mv_wx = wx / wl; g_mv_wy = wy / wl;                  /* tela isometrica -> mundo */
            /* andar/correr com histerese: perto do limite a passada nao fica alternando */
            InterlockedExchange(&g_mv_run, g_mv_run ? inten >= cfg.run_at - 0.10f : inten >= cfg.run_at);
            InterlockedExchange(&g_mv_active, 1);
        } else {
            InterlockedExchange(&g_mv_active, 0);
            g_sm_on = 0;
        }
        /* analogico direito: so a direcao de mira (ataque/habilidade sem alvo); o cursor fica oculto */
    } else {
        InterlockedExchange(&g_mv_active, 0);
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
        native_init();
        draw_init();
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
    d2nx_remove_hook();
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


/* Diagnostico do "Bad Character File": lista os saves do Median XL (%APPDATA%\MedianXL\save)
 * com tamanho e se o cabecalho do .d2s confere (assinatura, tamanho e checksum). */
static void log_saves(void)
{
    char dir[MAX_PATH], pat[MAX_PATH], path[MAX_PATH];
    WIN32_FIND_DATAA fd;
    HANDLE f;
    int n = 0;
    if (!GetEnvironmentVariableA("APPDATA", dir, sizeof(dir))) return;
    lstrcatA(dir, "\\MedianXL\\save\\");
    snprintf(pat, sizeof(pat), "%s*.d2s", dir);
    f = FindFirstFileA(pat, &fd);
    if (f == INVALID_HANDLE_VALUE) { lg("saves: nenhum .d2s em %s", dir); return; }
    do {
        unsigned char *b;
        DWORD size = fd.nFileSizeLow, got = 0, sum = 0, i;
        HANDLE h;
        const char *st = "ilegivel";
        snprintf(path, sizeof(path), "%s%s", dir, fd.cFileName);
        h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE && size >= 16 && size < (1u << 20) && (b = malloc(size))) {
            if (ReadFile(h, b, size, &got, NULL) && got == size) {
                DWORD magic = *(DWORD *)b, fsize = *(DWORD *)(b + 8), chk = *(DWORD *)(b + 12);
                b[12] = b[13] = b[14] = b[15] = 0;
                for (i = 0; i < size; i++) sum = ((sum << 1) | (sum >> 31)) + b[i];
                st = magic != 0xAA55AA55 ? "assinatura invalida" : fsize != size ? "tamanho no cabecalho difere"
                   : sum != chk ? "checksum invalido" : size < 400 ? "ok, mas so cabecalho (nunca entrou no jogo)" : "ok";
            }
            free(b);
        }
        if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
        lg("save %s: %lu bytes, %s", fd.cFileName, size, st);
        n++;
    } while (FindNextFileA(f, &fd));
    FindClose(f);
    lg("saves: %d personagem(ns) em %s", n, dir);
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
    log_saves();
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
