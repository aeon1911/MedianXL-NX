#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include "StormLib.h"

namespace fs = std::filesystem;

int main(int argc, char* argv[]) {
    std::string d2_dir = "sd-card/switch/wine/drive_c/DiabloII";
    std::string out_mpq = "sd-card/switch/wine/drive_c/DiabloII/medianxl-data.mpq";
    if (argc > 1) d2_dir = argv[1];
    if (argc > 2) out_mpq = argv[2];

    std::vector<std::string> mpqs;
    for (const auto& entry : fs::directory_iterator(d2_dir)) {
        std::string name = entry.path().filename().string();
        if (name.rfind("medianxl-", 0) == 0 && name.rfind(".mpq") == name.length() - 4) {
            if (entry.path() != fs::path(out_mpq)) {
                mpqs.push_back(entry.path().string());
            }
        }
    }
    std::sort(mpqs.begin(), mpqs.end());
    std::cout << "Found " << mpqs.size() << " source MPQ archives to merge into: " << out_mpq << std::endl;

    if (mpqs.empty()) {
        std::cerr << "No MPQs found!" << std::endl;
        return 1;
    }

    if (fs::exists(out_mpq)) {
        fs::remove(out_mpq);
    }

    HANDLE hDstMpq = NULL;
    SFILE_CREATE_MPQ createInfo = {};
    createInfo.cbSize = sizeof(SFILE_CREATE_MPQ);
    createInfo.dwMpqVersion = MPQ_FORMAT_VERSION_1;
    createInfo.dwStreamFlags = STREAM_PROVIDER_FLAT | BASE_PROVIDER_FILE;
    createInfo.dwFileFlags1 = MPQ_FILE_DEFAULT_INTERNAL; // (listfile)
    createInfo.dwFileFlags2 = 0;                          // no (attributes)
    createInfo.dwFileFlags3 = 0;                          // no (signature)
    createInfo.dwAttrFlags = 0;                           // disable (attributes) completely for D2 1.13c compat
    createInfo.dwSectorSize = 4096;
    createInfo.dwMaxFileCount = 16384;

    if (!SFileCreateArchive2(out_mpq.c_str(), &createInfo, &hDstMpq)) {
        std::cerr << "Failed to create " << out_mpq << " (error " << SErrGetLastError() << ")" << std::endl;
        return 1;
    }

    auto start_time = std::chrono::steady_clock::now();
    int files_added = 0;
    int archives_processed = 0;
    std::vector<char> buffer;

    for (const auto& mpq_path : mpqs) {
        HANDLE hSrcMpq = NULL;
        if (!SFileOpenArchive(mpq_path.c_str(), 0, STREAM_FLAG_READ_ONLY, &hSrcMpq)) {
            std::cerr << "Warning: Failed to open source " << mpq_path << std::endl;
            continue;
        }

        SFILE_FIND_DATA findData;
        HANDLE hFind = SFileFindFirstFile(hSrcMpq, "*", &findData, NULL);
        if (hFind) {
            do {
                std::string fname = findData.cFileName;
                if (fname == "(listfile)" || fname == "(attributes)" || fname == "(signature)") {
                    continue;
                }

                HANDLE hSrcFile = NULL;
                if (!SFileOpenFileEx(hSrcMpq, fname.c_str(), SFILE_OPEN_FROM_MPQ, &hSrcFile)) {
                    std::cerr << "Warning: Failed to open file " << fname << " in " << mpq_path << std::endl;
                    continue;
                }

                DWORD fileSize = SFileGetFileSize(hSrcFile, NULL);
                if (buffer.size() < fileSize) {
                    buffer.resize(fileSize);
                }

                DWORD bytesRead = 0;
                if (fileSize > 0) {
                    if (!SFileReadFile(hSrcFile, buffer.data(), fileSize, &bytesRead, NULL) || bytesRead != fileSize) {
                        std::cerr << "Warning: Failed to read file " << fname << " (skipping corrupt/unreadable)" << std::endl;
                        SFileCloseFile(hSrcFile);
                        continue;
                    }
                }
                SFileCloseFile(hSrcFile);

                HANDLE hDstFile = NULL;
                DWORD writeFlags = (findData.dwFileFlags & (MPQ_FILE_IMPLODE | MPQ_FILE_COMPRESS)) | MPQ_FILE_REPLACEEXISTING;
                if (writeFlags == MPQ_FILE_REPLACEEXISTING) {
                    writeFlags |= MPQ_FILE_IMPLODE;
                }
                DWORD comp = (writeFlags & MPQ_FILE_IMPLODE) ? MPQ_COMPRESSION_PKWARE : MPQ_COMPRESSION_ZLIB;

                if (!SFileCreateFile(hDstMpq, fname.c_str(), 0, fileSize, 0, writeFlags, &hDstFile)) {
                    std::cerr << "Warning: Failed to create file " << fname << " in dst MPQ" << std::endl;
                    continue;
                }

                if (fileSize > 0) {
                    if (!SFileWriteFile(hDstFile, buffer.data(), fileSize, comp)) {
                        std::cerr << "Warning: Failed to write file " << fname << " in dst MPQ" << std::endl;
                    }
                }

                SFileFinishFile(hDstFile);
                files_added++;

                if (files_added % 1000 == 0) {
                    std::cout << "  Progress: " << files_added << " files added..." << std::endl;
                }
            } while (SFileFindNextFile(hFind, &findData));
            SFileFindClose(hFind);
        }
        SFileCloseArchive(hSrcMpq);
        archives_processed++;
    }

    std::cout << "Compacting archive..." << std::endl;
    SFileCompactArchive(hDstMpq, NULL, false);
    SFileCloseArchive(hDstMpq);

    auto end_time = std::chrono::steady_clock::now();
    double sec = std::chrono::duration<double>(end_time - start_time).count();
    auto out_size = fs::file_size(out_mpq);

    std::cout << "\n=== MERGE COMPLETE ===" << std::endl;
    std::cout << "Processed " << archives_processed << " archives." << std::endl;
    std::cout << "Added " << files_added << " files into " << out_mpq << std::endl;
    std::cout << "Final MPQ size: " << out_size / (1024 * 1024.0) << " MB" << std::endl;
    std::cout << "Elapsed time: " << sec << " seconds." << std::endl;

    return 0;
}
