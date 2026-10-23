#!/bin/sh
# Compila as ferramentas do PC usadas na montagem: StormLib (baixada do GitHub do autor, versao fixa),
# build/bin/merge_mpqs e build/bin/mpq_extract. Precisa de g++, cmake, make, git, zlib e bzip2
# (ou rode pelo setup.sh, que usa o container de tools/Dockerfile quando faltar algo).
set -e
cd "$(dirname "$0")/.."
STORMLIB_REPO=https://github.com/ladislav-zezula/StormLib.git
STORMLIB_REV=6bb1882bd00ddbc3729cac5dac0fda81a61e5514   # versao testada
SL=build/stormlib
mkdir -p build/bin
if [ ! -f "$SL/src/StormLib.h" ]; then
    rm -rf "$SL"
    git clone --quiet "$STORMLIB_REPO" "$SL"
    git -C "$SL" checkout --quiet "$STORMLIB_REV"
fi
if [ ! -f "$SL/build/libstorm.a" ]; then
    cmake -S "$SL" -B "$SL/build" -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF >/dev/null
    cmake --build "$SL/build" -j"$(nproc 2>/dev/null || echo 4)" >/dev/null
fi
for t in merge_mpqs mpq_extract; do
    g++ -O2 -std=c++17 -I"$SL/src" "tools/$t.cpp" "$SL/build/libstorm.a" -lz -lbz2 -o "build/bin/$t"
done
echo "ferramentas prontas em build/bin (merge_mpqs, mpq_extract)"
