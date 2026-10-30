// Grava dentro de um MPQ os arquivos de uma pasta (sobrescreve o que ja existe com o mesmo caminho), SEM compressao.
//
// Por que existe: o Median XL (D2Sigma.dll) le suas tabelas (experience_sp.bin, itemratio_sp.bin, treasureclassex.bin,
// npcstores.bin...) pela Storm, direto do MPQ. Arquivos soltos em data\global\excel\ so valem para as tabelas que o D2Common
// carrega pelo Fog (-direct), como charstats.bin; as do Median XL ignoram a pasta (medido no Ryujinx: a Storm do jogo devolve
// o CRC do MPQ original, nao o do arquivo solto). Para os multiplicadores de XP/runas/unicos/raros e os vendedores valerem,
// as tabelas gerais pelo tools/patch_gameplay.py precisam estar dentro do medianxl-data.mpq.
//
// Idempotente: o arquivo so e regravado se o conteudo for diferente (o MPQ nao muda, e o mtime tambem nao, se nada mudou).
// A Storm do Diablo II 1.13c nao descomprime zlib: nada aqui e comprimido.
// Uso: mpq_overlay <arquivo.mpq> <pasta-raiz>     (pasta-raiz/data/global/excel/x.bin -> "data\global\excel\x.bin")
#include "StormLib.h"
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static bool read_all(const fs::path &p, std::vector<char> &out)
{
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "uso: %s <arquivo.mpq> <pasta-raiz>\n", argv[0]); return 2; }
    fs::path root = argv[2];
    std::vector<fs::path> files;
    for (auto &e : fs::recursive_directory_iterator(root))
        if (e.is_regular_file()) files.push_back(e.path());
    if (files.empty()) { printf("mpq_overlay: nada em %s\n", argv[2]); return 0; }

    HANDLE h;
    if (!SFileOpenArchive(argv[1], 0, 0, &h)) { fprintf(stderr, "nao abriu %s (%d)\n", argv[1], (int)SErrGetLastError()); return 1; }
    int changed = 0, same = 0, bad = 0;
    for (auto &p : files) {
        std::string name = fs::relative(p, root).generic_string();
        for (auto &c : name) if (c == '/') c = '\\';
        std::vector<char> now, old;
        if (!read_all(p, now)) { fprintf(stderr, "nao leu %s\n", p.c_str()); bad = 1; continue; }
        HANDLE f;
        bool equal = false;
        if (SFileOpenFileEx(h, name.c_str(), 0, &f)) {
            DWORD size = SFileGetFileSize(f, NULL), got = 0;
            old.resize(size);
            if (size == now.size() && SFileReadFile(f, old.data(), size, &got, NULL) && got == size) equal = (old == now);
            SFileCloseFile(f);
        }
        if (equal) { same++; continue; }
        if (SFileAddFileEx(h, p.c_str(), name.c_str(), MPQ_FILE_REPLACEEXISTING, 0, 0)) {
            printf("mpq_overlay: %s (%zu bytes)\n", name.c_str(), now.size());
            changed++;
        } else { fprintf(stderr, "nao gravou %s (%d)\n", name.c_str(), (int)SErrGetLastError()); bad = 1; }
    }
    if (!SFileCloseArchive(h)) { fprintf(stderr, "nao fechou o MPQ (%d)\n", (int)SErrGetLastError()); return 1; }
    printf("mpq_overlay: %d gravados, %d ja iguais%s\n", changed, same, bad ? ", COM ERROS" : "");
    return bad;
}
