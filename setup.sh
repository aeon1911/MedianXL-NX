#!/bin/sh
# Monta o cartao SD do Median XL para Switch a partir da SUA copia do Diablo II: Lord of Destruction.
#
#   ./setup.sh "/caminho/do/Diablo II"          -> dist/sd-card/  (copie a pasta switch/ para a raiz do SD)
#   ./setup.sh "/caminho/do/Diablo II" --zip    -> tambem dist/d2-medianxl-autorun-switch-2.14.6-final.zip
#
# O que ele faz (detalhes no INSTALL.md):
#   (passos em tools/build_all.sh)
#   1. compila D2Autorun.dll e D2NXPad.dll (src-d2nx/build.sh, mingw 32-bit);
#   2. baixa o Median XL 2.14.6 do canal oficial do launcher e confere os MD5 (tools/fetch_medianxl.py);
#   3. compila StormLib + merge_mpqs + mpq_extract (tools/build_tools.sh);
#   4. monta build/test_game com os MPQs base da sua instalacao + Median XL + correcoes (tools/prepare_game.py);
#   5. monta dist/sd-card (tools/assemble_sd_tree.py) e, com --zip, o pacote (tools/package_release.py).
# Precisa de python3 e de: i686-w64-mingw32-gcc, g++, cmake, make, git, zlib e bzip2 (headers).
# Se faltar algum, usa o Docker com tools/Dockerfile (nada e instalado no sistema).
# Nenhum arquivo da Blizzard ou do Median XL fica no repositorio: tudo vai para build/ e dist/ (fora do git).
set -e
cd "$(dirname "$0")"
D2="$1"
ZIPARG=""
[ "$2" = "--zip" ] && ZIPARG=--zip
if [ -z "$D2" ] || [ ! -d "$D2" ]; then
    sed -n '2,17p' "$0" | sed 's/^# \{0,1\}//'
    echo
    echo "ERRO: informe a pasta da sua instalacao do Diablo II LoD (a que tem d2data.mpq e d2exp.mpq)."
    exit 1
fi
D2="$(cd "$D2" && pwd)"

have_local() {
    for t in python3 i686-w64-mingw32-gcc g++ cmake make git; do command -v $t >/dev/null || return 1; done
    printf '#include <zlib.h>\n#include <bzlib.h>\nint main(void){return 0;}\n' | g++ -x c++ - -o /dev/null -lz -lbz2 2>/dev/null
}

if have_local; then
    echo "== usando as ferramentas desta maquina"
    sh tools/build_all.sh "$D2" $ZIPARG
else
    command -v docker >/dev/null || { echo "ERRO: faltam ferramentas de build e o Docker nao esta instalado (veja INSTALL.md)"; exit 1; }
    echo "== faltam ferramentas locais: usando o Docker (imagem medianxl-nx-builder, tools/Dockerfile)"
    docker build -q -t medianxl-nx-builder -f tools/Dockerfile tools >/dev/null
    docker run --rm -u "$(id -u):$(id -g)" -e HOME=/tmp \
        -v "$PWD":/w -v "$D2":/d2:ro -w /w medianxl-nx-builder sh tools/build_all.sh /d2 $ZIPARG
fi
echo
echo "PRONTO: copie a pasta dist/sd-card/switch para a raiz do cartao SD (junte com a switch/ que ja existe)."
echo "No console: abra o hbmenu por Title Override (segure R ao abrir um jogo) e inicie o d2-autorun."
