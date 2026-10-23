# Ambiente de build completo (usado por setup.sh quando a maquina nao tem as ferramentas):
#   - i686-w64-mingw32-gcc: D2Autorun.dll e D2NXPad.dll (src-d2nx/build.sh)
#   - g++/cmake/zlib/bzip2: StormLib + tools/merge_mpqs + tools/mpq_extract
#   - python3: scripts de montagem
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends \
        gcc-mingw-w64-i686 g++ make cmake git ca-certificates zlib1g-dev libbz2-dev python3 \
    && rm -rf /var/lib/apt/lists/*
WORKDIR /w
