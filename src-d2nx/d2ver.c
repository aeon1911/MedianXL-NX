/* Tabela de versoes do D2Client. Fontes: PlugY (D2Funcs.h, colunas 1.09b..1.13c),
 * slashdiablo-maphack (1.13c) e analise propria dos binarios dos cores do D2SE.
 * Qualquer valor pode ser sobrescrito no d2nxpad.ini, secao [Offsets]. */
#include "d2nxpad.h"

static const D2Ver versions[] = {
 /* name     ts          size      resX     resY     mouseX   mouseY   fMX      fMY      player   fPlayer  ui       fmt  printMsg */
 {"1.07",  0x3b007197, 0x160000, 0,       0,       0,       0,       0,       0,       0,       0,       0,       0,   0},
 {"1.08",  0x3b2eb437, 0x154000, 0,       0,       0,       0,       0,       0,       0,       0,       0,       0,   0},
 {"1.09b", 0x3b7c5076, 0x157000, 0xD50EC, 0xD50E8, 0,       0,       0xB6670, 0xB6680, 0,       0x8DC40, 0,       0,   0},
 {"1.09d", 0x3c0700b0, 0x156000, 0xD40E4, 0xD40E0, 0x12B168,0x12B16C,0xB59F0, 0xB5A00, 0,       0x8CFC0, 0x1248B4,0,   0},
 {"1.10f", 0x3f7cb8be, 0x133000, 0xD40F4, 0xD40F0, 0x121AE4,0x121AE8,     0xB7BC0, 0xB7BD0, 0,       0x883D0, 0x11A6A8,0, 0x71740},
 {"1.11b", 0x43028ca5, 0x135000, 0xF4FC4, 0xF4FC8, 0x11B418,0x11B414,0,       0,       0x11C1E0,0,       0x1040C0,1, 0x71740},
 {"1.12a", 0x483cb8df, 0x135000, 0xDC6E0, 0xDC6E4, 0x101638,0x101634,0,       0,       0x11C3D0,0,       0x102B58,1, 0x7D850},
 {"1.13c", 0x4b95ca3e, 0x135000, 0xDBC48, 0xDBC4C, 0x11B828,0x11B824,0,       0,       0x11BBFC,0,       0xFAD80, 1, 0x7D850},
};

D2Ver g_ver_override;

const D2Ver *d2ver_detect(HMODULE client)
{
    IMAGE_DOS_HEADER *dos = (IMAGE_DOS_HEADER *)client;
    IMAGE_NT_HEADERS32 *nt;
    unsigned i;
    if (!client || dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
    nt = (IMAGE_NT_HEADERS32 *)((BYTE *)client + dos->e_lfanew);
    for (i = 0; i < sizeof(versions) / sizeof(versions[0]); i++)
        if (versions[i].client_ts == nt->FileHeader.TimeDateStamp) return &versions[i];
    /* D2Client modificado por mod: tenta pelo tamanho da imagem + checksum do timestamp */
    return NULL;
}
