/* D2NXPad - controle estilo Diablo II Resurrected (Switch) para Diablo II LoD 1.07-1.13c.
 * Copyright 2026. Licenca MIT. Carregado dentro do processo do jogo (D2SE.exe / Game.exe). */
#ifndef D2NXPAD_H
#define D2NXPAD_H
#include <windows.h>

/* ---- versoes ---- */
typedef struct D2Ver {
    const char *name;
    DWORD client_ts;       /* TimeDateStamp do D2Client.dll */
    DWORD client_size;     /* SizeOfImage do D2Client.dll */
    /* RVAs no D2Client */
    DWORD res_x, res_y;    /* resolucao do jogo (DWORD) */
    DWORD mouse_x, mouse_y;/* variaveis do mouse (>=1.11) */
    DWORD f_mouse_x, f_mouse_y; /* funcoes stdcall do mouse (<=1.10) */
    DWORD player;          /* variavel Unit* do jogador (>=1.11) */
    DWORD f_player;        /* funcao stdcall Unit* (<=1.10) */
    DWORD ui_vars;         /* array DWORD com estado dos paineis (0 = desconhecido) */
    int room_fmt;          /* 0: layout 1.09/1.10, 1: layout 1.11+ */
    DWORD print_msg;       /* RVA PrintGameString (stdcall wchar_t*, int color) */
} D2Ver;

const D2Ver *d2ver_detect(HMODULE client);
extern D2Ver g_ver_override; /* preenchido pelo INI [Offsets] */

/* ---- painel / UI (indices 1.10+; iguais em 1.11-1.13) ---- */
enum {
    UI_INVENTORY = 0x01, UI_CHARACTER = 0x02, UI_MINISKILL = 0x03, UI_SKILLTREE = 0x04,
    UI_CHAT = 0x05, UI_NPCMENU = 0x08, UI_GAMEMENU = 0x09, UI_AUTOMAP = 0x0A, UI_CONFIG = 0x0B,
    UI_NPCSHOP = 0x0C, UI_HOLDALT = 0x0D, UI_ANVIL = 0x0E, UI_QUEST = 0x0F, UI_QUESTLOG = 0x11,
    UI_WAYPOINT = 0x14, UI_MINIPANEL = 0x15, UI_PARTY = 0x16, UI_TRADE = 0x17, UI_MSGLOG = 0x18,
    UI_STASH = 0x19, UI_CUBE = 0x1A, UI_HELP = 0x21, UI_MERC = 0x24, UI_SCROLL = 0x25, UI_MAX = 0x28
};
#endif
