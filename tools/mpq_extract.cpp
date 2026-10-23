// Extrai arquivos de um MPQ (usado para tirar do Median XL as tabelas originais que o menu de gameplay
// do D2NXPad e o tools/patch_gameplay.py modificam: experience_sp.bin, itemratio_sp.bin, treasureclassex.bin).
// Uso: mpq_extract <arquivo.mpq> <nome no MPQ> <saida> [<nome> <saida> ...]
#include "StormLib.h"
#include <cstdio>

int main(int argc, char **argv)
{
    if (argc < 4 || (argc - 2) % 2) { fprintf(stderr, "uso: %s <mpq> <nome> <saida> [<nome> <saida> ...]\n", argv[0]); return 2; }
    HANDLE h;
    if (!SFileOpenArchive(argv[1], 0, STREAM_FLAG_READ_ONLY, &h)) { fprintf(stderr, "nao abriu %s (%d)\n", argv[1], (int)SErrGetLastError()); return 1; }
    int bad = 0;
    for (int i = 2; i + 1 < argc; i += 2) {
        if (SFileExtractFile(h, argv[i], argv[i + 1], 0)) printf("%s -> %s\n", argv[i], argv[i + 1]);
        else { fprintf(stderr, "nao extraiu %s (%d)\n", argv[i], (int)SErrGetLastError()); bad = 1; }
    }
    SFileCloseArchive(h);
    return bad;
}
