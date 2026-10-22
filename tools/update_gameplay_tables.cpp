#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <filesystem>
#include "StormLib.h"

namespace fs = std::filesystem;

struct GameplayConfig {
    float xp_multiplier = 2.0f;
    float rune_multiplier = 2.0f;
    float unique_multiplier = 1.5f;
    float rare_multiplier = 1.5f;
    uint32_t start_gold = 10000;
};

GameplayConfig load_config(const std::string& ini_path) {
    GameplayConfig cfg;
    std::ifstream f(ini_path);
    if (!f.is_open()) return cfg;

    std::string line;
    bool in_gameplay = false;
    while (std::getline(f, line)) {
        // trim whitespace
        size_t first = line.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) continue;
        if (line[first] == ';' || line[first] == '#') continue;

        if (line[first] == '[') {
            size_t last = line.find(']', first);
            if (last != std::string::npos) {
                std::string sec = line.substr(first + 1, last - first - 1);
                in_gameplay = (sec == "Gameplay");
            }
            continue;
        }

        if (!in_gameplay) continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(first, eq - first);
        // trim key
        key.erase(key.find_last_not_of(" \t\r\n") + 1);

        std::string val = line.substr(eq + 1);
        size_t vstart = val.find_first_not_of(" \t\r\n");
        if (vstart != std::string::npos) {
            val = val.substr(vstart);
            size_t vend = val.find_first_of(" \t\r\n;#");
            if (vend != std::string::npos) val = val.substr(0, vend);
        }

        try {
            if (key == "MultiplicadorXP" || key == "XpMultiplier") cfg.xp_multiplier = std::stof(val);
            else if (key == "MultiplicadorRunas" || key == "RuneMultiplier") cfg.rune_multiplier = std::stof(val);
            else if (key == "MultiplicadorUnicos" || key == "UniqueMultiplier") cfg.unique_multiplier = std::stof(val);
            else if (key == "MultiplicadorRaros" || key == "RareMultiplier") cfg.rare_multiplier = std::stof(val);
            else if (key == "OuroInicial" || key == "StartingGold") cfg.start_gold = (uint32_t)std::stoul(val);
        } catch (...) {}
    }
    return cfg;
}

std::vector<char> read_mpq_file(HANDLE hMpq, const std::string& name) {
    HANDLE hFile;
    if (!SFileOpenFileEx(hMpq, name.c_str(), SFILE_OPEN_FROM_MPQ, &hFile)) {
        return {};
    }
    DWORD sz = SFileGetFileSize(hFile, NULL);
    std::vector<char> buf(sz);
    DWORD read = 0;
    SFileReadFile(hFile, buf.data(), sz, &read, NULL);
    SFileCloseFile(hFile);
    return buf;
}

bool write_mpq_file(HANDLE hMpq, const std::string& name, const std::vector<char>& data) {
    HANDLE hDstFile = NULL;
    DWORD writeFlags = MPQ_FILE_REPLACEEXISTING | MPQ_FILE_COMPRESS;
    DWORD comp = MPQ_COMPRESSION_ZLIB;

    if (!SFileCreateFile(hMpq, name.c_str(), 0, (DWORD)data.size(), 0, writeFlags, &hDstFile)) {
        std::cerr << "Error: SFileCreateFile failed for " << name << ": " << SErrGetLastError() << std::endl;
        return false;
    }
    if (!data.empty()) {
        if (!SFileWriteFile(hDstFile, data.data(), (DWORD)data.size(), comp)) {
            std::cerr << "Error: SFileWriteFile failed for " << name << ": " << SErrGetLastError() << std::endl;
            SFileCloseFile(hDstFile);
            return false;
        }
    }
    SFileFinishFile(hDstFile);
    return true;
}

bool patch_archive(const std::string& mpq_path, const GameplayConfig& cfg, const std::string& out_dir = "") {
    if (!fs::exists(mpq_path)) {
        std::cerr << "Archive does not exist: " << mpq_path << std::endl;
        return false;
    }

    HANDLE hMpq;
    if (!SFileOpenArchive(mpq_path.c_str(), 0, 0, &hMpq)) {
        std::cerr << "Failed to open archive " << mpq_path << ": " << SErrGetLastError() << std::endl;
        return false;
    }

    std::cout << ">>> Modifying " << mpq_path << " with Gameplay multipliers..." << std::endl;

    // 1. Experience
    auto exp_data = read_mpq_file(hMpq, "data\\global\\excel\\experience_sp.bin");
    if (!exp_data.empty()) {
        int cnt = (int)(exp_data.size() - 0x20) / 32;
        for (int lvl = 0; lvl < cnt; lvl++) {
            size_t offset = 0x20 + lvl * 32;
            for (int c = 0; c < 7; c++) {
                size_t pos = offset + 4 + c * 4;
                uint32_t orig = 0;
                std::memcpy(&orig, &exp_data[pos], 4);
                uint32_t new_val = (orig > 0) ? std::max(1u, (uint32_t)(orig / cfg.xp_multiplier)) : 0;
                std::memcpy(&exp_data[pos], &new_val, 4);
            }
        }
        write_mpq_file(hMpq, "data\\global\\excel\\experience_sp.bin", exp_data);
        if (!out_dir.empty()) {
            fs::create_directories(out_dir + "/data/global/excel");
            std::ofstream out(out_dir + "/data/global/excel/experience_sp.bin", std::ios::binary);
            out.write(exp_data.data(), exp_data.size());
        }
        std::cout << "  [1/4] experience_sp.bin: " << cnt << " levels scaled by " << cfg.xp_multiplier << "x XP" << std::endl;
    }

    // 2. ItemRatio
    auto ratio_data = read_mpq_file(hMpq, "data\\global\\excel\\itemratio_sp.bin");
    if (!ratio_data.empty()) {
        uint32_t rows = 0;
        std::memcpy(&rows, ratio_data.data(), 4);
        for (uint32_t r = 0; r < rows; r++) {
            size_t offset = 4 + r * 68;
            uint32_t u = 0, r_val = 0, s = 0;
            std::memcpy(&u, &ratio_data[offset + 8], 4);
            std::memcpy(&r_val, &ratio_data[offset + 20], 4);
            std::memcpy(&s, &ratio_data[offset + 32], 4);

            uint32_t new_u = std::max(5u, (uint32_t)(u / cfg.unique_multiplier));
            uint32_t new_r = std::max(5u, (uint32_t)(r_val / cfg.rare_multiplier));
            uint32_t new_s = std::max(6u, (uint32_t)(s / cfg.unique_multiplier));

            std::memcpy(&ratio_data[offset + 8], &new_u, 4);
            std::memcpy(&ratio_data[offset + 20], &new_r, 4);
            std::memcpy(&ratio_data[offset + 32], &new_s, 4);
        }
        write_mpq_file(hMpq, "data\\global\\excel\\itemratio_sp.bin", ratio_data);
        if (!out_dir.empty()) {
            std::ofstream out(out_dir + "/data/global/excel/itemratio_sp.bin", std::ios::binary);
            out.write(ratio_data.data(), ratio_data.size());
        }
        std::cout << "  [2/4] itemratio_sp.bin: Uniques " << cfg.unique_multiplier << "x, Rares " << cfg.rare_multiplier << "x" << std::endl;
    }

    // 3. TreasureClassEx
    auto tc_data = read_mpq_file(hMpq, "data\\global\\excel\\treasureclassex.bin");
    if (!tc_data.empty()) {
        uint32_t tc_rows = 0;
        std::memcpy(&tc_rows, tc_data.data(), 4);
        int rune_mods = 0;
        size_t row_size = 736;
        for (uint32_t r = 0; r < tc_rows; r++) {
            size_t offset = 4 + r * row_size;
            std::string tc_name = (const char*)&tc_data[offset];
            for (int item_idx = 0; item_idx < 10; item_idx++) {
                size_t item_off = offset + 0x38 + item_idx * 64;
                std::string item_name = (const char*)&tc_data[item_off];
                std::string iname_lower = item_name;
                std::transform(iname_lower.begin(), iname_lower.end(), iname_lower.begin(), ::tolower);

                size_t prob_pos = offset + 0x2b8 + item_idx * 4;
                uint32_t prob = 0;
                std::memcpy(&prob, &tc_data[prob_pos], 4);

                if (iname_lower.find("rune") != std::string::npos && tc_name.rfind("Runes ", 0) != 0) {
                    uint32_t new_prob = std::min(65535u, (uint32_t)(prob * cfg.rune_multiplier));
                    std::memcpy(&tc_data[prob_pos], &new_prob, 4);
                    rune_mods++;
                }
            }
        }
        write_mpq_file(hMpq, "data\\global\\excel\\treasureclassex.bin", tc_data);
        if (!out_dir.empty()) {
            std::ofstream out(out_dir + "/data/global/excel/treasureclassex.bin", std::ios::binary);
            out.write(tc_data.data(), tc_data.size());
        }
        std::cout << "  [3/4] treasureclassex.bin: " << rune_mods << " rune drop entries multiplied by " << cfg.rune_multiplier << "x" << std::endl;
    }

    // 4. CharStats
    auto cs_data = read_mpq_file(hMpq, "data\\global\\excel\\charstats.bin");
    if (!cs_data.empty()) {
        uint32_t cs_classes = 0;
        std::memcpy(&cs_classes, cs_data.data(), 4);
        size_t row_len = 196;
        for (uint32_t c = 0; c < cs_classes; c++) {
            size_t offset = 4 + c * row_len;
            // search for b"box " in this record
            auto it = std::search(cs_data.begin() + offset, cs_data.begin() + offset + row_len, "box ", "box " + 4);
            if (it != cs_data.begin() + offset + row_len) {
                size_t box_pos = std::distance(cs_data.begin(), it);
                size_t item_start = box_pos - 8;
                size_t slot9_pos = item_start + 8 * 8; // slot 9
                std::memcpy(&cs_data[slot9_pos], "gld ", 4);
                uint16_t loc = 0;
                uint16_t count = (uint16_t)std::min(10000u, cfg.start_gold);
                std::memcpy(&cs_data[slot9_pos + 4], &loc, 2);
                std::memcpy(&cs_data[slot9_pos + 6], &count, 2);
            }
        }
        write_mpq_file(hMpq, "data\\global\\excel\\charstats.bin", cs_data);
        if (!out_dir.empty()) {
            std::ofstream out(out_dir + "/data/global/excel/charstats.bin", std::ios::binary);
            out.write(cs_data.data(), cs_data.size());
        }
        std::cout << "  [4/4] charstats.bin: Starting gold set to " << cfg.start_gold << " for all 7 classes" << std::endl;
    }

    SFileCloseArchive(hMpq);
    std::cout << ">>> MPQ updated successfully!" << std::endl;
    return true;
}

int main(int argc, char* argv[]) {
    std::string ini_path = "sd-card/switch/wine/drive_c/DiabloII/d2nx/d2nxpad.ini";
    if (argc > 1 && fs::exists(argv[1])) ini_path = argv[1];

    GameplayConfig cfg = load_config(ini_path);
    std::cout << "=== Diablo II / Median XL Gameplay Tables Patcher ===" << std::endl;
    std::cout << "Config Loaded from: " << ini_path << std::endl;
    std::cout << "  XP Multiplier:     " << cfg.xp_multiplier << "x" << std::endl;
    std::cout << "  Rune Multiplier:   " << cfg.rune_multiplier << "x" << std::endl;
    std::cout << "  Unique Multiplier: " << cfg.unique_multiplier << "x" << std::endl;
    std::cout << "  Rare Multiplier:   " << cfg.rare_multiplier << "x" << std::endl;
    std::cout << "  Starting Gold lv1: " << cfg.start_gold << std::endl;

    std::vector<std::string> archives = {
        "build/test_game/medianxl-data.mpq",
        "sd-card/switch/wine/drive_c/DiabloII/medianxl-data.mpq",
        "dist/sd-card/switch/wine/drive_c/DiabloII/medianxl-data.mpq",
        "build/ryujinx-test/sdcard/switch/wine/drive_c/DiabloII/medianxl-data.mpq"
    };

    for (const auto& arc : archives) {
        if (fs::exists(arc)) {
            std::string d2_dir = fs::path(arc).parent_path().string();
            patch_archive(arc, cfg, d2_dir);
        }
    }

    return 0;
}
