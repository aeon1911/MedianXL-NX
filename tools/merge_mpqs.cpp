// Junta os MPQs do Median XL (medianxl-*.mpq) em um so, para o boot no cartao do Switch abrir um arquivo
// em vez de dezenas. COPIA OS BLOCOS COMPRIMIDOS COMO ESTAO: nada e descompactado nem recompactado.
//
// A versao anterior relia cada arquivo e o regravava com zlib. A Storm do Diablo II 1.13c nao descomprime
// zlib, e os 389 sons do Median XL (ADPCM/Huffman da Blizzard) viravam zlib: ao tocar um deles o jogo
// fechava com "The file data is corrupt" (medianxl-data.mpq), por exemplo ao usar o item de 1.500 de ouro.
//
// Os dados de cada arquivo nao sao criptografados (so o (listfile)/(attributes) de cada MPQ, que ficam de
// fora) e a tabela de setores e relativa ao inicio do arquivo, entao o bloco pode ser copiado byte a byte.
// O MPQ de saida e formato 1 (o do D2 1.13c): cabecalho de 32 bytes, dados, tabela de hash e de blocos
// criptografadas com as chaves padrao, e um (listfile) sem compressao.
//
// Uso: merge_mpqs <pasta com medianxl-*.mpq> <saida.mpq>
// Arquivos repetidos entre MPQs: vale o do ultimo MPQ em ordem alfabetica (como a versao anterior).
#include "StormLib.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static uint32_t crypt_table[0x500];
static void init_crypt()
{
    uint32_t seed = 0x00100001;
    for (int i = 0; i < 0x100; i++)
        for (int j = 0, idx = i; j < 5; j++, idx += 0x100) {
            seed = (seed * 125 + 3) % 0x2AAAAB; uint32_t t1 = (seed & 0xFFFF) << 16;
            seed = (seed * 125 + 3) % 0x2AAAAB; uint32_t t2 = seed & 0xFFFF;
            crypt_table[idx] = t1 | t2;
        }
}
static uint32_t hash_str(const std::string &s, int type)
{
    uint32_t s1 = 0x7FED7FED, s2 = 0xEEEEEEEE;
    for (unsigned char c : s) {
        c = (unsigned char)toupper(c == '/' ? '\\' : c);
        s1 = crypt_table[type * 0x100 + c] ^ (s1 + s2);
        s2 = c + s1 + s2 + (s2 << 5) + 3;
    }
    return s1;
}
static void encrypt(uint32_t *d, size_t n, uint32_t key)
{
    uint32_t s2 = 0xEEEEEEEE;
    for (size_t i = 0; i < n; i++) {
        s2 += crypt_table[0x400 + (key & 0xFF)];
        uint32_t plain = d[i];
        d[i] = plain ^ (key + s2);
        key = ((~key << 0x15) + 0x11111111) | (key >> 0x0B);
        s2 = plain + s2 + (s2 << 5) + 3;
    }
}

struct Entry { std::string name, src; uint64_t off; uint32_t csize, size, flags; };

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "uso: %s <pasta medianxl-*.mpq> <saida.mpq>\n", argv[0]); return 2; }
    init_crypt();
    std::vector<std::string> mpqs;
    for (auto &e : fs::directory_iterator(argv[1])) {
        std::string n = e.path().filename().string(), low = n;
        std::transform(low.begin(), low.end(), low.begin(), ::tolower);
        if (low.rfind("medianxl-", 0) == 0 && low.size() > 4 && low.substr(low.size() - 4) == ".mpq" &&
            low != "medianxl-data.mpq" && fs::absolute(e.path()) != fs::absolute(argv[2]))
            mpqs.push_back(e.path().string());
    }
    std::sort(mpqs.begin(), mpqs.end());
    if (mpqs.empty()) { fprintf(stderr, "nenhum medianxl-*.mpq em %s\n", argv[1]); return 1; }

    std::map<std::string, Entry> files;   // nome em maiusculas -> ultimo MPQ que o tem
    for (auto &p : mpqs) {
        HANDLE h;
        if (!SFileOpenArchive(p.c_str(), 0, STREAM_FLAG_READ_ONLY, &h)) { fprintf(stderr, "nao abriu %s\n", p.c_str()); return 1; }
        SFILE_FIND_DATA fd;
        HANDLE f = SFileFindFirstFile(h, "*", &fd, NULL);
        if (!f) { fprintf(stderr, "%s sem (listfile)\n", p.c_str()); return 1; }
        do {
            std::string n = fd.cFileName;
            if (n == "(listfile)" || n == "(attributes)" || n == "(signature)") continue;
            HANDLE fh; uint64_t off = 0; uint32_t csize = 0, flags = 0;
            if (!SFileOpenFileEx(h, n.c_str(), SFILE_OPEN_FROM_MPQ, &fh)) { fprintf(stderr, "nao abriu %s em %s\n", n.c_str(), p.c_str()); return 1; }
            SFileGetFileInfo(fh, SFileInfoByteOffset, &off, sizeof(off), NULL);
            SFileGetFileInfo(fh, SFileInfoCompressedSize, &csize, sizeof(csize), NULL);
            SFileGetFileInfo(fh, SFileInfoFlags, &flags, sizeof(flags), NULL);
            SFileCloseFile(fh);
            if (flags & (MPQ_FILE_ENCRYPTED | MPQ_FILE_SECTOR_CRC | MPQ_FILE_PATCH_FILE | MPQ_FILE_DELETE_MARKER)) {
                fprintf(stderr, "%s em %s tem flags %08x que esta copia nao trata\n", n.c_str(), p.c_str(), flags);
                return 1;
            }
            std::string key = n;
            std::transform(key.begin(), key.end(), key.begin(), ::toupper);
            files[key] = Entry{ n, p, off, csize, fd.dwFileSize, flags };
        } while (SFileFindNextFile(f, &fd));
        SFileFindClose(f);
        SFileCloseArchive(h);
    }

    // (listfile) do MPQ novo, sem compressao nem criptografia
    std::string listfile;
    for (auto &kv : files) listfile += kv.second.name + "\r\n";

    uint32_t nfiles = (uint32_t)files.size() + 1, hsize = 16;
    while (hsize < nfiles * 2) hsize <<= 1;
    FILE *out = fopen(argv[2], "wb");
    if (!out) { fprintf(stderr, "nao criou %s\n", argv[2]); return 1; }
    char zero[32] = {0};
    fwrite(zero, 1, 32, out);                              // cabecalho, reescrito no fim

    std::vector<uint32_t> blocks;                          // offset, csize, size, flags
    std::vector<uint32_t> hashes(hsize * 4, 0xFFFFFFFF);
    std::map<std::string, FILE *> srcs;
    std::vector<char> buf;
    auto add_hash = [&](const std::string &name, uint32_t block) {
        uint32_t i = hash_str(name, 0) & (hsize - 1);
        while (hashes[i * 4 + 3] != 0xFFFFFFFF) i = (i + 1) & (hsize - 1);
        hashes[i * 4 + 0] = hash_str(name, 1);
        hashes[i * 4 + 1] = hash_str(name, 2);
        hashes[i * 4 + 2] = 0;                             // locale neutro, plataforma 0
        hashes[i * 4 + 3] = block;
    };
    for (auto &kv : files) {
        Entry &e = kv.second;
        FILE *&s = srcs[e.src];
        if (!s && !(s = fopen(e.src.c_str(), "rb"))) { fprintf(stderr, "nao leu %s\n", e.src.c_str()); return 1; }
        buf.resize(e.csize);
        if (fseeko(s, (off_t)e.off, SEEK_SET) || fread(buf.data(), 1, e.csize, s) != e.csize) {
            fprintf(stderr, "leitura curta de %s em %s\n", e.name.c_str(), e.src.c_str()); return 1;
        }
        uint32_t pos = (uint32_t)ftello(out);
        fwrite(buf.data(), 1, e.csize, out);
        add_hash(e.name, (uint32_t)(blocks.size() / 4));
        blocks.insert(blocks.end(), { pos, e.csize, e.size, e.flags });
    }
    uint32_t lpos = (uint32_t)ftello(out);
    fwrite(listfile.data(), 1, listfile.size(), out);
    add_hash("(listfile)", (uint32_t)(blocks.size() / 4));
    blocks.insert(blocks.end(), { lpos, (uint32_t)listfile.size(), (uint32_t)listfile.size(), MPQ_FILE_EXISTS });

    uint32_t hpos = (uint32_t)ftello(out);
    encrypt(hashes.data(), hashes.size(), hash_str("(hash table)", 3));
    fwrite(hashes.data(), 4, hashes.size(), out);
    uint32_t bpos = (uint32_t)ftello(out);
    uint32_t nblocks = (uint32_t)(blocks.size() / 4);
    encrypt(blocks.data(), blocks.size(), hash_str("(block table)", 3));
    fwrite(blocks.data(), 4, blocks.size(), out);
    uint32_t total = (uint32_t)ftello(out);

    uint32_t hdr[8];
    memcpy(&hdr[0], "MPQ\x1a", 4);
    hdr[1] = 32; hdr[2] = total;
    hdr[3] = 0 | (3u << 16);                               // formato 1, setor de 4096 bytes (como os originais)
    hdr[4] = hpos; hdr[5] = bpos; hdr[6] = hsize; hdr[7] = nblocks;
    fseeko(out, 0, SEEK_SET);
    fwrite(hdr, 4, 8, out);
    fclose(out);
    for (auto &kv : srcs) fclose(kv.second);
    printf("%zu MPQs, %zu arquivos copiados sem recompressao, %u bytes -> %s\n", mpqs.size(), files.size(), total, argv[2]);
    return 0;
}
